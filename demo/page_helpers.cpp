#include "page_helpers.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>

#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "internal/focus.h"
#include "internal/item.h"
#include "internal/multi_handle.h"
#include "internal/selection_pill.h"
#include "internal/text_util.h"
#include "internal/value_drag.h"

namespace facet::demo
{
namespace
{

ImU32 Color(const ImVec4& color)
{
    return facet::ToU32(color);
}

// Prototype controls: the knob of a slider grows by this share while it is held, so a press
// shows on the knob under the pointer.
constexpr float kKnobPressGrowth = 0.15f;
// The selection pill of a segmented control sits this share of spacing.xs inside its segment.
constexpr float kSegmentInsetShare = 0.5f;

} // namespace

void PageHeader(const char* title, const char* summary)
{
    facet::Text(facet::TextStyle::Title1, "%s", title);
    ImGui::Dummy(ImVec2(0.0f, facet::Px(facet::T().spacing.xs)));
    Paragraph(summary);
}

void SectionHeader(const char* title)
{
    const facet::Theme& theme = facet::T();
    ImGui::Dummy(ImVec2(0.0f, facet::Px(theme.spacing.xl)));
    facet::Text(facet::TextStyle::Title3, "%s", title);
    ImGui::Dummy(ImVec2(0.0f, facet::Px(theme.spacing.xs)));
}

void Paragraph(const char* text)
{
    facet::PushTextStyle(facet::TextStyle::Body);
    ImGui::PushStyleColor(ImGuiCol_Text, facet::T().colors.labelSecondary);
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
    facet::PopTextStyle();
}

float ContentRight()
{
    return ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
}

bool SameLineIfFits(float right, float nextWidth, float spacing)
{
    if (spacing < 0.0f)
        spacing = ImGui::GetStyle().ItemSpacing.x;
    if (ImGui::GetItemRectMax().x + spacing + nextWidth > right)
        return false;
    ImGui::SameLine(0.0f, spacing);
    return true;
}

float ButtonWidth(const char* label)
{
    return ImGui::CalcTextSize(label, nullptr, true).x + ImGui::GetStyle().FramePadding.x * 2.0f;
}

float CheckboxWidth(const char* label)
{
    return ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x + ImGui::CalcTextSize(label, nullptr, true).x;
}

float DemoWidth(float units)
{
    const float room = ImGui::GetContentRegionAvail().x / facet::Px(1.0f);
    return std::max(1.0f, std::min(units, room));
}

MixedHeightRow::MixedHeightRow(float lineHeight, float spacing)
    : top_(ImGui::GetCursorScreenPos().y),
      column_(ImGui::GetCursorPosX()),
      lineHeight_(lineHeight),
      spacing_(spacing),
      stack_(facet::AvailableWidthClass() == facet::WidthClass::Compact)
{
}

void MixedHeightRow::Item(float height)
{
    if (count_++ > 0)
    {
        if (stack_)
        {
            ImGui::SetCursorPosX(column_);
            return;  // a line of its own: nothing to center on
        }
        ImGui::SameLine(0.0f, spacing_);
    }
    else if (stack_)
    {
        return;
    }
    // The cursor of a continued line sits at the line's first item, which may itself be moved
    // down; the offset is taken from the top of the row instead.
    const float x = ImGui::GetCursorScreenPos().x;
    ImGui::SetCursorScreenPos(ImVec2(x, top_ + std::max(0.0f, std::round((lineHeight_ - height) * 0.5f))));
}

// The box the last LabeledRow label is centered in (its column by the line height), for
// LastLabeledRowBox.
static ImRect& LastLabelBox()
{
    static ImRect box;
    return box;
}

void LabeledRow(const char* label, float lineHeight)
{
    const facet::Theme& theme = facet::T();
    const facet::LabeledRowLayout row = facet::LayoutLabeledRow(ImGui::GetContentRegionAvail().x);
    const float line = lineHeight > 0.0f ? lineHeight : ImGui::GetFrameHeight();
    const float height = row.stacked ? facet::Px(theme.type[facet::TextStyle::Footnote].size) : line;
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float startX = ImGui::GetCursorPosX();
    ImGui::Dummy(ImVec2(row.stacked ? row.labelWidth : row.controlOffset, height));
    facet::PushTextStyle(facet::TextStyle::Footnote);
    const float y = row.stacked ? pos.y : facet::CenteredTextY(pos.y, height);
    LastLabelBox() = ImRect(pos, ImVec2(pos.x + row.labelWidth, pos.y + height));
    // Clipped to the label column, so a long label never runs under the control.
    const ImVec2 clipMax(pos.x + row.labelWidth, pos.y + height);
    ImGui::GetWindowDrawList()->PushClipRect(pos, clipMax, true);
    ImGui::GetWindowDrawList()->AddText(ImVec2(pos.x, y), Color(theme.colors.labelSecondary), label);
    ImGui::GetWindowDrawList()->PopClipRect();
    facet::PopTextStyle();
    if (!row.stacked)
        ImGui::SameLine(startX + row.controlOffset, 0.0f);
}

ImRect LastLabeledRowBox()
{
    return LastLabelBox();
}

void Note(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    ImGui::PushStyleColor(ImGuiCol_Text, facet::T().colors.labelSecondary);
    facet::PushTextStyle(facet::TextStyle::Footnote);
    ImGui::TextWrappedV(fmt, args);
    facet::PopTextStyle();
    ImGui::PopStyleColor();
    va_end(args);
}

void ReadyFamilyNote(const char* family)
{
    Note("Every widget of this family works as its entry in docs/widgets/%s.md describes. Declared in "
         "facet/widgets/%s.h; the declarations and usage examples are in docs/widgets/%s.md.",
         family, family, family);
}

void NoteBeside(const char* fmt, ...)
{
    const ImRect item(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    va_list args;
    va_start(args, fmt);
    ImGui::PushStyleColor(ImGuiCol_Text, facet::T().colors.labelSecondary);
    facet::PushTextStyle(facet::TextStyle::Footnote);
    const float y = facet::CenteredTextY(item.Min.y, item.GetHeight());
    ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x, y));
    ImGui::TextWrappedV(fmt, args);
    facet::PopTextStyle();
    ImGui::PopStyleColor();
    va_end(args);
}

// ---- Prototype controls ---------------------------------------------------------------------

void ProbeRect(PageCtx& ctx, const char* name, ImGuiID id, ImVec2 min, ImVec2 max)
{
    if (ctx.probes == nullptr)
        return;
    ImGuiContext& g = *GImGui;
    const ImGuiLastItemData backup = g.LastItemData;
    g.LastItemData.ID = id;
    g.LastItemData.Rect = ImRect(min, max);
    g.LastItemData.NavRect = ImRect(min, max);
    ctx.Probe(name);
    g.LastItemData = backup;
}

void FloatingProbeRect(PageCtx& ctx, const char* name, ImGuiID id, ImVec2 min, ImVec2 max)
{
    if (ctx.probes == nullptr)
        return;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::PushClipRect(viewport->Pos, ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y),
                        false);
    ProbeRect(ctx, name, id, min, max);
    ImGui::PopClipRect();
}

void FloatingProbe(PageCtx& ctx, const char* name, const ImRect& rect)
{
    if (ctx.probes == nullptr)
        return;
    FloatingProbeRect(ctx, name, ImGui::GetID(name), rect.Min, rect.Max);
}

namespace
{

// The fill of a control background from the hover and press amounts.
ImVec4 ControlFill(const facet::internal::ItemState& item)
{
    const facet::Colors& c = facet::T().colors;
    if (item.disabled)
        return c.fill[3];
    const ImVec4 hovered = facet::Mix(c.fill[2], c.fill[1], item.hoverAmount);
    return facet::Mix(hovered, c.fill[0], item.pressAmount);
}

// The line a knob centre travels along inside a slider rectangle: inset by half a knob at
// both ends, so the knob stays inside.
ImRect KnobTrack(const ImRect& rect, facet::Orientation orientation, float knob)
{
    const float half = knob * 0.5f;
    if (orientation == facet::Orientation::Vertical)
    {
        const float x = rect.GetCenter().x;
        return ImRect(x, rect.Min.y + half, x, rect.Max.y - half);
    }
    const float y = rect.GetCenter().y;
    return ImRect(rect.Min.x + half, y, rect.Max.x - half, y);
}

// The slider rectangle at the cursor.
ImRect SliderRect(const ProtoSliderOptions& options)
{
    const facet::Theme& t = facet::T();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float height = facet::ControlHeight(facet::ControlSize::Regular);
    if (options.orientation == facet::Orientation::Vertical)
    {
        const float length = facet::Px(options.length > 0.0f ? options.length : t.spacing.xxxl * 5.0f);
        return ImRect(pos, ImVec2(pos.x + height, pos.y + length));
    }
    // The width precedence of the API conventions: options, SetNextItemWidth, PushItemWidth (a
    // labelled row pushes its slot), then the available width.
    const float length = std::max(height, facet::internal::ResolveItemWidth(options.length, facet::internal::TakeNextItemData(),
                                                                            facet::kFill));
    return ImRect(pos, ImVec2(pos.x + length, pos.y + height));
}

// How a track looks besides its normal state.
enum class TrackLook
{
    Normal,
    ReadOnly,  // the fill in labelSecondary: shown, not editable
    Disabled,  // everything in the disabled tokens
};

// The track line, the filled part from `fillFrom` to `fillTo` (fractions), step ticks and snap
// stops.
void DrawTrack(ImDrawList* drawList, const ImRect& track, facet::Orientation orientation, double fillFrom,
               double fillTo, TrackLook look, const facet::internal::ValueMapping& mapping)
{
    const bool disabled = look == TrackLook::Disabled;
    const facet::Theme& t = facet::T();
    const facet::Colors& c = t.colors;
    const float thickness = facet::PxSnapped(t.metrics.sliderTrackHeight);
    const float half = thickness * 0.5f;
    const bool vertical = orientation == facet::Orientation::Vertical;
    const auto lineRect = [&](float a, float b) {
        if (vertical)
            return ImRect(track.Min.x - half, std::min(a, b), track.Min.x + half, std::max(a, b));
        return ImRect(std::min(a, b), track.Min.y - half, std::max(a, b), track.Min.y + half);
    };
    const float start = vertical ? track.Max.y : track.Min.x;
    const float end = vertical ? track.Min.y : track.Max.x;
    const ImRect whole = lineRect(start - (vertical ? -half : half), end + (vertical ? -half : half));
    // The unfilled line in the 3:1 boundary color, so the ends of the range stay visible.
    facet::FillRect(drawList, whole.Min, whole.Max, Color(disabled ? c.fill[3] : facet::ControlBoundaryColor(c)), half);
    const float a = facet::internal::FractionToTrack(track, orientation, fillFrom);
    const float b = facet::internal::FractionToTrack(track, orientation, fillTo);
    if (a != b)
    {
        const ImRect fill = lineRect(a, b);
        const ImVec4& color = disabled ? c.labelTertiary : (look == TrackLook::ReadOnly ? c.labelSecondary : c.accent);
        facet::FillRect(drawList, fill.Min, fill.Max, Color(color), half);
    }
    // Ticks for a short list of steps and for snap stops, so a slider shows where it can stop.
    const facet::internal::ValueMapping m = facet::internal::NormalizeMapping(mapping);
    const float dot = thickness * 0.5f;
    const ImU32 tick = Color(disabled ? c.labelTertiary : c.labelSecondary);
    const auto drawTick = [&](double value) {
        const float at = facet::internal::FractionToTrack(track, orientation, facet::internal::ValueToFraction(m, value));
        const ImVec2 center = vertical ? ImVec2(track.Min.x + thickness * 2.0f, at) : ImVec2(at, track.Min.y + thickness * 2.0f);
        drawList->AddCircleFilled(center, dot, tick);
    };
    if (m.step > 0.0 && m.max > m.min && (m.max - m.min) / m.step <= 20.0)
    {
        const double count = std::floor((m.max - m.min) / m.step + 1e-9);
        for (int i = 0; i <= static_cast<int>(count); ++i)
            drawTick(m.min + i * m.step);
        // A max off the grid is a stop too (End and drags reach it).
        if (m.min + count * m.step < m.max - m.step * 1e-9)
            drawTick(m.max);
    }
    for (const double stop : m.stops)
    {
        if (stop >= m.min && stop <= m.max)
            drawTick(stop);
    }
}

// A knob: the background color with a 3:1 outline, a little larger while held.
void DrawKnob(ImDrawList* drawList, const ImRect& rect, const facet::internal::ItemState& item, bool disabled)
{
    const facet::Colors& c = facet::T().colors;
    const ImVec2 center = rect.GetCenter();
    const float radius = rect.GetWidth() * 0.5f * (1.0f + kKnobPressGrowth * item.pressAmount);
    const ImVec4 boundary = facet::ControlBoundaryColor(c);
    const ImVec4 outline = disabled ? c.labelTertiary : facet::Mix(boundary, c.label, item.hoverAmount * 0.5f);
    drawList->AddCircleFilled(center, radius, Color(c.bg));
    drawList->AddCircle(center, radius, Color(outline), 0, facet::HairlineThickness() * 1.5f);
    // Round, like the knob, so the ring keeps an even gap all around it.
    facet::internal::DrawFocusRing(drawList, rect, rect.GetWidth() * 0.5f, item.focusAmount, 0,
                                   facet::internal::RingShape::Round);
}

} // namespace

float ProtoButtonWidth(const char* label, bool selected)
{
    facet::internal::PushPillLabelStyle(facet::TextStyle::Body, selected);
    const float width = ImGui::CalcTextSize(label, nullptr, true).x + facet::Px(facet::T().spacing.md) * 2.0f;
    facet::PopTextStyle();
    return width;
}

bool ProtoButton(PageCtx& ctx, const char* label, const char* probe, bool repeat, bool selected)
{
    const facet::Theme& t = facet::T();
    const facet::Colors& c = t.colors;
    const char* end = ImGui::FindRenderedTextEnd(label);
    facet::internal::PushPillLabelStyle(facet::TextStyle::Body, selected);
    const float padding = facet::Px(t.spacing.md);
    const float height = facet::ControlHeight(facet::ControlSize::Regular);
    const ImVec2 size(ImGui::CalcTextSize(label, end).x + padding * 2.0f, height);
    facet::internal::ItemOptions options;
    options.repeat = repeat;
    const facet::internal::ItemState item = facet::internal::ItemBehavior(ImGui::GetID(label), size, options);
    ctx.Probe(probe);
    if (item.visible)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const float radius = facet::Px(t.radii.control);
        if (!selected)
            facet::FillRect(drawList, item.rect.Min, item.rect.Max, Color(ControlFill(item)), radius);
        facet::StrokeRect(drawList, item.rect.Min, item.rect.Max,
                          Color(item.disabled ? c.separator : facet::ControlBoundaryColor(c)), radius,
                          facet::HairlineThickness());
        drawList->AddText(ImVec2(item.rect.Min.x + padding, facet::CenteredTextY(item.rect.Min.y, height)),
                          Color(item.disabled ? c.labelTertiary : c.label), label, end);
        facet::internal::DrawItemFocusRing(item, radius);
    }
    facet::PopTextStyle();
    return item.pressed;
}

bool ProtoToggle(PageCtx& ctx, const char* label, bool* value, const char* probe)
{
    const facet::Theme& t = facet::T();
    const facet::Colors& c = t.colors;
    const char* end = ImGui::FindRenderedTextEnd(label);
    facet::PushTextStyle(facet::TextStyle::Body);
    const float gap = facet::Px(t.spacing.md);
    const float height = facet::ControlHeight(facet::ControlSize::Regular);
    const ImVec2 track(facet::Px(t.metrics.toggleWidth), facet::Px(t.metrics.toggleHeight));
    const float textWidth = ImGui::CalcTextSize(label, end).x;
    const ImVec2 size(textWidth + (textWidth > 0.0f ? gap : 0.0f) + track.x, height);
    const ImGuiID id = ImGui::GetID(label);
    const facet::internal::ItemState item = facet::internal::ItemBehavior(id, size);
    ctx.Probe(probe);
    bool written = false;
    if (item.pressed && value != nullptr && !item.readOnly)
    {
        *value = !*value;
        written = true;
        ImGui::MarkItemEdited(id);
    }
    const bool on = value != nullptr && *value;
    const float knobT = facet::Spring(id, 0, on ? 1.0f : 0.0f, t.motion.quick);
    if (item.visible)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddText(ImVec2(item.rect.Min.x, facet::CenteredTextY(item.rect.Min.y, height)),
                          Color(item.disabled ? c.labelTertiary : c.label), label, end);
        const ImVec2 min(item.rect.Max.x - track.x, item.rect.Min.y + (height - track.y) * 0.5f);
        const ImVec2 max(item.rect.Max.x, min.y + track.y);
        const float radius = track.y * 0.5f;
        const ImVec4 off = item.disabled ? c.fill[3] : ControlFill(item);
        const ImVec4 trackColor = item.disabled ? c.fill[3] : facet::Mix(off, c.accent, knobT);
        facet::FillRect(drawList, min, max, Color(trackColor), radius);
        if (knobT < 1.0f)
            facet::StrokeRect(drawList, min, max,
                              Color(facet::WithAlpha(facet::ControlBoundaryColor(c), 1.0f - knobT)), radius,
                              facet::HairlineThickness());
        const float inset = facet::Px(t.metrics.toggleKnobInset);
        const float knob = track.y - inset * 2.0f;
        const float x = min.x + inset + knob * 0.5f + knobT * (track.x - inset * 2.0f - knob);
        const ImVec4 knobColor = item.disabled ? c.labelTertiary : facet::Mix(c.labelSecondary, c.onAccent, knobT);
        drawList->AddCircleFilled(ImVec2(x, (min.y + max.y) * 0.5f), knob * 0.5f, Color(knobColor));
        facet::internal::DrawFocusRing(drawList, ImRect(min, max), radius, item.focusAmount);
    }
    facet::PopTextStyle();
    return written;
}

bool ProtoSlider(PageCtx& ctx, const char* id, double* value, const ProtoSliderOptions& options, const char* probe)
{
    const facet::Theme& t = facet::T();
    const ImRect rect = SliderRect(options);
    const float knob = facet::Px(t.metrics.sliderKnobSize);
    const ImRect track = KnobTrack(rect, options.orientation, knob);
    facet::internal::ItemOptions itemOptions;
    itemOptions.press = facet::internal::PressMode::Click;
    itemOptions.readOnly = options.readOnly;
    const ImGuiID itemId = ImGui::GetID(id);
    const facet::internal::ItemState item = facet::internal::ItemBehavior(itemId, rect.GetSize(), itemOptions);
    ctx.Probe(probe);
    char name[96];
    std::snprintf(name, sizeof(name), "%s.track", probe);
    const bool vertical = options.orientation == facet::Orientation::Vertical;
    ProbeRect(ctx, name, itemId, vertical ? ImVec2(rect.Min.x, track.Min.y) : ImVec2(track.Min.x, rect.Min.y),
              vertical ? ImVec2(rect.Max.x, track.Max.y) : ImVec2(track.Max.x, rect.Max.y));

    facet::internal::ValueDragOptions drag;
    drag.mapping = {.min = options.min, .max = options.max, .scale = options.scale, .step = options.step,
                    .integer = options.integer, .stops = options.stops};
    drag.orientation = options.orientation;
    drag.grabRadius = knob * 0.5f;
    drag.readOnly = options.readOnly;
    double edited = value != nullptr ? *value : options.min;
    const facet::internal::ValueDragResult result = facet::internal::ValueDragBehavior(item, &edited, track, drag);
    if (result.changed && value != nullptr)
        *value = edited;

    if (item.visible)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const double fraction = facet::internal::ValueToFraction(drag.mapping, edited);
        const TrackLook look = item.disabled ? TrackLook::Disabled
                               : (options.readOnly || item.readOnly) ? TrackLook::ReadOnly
                                                                     : TrackLook::Normal;
        DrawTrack(drawList, track, options.orientation, 0.0, fraction, look, drag.mapping);
        const ImRect knobRect = facet::internal::HandleRect(track, options.orientation, drag.mapping, edited, knob);
        DrawKnob(drawList, knobRect, item, item.disabled);
    }
    return result.changed;
}

bool ProtoRange(PageCtx& ctx, const char* id, double* values, int count, const ProtoRangeOptions& options,
                const char* probe, int* removeRequested)
{
    if (removeRequested != nullptr)
        *removeRequested = -1;
    count = std::clamp(count, 0, facet::internal::kMaxHandles);
    if (values == nullptr || count == 0)
        return false;
    const facet::Theme& t = facet::T();
    const ProtoSliderOptions& slider = options.slider;
    const ImRect rect = SliderRect(slider);
    const float knob = facet::Px(t.metrics.sliderKnobSize);
    const ImRect track = KnobTrack(rect, slider.orientation, knob);
    const ImGuiID widgetId = ImGui::GetID(id);

    facet::internal::MultiHandleOptions multi;
    multi.drag.mapping = {.min = slider.min, .max = slider.max, .scale = slider.scale, .step = slider.step,
                          .integer = slider.integer, .stops = slider.stops};
    multi.drag.orientation = slider.orientation;
    multi.drag.readOnly = slider.readOnly;
    multi.constraints.minGap = options.minGap;
    multi.constraints.collision = options.push ? facet::internal::CollisionMode::Push : facet::internal::CollisionMode::Clamp;
    multi.constraints.mirror = options.mirror;
    multi.constraints.center = (slider.min + slider.max) * 0.5;
    multi.handleSize = knob;
    multi.spanDrag = options.spanDrag;
    multi.removeKeys = options.removeKeys;
    multi.keys = options.keys;

    facet::internal::ItemState handles[facet::internal::kMaxHandles];
    ImGui::BeginGroup();
    const facet::internal::MultiHandleResult result = facet::internal::MultiHandleBehavior(
        widgetId, rect, track, std::span<double>(values, static_cast<size_t>(count)),
        std::span<facet::internal::ItemState>(handles, static_cast<size_t>(count)), multi);
    char name[96];
    const bool vertical = slider.orientation == facet::Orientation::Vertical;
    std::snprintf(name, sizeof(name), "%s.track", probe);
    ProbeRect(ctx, name, result.track.id, vertical ? ImVec2(rect.Min.x, track.Min.y) : ImVec2(track.Min.x, rect.Min.y),
              vertical ? ImVec2(rect.Max.x, track.Max.y) : ImVec2(track.Max.x, rect.Max.y));
    for (int i = 0; i < count; ++i)
    {
        std::snprintf(name, sizeof(name), "%s.h%d", probe, i);
        ProbeRect(ctx, name, handles[i].id, handles[i].rect.Min, handles[i].rect.Max);
    }
    ImGui::EndGroup();
    facet::internal::FinishWidgetGroup(widgetId);
    ctx.Probe(probe);
    if (removeRequested != nullptr)
        *removeRequested = result.removeRequested;

    if (result.track.visible)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const facet::internal::ValueMapping& m = multi.drag.mapping;
        const double first = count > 1 ? facet::internal::ValueToFraction(m, values[0]) : 0.0;
        const double last = facet::internal::ValueToFraction(m, values[count - 1]);
        const TrackLook look = result.track.disabled ? TrackLook::Disabled
                               : (slider.readOnly || result.track.readOnly) ? TrackLook::ReadOnly
                                                                            : TrackLook::Normal;
        DrawTrack(drawList, track, slider.orientation, first, last, look, m);
        // The span thickens while a press would grab it and while it is dragged, so it shows
        // that it moves as one piece.
        const float spanAmount = std::max(result.span.hoverAmount, result.span.pressAmount);
        if (count > 1 && look == TrackLook::Normal && spanAmount > 0.0f)
        {
            const float half = facet::PxSnapped(t.metrics.sliderTrackHeight) * 0.5f + facet::Px(t.spacing.xs) * 0.5f * spanAmount;
            const float a = facet::internal::FractionToTrack(track, slider.orientation, first);
            const float b = facet::internal::FractionToTrack(track, slider.orientation, last);
            const ImRect span = vertical ? ImRect(track.Min.x - half, std::min(a, b), track.Min.x + half, std::max(a, b))
                                         : ImRect(std::min(a, b), track.Min.y - half, std::max(a, b), track.Min.y + half);
            facet::FillRect(drawList, span.Min, span.Max, Color(t.colors.accent), half);
        }
        // The handle a press would take, or the one in use, is drawn on top.
        int top = result.activeHandle >= 0 ? result.activeHandle : result.focusedHandle;
        int order[facet::internal::kMaxHandles];
        facet::internal::HandleConstraints bounds = multi.constraints;
        bounds.min = facet::internal::NormalizeMapping(m).min;
        bounds.max = facet::internal::NormalizeMapping(m).max;
        facet::internal::HandleDrawOrder(std::span<const double>(values, static_cast<size_t>(count)), bounds, top,
                                         std::span<int>(order, static_cast<size_t>(count)));
        for (int k = 0; k < count; ++k)
        {
            const int i = order[k];
            const facet::internal::ItemState& handle = handles[i];
            const ImRect knobRect = facet::internal::HandleRect(track, slider.orientation, m, values[i], knob);
            DrawKnob(drawList, knobRect, handle, result.track.disabled);
        }
    }
    return result.changed;
}

bool ProtoSegmented(PageCtx& ctx, const char* id, int* index, std::span<const char* const> items, const char* probe,
                    float width)
{
    const facet::Theme& t = facet::T();
    const facet::Colors& c = t.colors;
    const int count = static_cast<int>(items.size());
    if (count == 0)
        return false;
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    const ImGuiID widgetId = ImGui::GetID(id);
    facet::PushTextStyle(facet::TextStyle::Body);
    const float padding = facet::Px(t.spacing.md);
    const float height = facet::ControlHeight(facet::ControlSize::Regular);
    float widest = 0.0f;
    for (const char* item : items)
        widest = std::max(widest, ImGui::CalcTextSize(item, ImGui::FindRenderedTextEnd(item)).x);
    const float total = width > 0.0f ? facet::Px(width) : (widest + padding * 2.0f) * static_cast<float>(count);
    const float segment = total / static_cast<float>(count);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImRect rect(pos, ImVec2(pos.x + total, pos.y + height));
    const float radius = facet::Px(t.radii.control);
    const float inset = facet::Px(t.spacing.xs) * kSegmentInsetShare;

    ImGui::BeginGroup();
    ImGui::ItemSize(rect.GetSize());
    ImDrawList* drawList = window->DrawList;
    facet::FillRect(drawList, rect.Min, rect.Max, Color(c.fill[3]), radius);
    facet::StrokeRect(drawList, rect.Min, rect.Max, Color(facet::ControlBoundaryColor(c)), radius,
                      facet::HairlineThickness());

    const facet::internal::ForcedItemRequest forced = facet::internal::TakeForcedItemState();
    facet::internal::SelectionPill pill;
    pill.Begin(widgetId, drawList, ImVec2(window->Pos.x - window->Scroll.x, window->Pos.y - window->Scroll.y));
    const int selected = index != nullptr ? *index : -1;
    int next = selected;
    int focusedSegment = -1;
    char name[96];
    ImGui::PushOverrideID(widgetId);
    for (int i = 0; i < count; ++i)
    {
        const ImRect segmentRect(rect.Min.x + segment * i, rect.Min.y, rect.Min.x + segment * (i + 1), rect.Max.y);
        facet::internal::ItemOptions options;
        options.part = i;
        options.forced = &forced;
        // One Tab stop: the selected segment (the first when none is).
        options.tabStop = i == (selected >= 0 && selected < count ? selected : 0);
        const ImGuiID segmentId = ImGui::GetID(i);
        const facet::internal::ItemState item = facet::internal::PartBehavior(segmentId, segmentRect, options);
        std::snprintf(name, sizeof(name), "%s.%d", probe, i);
        ctx.Probe(name);
        if (item.pressed)
            next = i;
        if (item.focused && g.NavId == segmentId && item.forced == facet::internal::ForcedItemState::None &&
            !item.disabled)
            focusedSegment = i;
        if (i == selected)
            pill.Target(ImRect(segmentRect.Min.x + inset, segmentRect.Min.y + inset, segmentRect.Max.x - inset,
                               segmentRect.Max.y - inset));
        if (item.visible)
        {
            const float feedback = i == selected ? 0.0f : std::max(item.hoverAmount, item.pressAmount);
            if (feedback > 0.0f)
            {
                const ImVec4 fill = facet::Mix(c.fill[2], c.fill[1], item.pressAmount);
                facet::FillRect(drawList, ImVec2(segmentRect.Min.x + inset, segmentRect.Min.y + inset),
                                ImVec2(segmentRect.Max.x - inset, segmentRect.Max.y - inset),
                                Color(facet::WithAlpha(fill, fill.w * feedback)), radius - inset);
            }
            const char* label = items[static_cast<size_t>(i)];
            const char* end = ImGui::FindRenderedTextEnd(label);
            facet::internal::PushPillLabelStyle(facet::TextStyle::Body, i == selected);
            const float textWidth = ImGui::CalcTextSize(label, end).x;
            drawList->AddText(ImVec2(std::round(segmentRect.GetCenter().x - textWidth * 0.5f),
                                     facet::CenteredTextY(segmentRect.Min.y, height)),
                              Color(item.disabled ? c.labelTertiary : c.label), label, end);
            facet::PopTextStyle();
            facet::internal::DrawFocusRing(drawList, segmentRect, radius, item.focusAmount);
        }
    }
    // The arrows move the selection and the focus along the segments, once per press: after
    // all segments ran, so the segment that receives the focus does not see the same press.
    if (focusedSegment >= 0)
    {
        const ImGuiID segmentId = ImGui::GetID(focusedSegment);
        ImGui::SetKeyOwner(ImGuiKey_LeftArrow, segmentId);
        ImGui::SetKeyOwner(ImGuiKey_RightArrow, segmentId);
        int step = 0;
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, ImGuiInputFlags_Repeat, segmentId))
            step = 1;
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, ImGuiInputFlags_Repeat, segmentId))
            step = -1;
        const int target = std::clamp(focusedSegment + step, 0, count - 1);
        if (step != 0 && target != focusedSegment)
        {
            next = target;
            ImGui::SetFocusID(ImGui::GetID(target), window);
            ImGui::SetNavCursorVisible(true);
        }
    }
    ImGui::PopID();
    pill.End(Color(c.accentSubtle), radius - inset);
    ImGui::EndGroup();
    facet::internal::FinishWidgetGroup(widgetId);
    ctx.Probe(probe);
    facet::PopTextStyle();

    const bool changed = index != nullptr && next != selected && next >= 0;
    if (changed)
    {
        *index = next;
        ImGui::MarkItemEdited(widgetId);
    }
    return changed;
}

} // namespace facet::demo
