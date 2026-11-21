// TimelineScrubber (facet/widgets/media.h): a time ruler from 0 to the duration with a playhead and,
// in the second form, two loop markers. Pressing the ruler moves the playhead there and drags it
// (the value drag block, src/internal/value_drag.h); the playhead and each marker are Tab stops that
// the arrows step and Home and End send to their ends. The wheel zooms around the pointer and
// Shift+wheel pans; the view is kept under the id.

#include "facet/widgets/media.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/popup_host.h"
#include "internal/state_storage.h"
#include "internal/value_drag.h"
#include "widgets/media/media_look.h"
#include "widgets/sliders/slider_core.h"

namespace facet
{
namespace
{

using internal::ForcedItemState;

// The default height in control heights (the header promises two).
constexpr float kTimelineRows = 2.0f;
// One wheel notch zooms by this factor; Shift+wheel pans this share of the visible span.
constexpr double kTimelineZoomStep = 1.25;
constexpr double kTimelinePanShare = 0.1;
// The closest zoom shows this share of the duration.
constexpr double kTimelineMinSpanShare = 0.001;
// Arrow steps without options.step: a hundredth of the duration.
constexpr double kTimelineDefaultSteps = 100.0;
// Shift steps a tenth, Ctrl ten steps.
constexpr double kTimelineFineFactor = 0.1;
constexpr double kTimelineCoarseFactor = 10.0;
// Tick intervals in seconds, from the finest; the first one whose labels have room is used.
constexpr double kTimelineIntervals[] = {0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1.0, 2.0, 5.0, 10.0, 15.0, 30.0,
                                         60.0, 120.0, 300.0, 600.0, 900.0, 1800.0, 3600.0, 7200.0, 18000.0, 36000.0};
// A marker's outline darkens this far toward label on hover, and is this many hairlines wide, as
// the slider knob's is.
constexpr float kTimelineMarkerHoverShare = 0.5f;
constexpr float kTimelineMarkerOutlineHairlines = 1.5f;
// Part indices for forced states.
constexpr int kTimelinePartPlayhead = 0;
constexpr int kTimelinePartLoopIn = 1;
constexpr int kTimelinePartLoopOut = 2;

struct MediaTimelineState
{
    double start = 0.0;   // seconds at the left edge of the ruler
    double span = 0.0;    // seconds the ruler shows; 0: the whole duration
    double duration = 0.0;
};

// A time as text: the caller's format or formatter, else minutes and seconds (with hours from an
// hour on, and tenths or hundredths when the ticks are that fine).
void FormatTimelineTime(char* out, int size, double seconds, const TimelineScrubberOptions& o, double resolution,
                        double duration)
{
    if (o.formatter != nullptr)
    {
        o.formatter(out, size, seconds, o.formatterUser);
        return;
    }
    if (o.format != nullptr)
    {
        ImFormatString(out, static_cast<size_t>(size), o.format, seconds);
        return;
    }
    const int decimals = resolution < 0.1 - 1e-9 ? 2 : (resolution < 1.0 - 1e-9 ? 1 : 0);
    const double rounded = std::round(std::max(0.0, seconds) * std::pow(10.0, decimals)) / std::pow(10.0, decimals);
    const long long whole = static_cast<long long>(std::floor(rounded));
    const int hours = static_cast<int>(whole / 3600);
    const int minutes = static_cast<int>((whole / 60) % 60);
    const double secs = rounded - static_cast<double>(whole - whole % 60);
    char secText[16];
    if (decimals > 0)
        ImFormatString(secText, sizeof(secText), "%0*.*f", decimals + 3, decimals, secs);
    else
        ImFormatString(secText, sizeof(secText), "%02d", static_cast<int>(secs));
    if (duration >= 3600.0)
        ImFormatString(out, static_cast<size_t>(size), "%d:%02d:%s", hours, minutes, secText);
    else
        ImFormatString(out, static_cast<size_t>(size), "%d:%s", hours * 60 + minutes, secText);
}

struct TimelineHandle
{
    double* value = nullptr;
    double low = 0.0;    // the value never passes these
    double high = 0.0;
    ImGuiID id = 0;
    int part = 0;
    const char* name = nullptr;
    internal::ItemState item;
};

bool TimelineImpl(const char* label, double* playhead, double* loopIn, double* loopOut, double duration,
                  const TimelineScrubberOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    ImGuiContext& g = *GImGui;
    const internal::NextItemData next = internal::TakeNextItemData();
    const internal::ForcedItemRequest forced = internal::TakeForcedItemState();
    const ImGuiID id = window->GetID(label);
    MediaTimelineState& state = internal::GetState<MediaTimelineState>(id);
    const Theme& t = T();
    const Colors& c = t.colors;
    const Strings& s = GetStrings();
    const bool loop = loopIn != nullptr && loopOut != nullptr;
    const double length = std::isfinite(duration) && duration > 0.0 ? duration : 0.0;
    const bool readOnly = options.readOnly || length <= 0.0 || playhead == nullptr ||
                          (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;

    // Measure: the ruler line on top, the track band under it.
    const float width = std::round(internal::ResolveItemWidth(options.width, next, kFill));
    const float height = std::max(1.0f, std::round(options.height > 0.0f ? Px(options.height)
                                                                          : ControlHeight(ControlSize::Regular) * kTimelineRows));
    const ImVec2 origin(std::round(window->DC.CursorPos.x), std::round(window->DC.CursorPos.y));
    window->DC.CursorPos = origin;
    const ImRect rect(origin, ImVec2(origin.x + width, origin.y + height));
    const internal::SliderMetrics m = internal::GetSliderMetrics(ControlSize::Regular);
    const float pad = m.knob * 0.5f;
    float labelLine = 0.0f;
    {
        const internal::ControlTextScope text(TextStyle::Footnote);
        labelLine = std::ceil(ImGui::GetTextLineHeight());
    }
    const float tick = PxSnapped(t.spacing.xs);
    const float rulerBottom = std::min(rect.Max.y, rect.Min.y + labelLine + tick);
    const ImRect band(rect.Min.x, std::min(rect.Max.y, rulerBottom + PxSnapped(t.spacing.xs)), rect.Max.x, rect.Max.y);
    const float trackY = std::round(band.GetCenter().y);
    const float trackMin = rect.Min.x + pad;
    const float trackMax = std::max(trackMin + 1.0f, rect.Max.x - pad);

    // The view: reset when the duration changes, clamped to it.
    if (state.duration != length)
        state = {0.0, length, length};
    const double minSpan = length * kTimelineMinSpanShare;
    state.span = length > 0.0 ? std::clamp(state.span > 0.0 ? state.span : length, minSpan, length) : 1.0;
    state.start = std::clamp(state.start, 0.0, std::max(0.0, length - state.span));
    const double pxPerSecond = (trackMax - trackMin) / state.span;
    const auto toX = [&](double seconds) { return static_cast<float>(trackMin + (seconds - state.start) * pxPerSecond); };
    // The whole duration as a track for the value drag block, mostly outside the frame when zoomed.
    const ImRect fullTrack(toX(0.0), trackY, toX(std::max(length, state.span)), trackY);

    // The values as drawn: clamped, NaN at 0 (rule 14); written only by an edit.
    const auto clampTime = [&](const double* v) { return v != nullptr && std::isfinite(*v) ? std::clamp(*v, 0.0, length) : 0.0; };
    double values[3] = {clampTime(playhead), clampTime(loopIn), clampTime(loopOut)};
    if (loop && values[1] > values[2])
        values[1] = values[2];
    const double drawnValues[3] = {values[0], values[1], values[2]};

    ImGui::BeginGroup();
    ImGui::ItemSize(rect.GetSize());
    const ImVec2 cursor = window->DC.CursorPos;
    internal::ApplyNextItemData(next);

    // The handles first, so they take the pointer before the ruler under them.
    ImGui::PushOverrideID(id);
    TimelineHandle handles[3];
    handles[0] = {&values[0], 0.0, length, ImGui::GetID("##playhead"), kTimelinePartPlayhead, s.playhead, {}};
    handles[1] = {&values[1], 0.0, values[2], ImGui::GetID("##loopIn"), kTimelinePartLoopIn, s.loopStart, {}};
    handles[2] = {&values[2], values[1], length, ImGui::GetID("##loopOut"), kTimelinePartLoopOut, s.loopEnd, {}};
    const ImGuiID rulerId = ImGui::GetID("##ruler");
    ImGui::PopID();
    const int handleCount = loop ? 3 : 1;
    const float markerWidth = std::max(2.0f, PxSnapped(t.spacing.sm));
    bool written[3] = {false, false, false};
    for (int i = 0; i < handleCount; ++i)
    {
        TimelineHandle& h = handles[i];
        const float x = std::round(toX(*h.value));
        const ImRect hit = i == 0 ? ImRect(x - pad, trackY - pad, x + pad, trackY + pad)
                                  : ImRect(x - markerWidth * 0.5f, band.Min.y, x + markerWidth * 0.5f, band.Max.y);
        internal::ItemOptions o;
        o.press = internal::PressMode::Click;
        o.forced = &forced;
        o.part = h.part;
        o.readOnly = readOnly;
        o.navigable = length > 0.0;
        o.tabStop = length > 0.0;
        h.item = internal::PartBehavior(h.id, hit, o);
        internal::ValueDragOptions drag;
        drag.mapping = {.min = 0.0, .max = length > 0.0 ? length : 1.0};
        drag.grabRadius = -1.0f;
        drag.keyboard = false;
        drag.wheel = false;
        drag.readOnly = readOnly;
        drag.clampLow = h.low;
        drag.clampHigh = h.high;
        const internal::ValueDragResult edit = internal::ValueDragBehavior(h.item, h.value, fullTrack, drag);
        written[i] = edit.changed;
        // The keys: arrows step, Shift finer, Ctrl coarser, Home and End to the ends.
        if (h.item.focused && !readOnly && !h.item.disabled && h.item.forced == ForcedItemState::None)
        {
            const double base = options.step > 0.0 && std::isfinite(options.step) ? options.step : length / kTimelineDefaultSteps;
            const double step = base * (g.IO.KeyShift ? kTimelineFineFactor : (g.IO.KeyCtrl ? kTimelineCoarseFactor : 1.0));
            double target = *h.value;
            if (internal::MediaTakeKey(h.id, ImGuiKey_RightArrow))
                target += step;
            if (internal::MediaTakeKey(h.id, ImGuiKey_LeftArrow))
                target -= step;
            if (internal::MediaTakeKey(h.id, ImGuiKey_Home))
                target = h.low;
            if (internal::MediaTakeKey(h.id, ImGuiKey_End))
                target = h.high;
            target = std::clamp(target, h.low, h.high);
            if (target != *h.value)
            {
                *h.value = target;
                written[i] = true;
                internal::MarkWidgetEdited(h.id);
            }
        }
        // A marker moved by its neighbour's limits stays where it is limited to.
        if (i == 1)
            handles[2].low = values[1];
        if (i == 2)
            handles[1].high = values[2];
    }

    // The ruler and the track: a press moves the playhead under the pointer and drags it.
    internal::ItemOptions rulerOptions;
    rulerOptions.press = internal::PressMode::Click;
    rulerOptions.navigable = false;
    rulerOptions.part = -2;
    rulerOptions.forced = &forced;
    rulerOptions.readOnly = readOnly;
    rulerOptions.handCursor = !readOnly;
    const internal::ItemState ruler = internal::PartBehavior(rulerId, rect, rulerOptions);
    {
        internal::ValueDragOptions drag;
        drag.mapping = {.min = 0.0, .max = length > 0.0 ? length : 1.0};
        drag.keyboard = false;
        drag.wheel = false;
        drag.readOnly = readOnly;
        const internal::ValueDragResult edit = internal::ValueDragBehavior(ruler, &values[0], fullTrack, drag);
        written[0] = written[0] || edit.changed;
        // The ruler takes no focus of its own: a press gives it to the playhead, so the keys go on there.
        if (ruler.activated && length > 0.0 && g.NavId != handles[0].id)
            ImGui::SetFocusID(handles[0].id, window);
    }
    window->DC.CursorPos = cursor;
    const bool disabled = ruler.disabled;

    // The wheel zooms around the pointer, Shift+wheel and a sideways wheel pan, even when read-only.
    if (!disabled && length > 0.0 && ruler.forced == ForcedItemState::None)
    {
        const ImVec2 wheel = internal::MediaClaimWheel(id, rect);
        if (wheel.y != 0.0f)
        {
            const double at = state.start + (g.IO.MousePos.x - trackMin) / pxPerSecond;
            const double span = std::clamp(state.span / std::pow(kTimelineZoomStep, wheel.y), minSpan, length);
            state.start = at - (at - state.start) * span / state.span;
            state.span = span;
        }
        if (wheel.x != 0.0f)
            state.start -= static_cast<double>(wheel.x) * state.span * kTimelinePanShare;
        state.start = std::clamp(state.start, 0.0, std::max(0.0, length - state.span));
    }

    bool any = false;
    double* targets[3] = {playhead, loopIn, loopOut};
    for (int i = 0; i < handleCount; ++i)
    {
        if (written[i] && !readOnly && !disabled && targets[i] != nullptr && values[i] != drawnValues[i])
        {
            *targets[i] = values[i];
            any = true;
        }
    }

    // Draw.
    if (ImGui::IsRectVisible(rect.Min, rect.Max))
    {
        ImDrawList* drawList = window->DrawList;
        drawList->PushClipRect(rect.Min, rect.Max, true);
        const internal::ControlTextScope text(TextStyle::Footnote);
        // Ticks: the finest interval whose labels have room.
        char buffer[48];
        FormatTimelineTime(buffer, sizeof(buffer), length, options, 1.0, length);
        const float labelRoom = ImGui::CalcTextSize(buffer).x + Px(t.spacing.lg);
        double interval = kTimelineIntervals[std::size(kTimelineIntervals) - 1];
        for (const double candidate : kTimelineIntervals)
        {
            FormatTimelineTime(buffer, sizeof(buffer), length, options, candidate, length);
            const float room = ImGui::CalcTextSize(buffer).x + Px(t.spacing.lg);
            if (candidate * pxPerSecond >= std::max(room, labelRoom))
            {
                interval = candidate;
                break;
            }
        }
        const ImU32 ink = ToU32(internal::ControlSecondaryColor(disabled));
        const ImU32 tickColor = ToU32(disabled ? c.labelTertiary : ControlBoundaryColor(c));
        if (length > 0.0)
        {
            const long long firstTick = static_cast<long long>(std::floor(state.start / interval));
            const long long lastTick = static_cast<long long>(std::ceil((state.start + state.span) / interval));
            for (long long k = firstTick; k <= lastTick; ++k)
            {
                const double at = static_cast<double>(k) * interval;
                if (at < 0.0 || at > length + 1e-9)
                    continue;
                const float x = std::round(toX(at));
                drawList->AddRectFilled(ImVec2(x, rulerBottom - tick), ImVec2(x + HairlineThickness(), rulerBottom), tickColor);
                FormatTimelineTime(buffer, sizeof(buffer), at, options, interval, length);
                const float w = ImGui::CalcTextSize(buffer).x;
                const float lx = std::clamp(std::round(x - w * 0.5f), rect.Min.x, rect.Max.x - w);
                drawList->AddText(ImVec2(lx, rect.Min.y), ink, buffer);
                // A minor tick halfway, where there is room for it.
                const float half = std::round(toX(at + interval * 0.5f));
                if (interval * 0.5 * pxPerSecond >= Px(t.spacing.sm) && at + interval * 0.5 <= length)
                    drawList->AddRectFilled(ImVec2(half, rulerBottom - std::round(tick * 0.5f)),
                                            ImVec2(half + HairlineThickness(), rulerBottom), tickColor);
            }
        }

        // The track: the whole duration, the buffered part, the loop region, the played part.
        const float trackThickness = PxSnapped(t.metrics.progressHeight);
        const float top = std::round(trackY - trackThickness * 0.5f);
        const float radius = trackThickness * 0.5f;
        const float x0 = std::max(rect.Min.x, toX(0.0));
        const float x1 = std::min(rect.Max.x, toX(length));
        FillRect(drawList, ImVec2(x0, top), ImVec2(std::max(x0, x1), top + trackThickness), ToU32(disabled ? c.fill[3] : c.fill[1]), radius);
        if (!std::isnan(options.buffered) && options.buffered > 0.0 && length > 0.0)
        {
            const float xb = std::min(rect.Max.x, toX(std::min(options.buffered, length)));
            if (xb > x0)
                FillRect(drawList, ImVec2(x0, top), ImVec2(xb, top + trackThickness), ToU32(disabled ? c.fill[2] : c.labelTertiary), radius);
        }
        if (loop && length > 0.0)
        {
            const float a = std::round(toX(values[1]));
            const float b = std::round(toX(values[2]));
            if (b > a)
                FillRect(drawList, ImVec2(a, band.Min.y), ImVec2(b, band.Max.y), ToU32(disabled ? c.fill[3] : c.accentSubtle),
                         Px(t.radii.element));
        }
        const float xp = std::round(toX(values[0]));
        if (xp > x0)
            FillRect(drawList, ImVec2(x0, top), ImVec2(std::min(xp, x1), top + trackThickness),
                     ToU32(disabled ? c.labelTertiary : (readOnly ? c.labelSecondary : c.accent)), radius);

        // The loop markers: narrow knobs over the band, in the look of a slider knob.
        const internal::SliderLook look = internal::PickSliderLook(disabled, readOnly);
        for (int i = 1; loop && i < 3; ++i)
        {
            const internal::ItemState& h = handles[i].item;
            const float x = std::round(toX(values[i]) - markerWidth * 0.5f);
            const ImRect r(x, band.Min.y, x + markerWidth, band.Max.y);
            const ImVec4 outline = disabled ? c.labelTertiary : Mix(ControlBoundaryColor(c), c.label, h.hoverAmount * kTimelineMarkerHoverShare);
            const float rr = std::min(Px(t.radii.element), markerWidth * 0.5f);
            FillRect(drawList, r.Min, r.Max, ToU32(c.bg), rr);
            StrokeRect(drawList, r.Min, r.Max, ToU32(outline), rr, HairlineThickness() * kTimelineMarkerOutlineHairlines);
            internal::DrawFocusRing(drawList, r, rr, h.focusAmount);
        }
        // The playhead: a line through ruler and band, and a knob on the track.
        const float line = std::max(1.0f, PxSnapped(t.media.playheadWidth));
        drawList->AddRectFilled(ImVec2(std::round(xp - line * 0.5f), rect.Min.y + labelLine),
                                ImVec2(std::round(xp - line * 0.5f) + line, rect.Max.y),
                                ToU32(disabled ? c.labelTertiary : c.label));
        const ImRect knob(xp - pad, trackY - pad, xp + pad, trackY + pad);
        internal::DrawSliderKnob(drawList, knob, handles[0].item, look);
        drawList->PopClipRect();
    }
    ImGui::EndGroup();
    internal::FinishWidgetGroup(id);

    // Tooltips: the label over the ruler, the handle's name and time on a focused or hovered handle.
    const internal::MediaText alt = internal::MediaVisibleText(label);
    bool handleTip = false;
    for (int i = 0; i < handleCount; ++i)
    {
        const internal::ItemState& h = handles[i].item;
        if (internal::BeginPopupTooltip(handles[i].id, h.rect, h.hovered && !h.active, h.focusVisible))
        {
            char time[48];
            FormatTimelineTime(time, sizeof(time), values[i], options, 0.1, length);
            if (!alt.IsEmpty())
                Text(TextStyle::Footnote, "%s", alt.text);
            Text(TextStyle::Footnote, "%s %s", handles[i].name, time);
            internal::EndPopupTooltip();
            handleTip = true;
        }
    }
    if (!handleTip && !alt.IsEmpty() && internal::BeginPopupTooltip(id, rect, ruler.hovered && !ruler.active, false))
    {
        Text(TextStyle::Footnote, "%s", alt.text);
        internal::EndPopupTooltip();
    }
    return any;
}

} // namespace

bool TimelineScrubber(const char* label, double* playhead, double duration, const TimelineScrubberOptions& options)
{
    return TimelineImpl(label, playhead, nullptr, nullptr, duration, options);
}

bool TimelineScrubber(const char* label, double* playhead, double* loopIn, double* loopOut, double duration,
                      const TimelineScrubberOptions& options)
{
    return TimelineImpl(label, playhead, loopIn, loopOut, duration, options);
}

} // namespace facet
