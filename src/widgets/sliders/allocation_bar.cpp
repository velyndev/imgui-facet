// AllocationBar: one bar split into segments whose shares always add up to 1. The bar is the slider
// track with a stretch per segment in its data color; the dividers between segments are the
// handles of the multi-handle block (src/internal/multi_handle.h), so they never cross, keep the
// minimum share between them, and clamp or push. The values the block edits are the dividers'
// positions, the running sums of the shares, so every edit keeps the total at 1 by construction.
//
// Under the bar each segment shows its label and its share in percent, centred on the segment; a
// click there, Enter on a divider or Ctrl+click on a divider types a share (the number field of the
// sliders family, slider_core.h).

#include "facet/widgets/sliders.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <span>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/chart_core.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/multi_handle.h"
#include "internal/text_field_number.h"
#include "widgets/sliders/slider_core.h"

namespace facet
{
namespace
{

using internal::kMaxHandles;
constexpr int kAllocationMaxSegments = kMaxHandles + 1;
// The share format when the caller gives none: whole percent.
constexpr const char* kAllocationDefaultFormat = "%.0f%%";
// Percent per share unit: the format and the typed field work in percent.
constexpr double kAllocationPercent = 100.0;

// The constraints of the dividers: the smallest share between two dividers and at both ends.
internal::HandleConstraints AllocationConstraints(double gap, HandleCollision collision)
{
    internal::HandleConstraints c;
    c.min = gap;
    c.max = 1.0 - gap;
    c.minGap = gap;
    c.collision = collision == HandleCollision::Push ? internal::CollisionMode::Push : internal::CollisionMode::Clamp;
    return c;
}

bool AllocationBarCore(const char* label, std::span<float> shares, const char* const* names, int itemCount,
                       const AllocationBarOptions& o)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    ImGuiContext& g = *GImGui;
    const internal::NextItemData next = internal::TakeNextItemData();
    const internal::SliderMetrics m = internal::GetSliderMetrics(o.size);
    internal::ControlTextScope text(m.control);
    const ImGuiID id = window->GetID(label);
    const Theme& t = T();
    const Colors& c = t.colors;
    const char* format = o.format != nullptr ? o.format : kAllocationDefaultFormat;

    const int n = std::min({static_cast<int>(shares.size()), itemCount, kAllocationMaxSegments});
    const int dividers = std::max(0, n - 1);
    // Fewer than two segments: nothing to move.
    const bool readOnly = o.readOnly || n < 2 || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;

    // The shares as drawn: negative and NaN count as 0, the rest normalized; all 0 gives equal shares.
    double drawn[kAllocationMaxSegments] = {};
    double total = 0.0;
    for (int k = 0; k < n; ++k)
    {
        const double s = static_cast<double>(shares[static_cast<size_t>(k)]);
        drawn[k] = std::isfinite(s) && s > 0.0 ? s : 0.0;
        total += drawn[k];
    }
    for (int k = 0; k < n; ++k)
        drawn[k] = total > 0.0 ? drawn[k] / total : 1.0 / n;

    // The smallest share, at most an equal split, on the step grid when there is one.
    double gap = std::isnan(o.minGap) ? 0.0 : std::max(0.0, o.minGap);
    if (o.step > 0.0 && gap > 0.0)
        gap = std::ceil(gap / o.step - 1e-9) * o.step;
    gap = n > 0 ? std::min(gap, 1.0 / n) : 0.0;
    const internal::HandleConstraints constraints = AllocationConstraints(gap, o.collision);
    internal::ValueMapping mapping;
    mapping.min = 0.0;
    mapping.max = 1.0;
    mapping.step = o.step;
    mapping = internal::NormalizeMapping(mapping);

    // The dividers at the running sums, repaired against the rules.
    double before[kMaxHandles] = {};
    double running = 0.0;
    for (int i = 0; i < dividers; ++i)
    {
        running += drawn[i];
        before[i] = running;
    }
    internal::ConstrainHandles(std::span<double>(before, static_cast<size_t>(dividers)), constraints);
    double bounds[kMaxHandles] = {};
    std::copy(before, before + dividers, bounds);

    // Layout: the slider row, then a caption line for the segments.
    const float width = internal::ResolveItemWidth(o.width, next, kFill);
    const internal::SliderFrame frame =
        internal::LayoutSliderFrame(window->DC.CursorPos, label, width, 0.0f, m, Orientation::Horizontal, 0.0f);
    float captionLine = 0.0f;
    {
        internal::ControlTextScope caption(TextStyle::Caption);
        captionLine = std::ceil(ImGui::GetFontSize());
    }
    const float captionGap = PxSnapped(t.spacing.xs);
    ImRect widget = frame.widget;
    widget.Max.y += captionGap + captionLine;
    const ImRect captions(frame.control.Min.x, frame.widget.Max.y + captionGap, frame.control.Max.x, widget.Max.y);

    ImGui::BeginGroup();
    ImGui::ItemSize(widget.GetSize());
    const ImVec2 cursor = window->DC.CursorPos;
    ImGui::PushOverrideID(id);
    ImGuiID handleIds[kMaxHandles] = {};
    for (int i = 0; i < dividers; ++i)
        handleIds[i] = ImGui::GetID(i);
    const ImGuiID trackId = ImGui::GetID("##track");
    ImGui::PopID();

    // The share a segment would have from the dividers.
    const auto shareOf = [&](const double* b, int k) {
        const double from = k == 0 ? 0.0 : b[k - 1];
        const double to = k == n - 1 ? 1.0 : b[k];
        return to - from;
    };
    // Applies a typed share of segment k: the divider after it moves (before it, for the last).
    const auto applyShare = [&](int k, double share) {
        if (std::isnan(share) || dividers == 0)
            return;
        share = std::clamp(share, 0.0, 1.0);
        const std::span<double> b(bounds, static_cast<size_t>(dividers));
        if (k < n - 1)
            internal::MoveHandle(b, k, internal::SnapValue(mapping, (k == 0 ? 0.0 : bounds[k - 1]) + share), constraints);
        else
            internal::MoveHandle(b, dividers - 1, internal::SnapValue(mapping, 1.0 - share), constraints);
    };

    bool disabled = (next.itemFlags & ImGuiItemFlags_Disabled) != 0;
    internal::ItemState handles[kMaxHandles];
    internal::MultiHandleResult r;
    const int typingPart = internal::SliderTypingPart(id);
    const bool typing = typingPart >= 0 && typingPart < n;
    if (typing)
    {
        internal::NumberFieldParams field;
        field.format = format;
        field.formatter = o.formatter;
        field.formatterUser = o.formatterUser;
        field.min = 0.0;
        field.max = kAllocationPercent;
        field.step = 0.0;
        field.size = o.size;
        const ImGuiID returnFocus = dividers > 0 ? handleIds[std::min(typingPart, dividers - 1)] : 0;
        const internal::SliderTypeResult typed = internal::SliderTypeField(
            id, typingPart, frame.control, shareOf(before, typingPart) * kAllocationPercent, field, returnFocus);
        if (typed.changed && !readOnly && !disabled)
            applyShare(typingPart, typed.value / kAllocationPercent);
    }
    else if (dividers > 0)
    {
        internal::MultiHandleOptions options;
        options.constraints = constraints;
        options.drag.mapping = mapping;
        options.drag.readOnly = readOnly;
        options.handleSize = frame.knob;
        options.spanDrag = false;
        ImGui::SetCursorScreenPos(frame.control.Min);
        r = internal::MultiHandleBehavior(id, frame.control, frame.track, std::span<double>(bounds, static_cast<size_t>(dividers)),
                                          std::span<internal::ItemState>(handles, static_cast<size_t>(dividers)), options);
        disabled = disabled || r.track.disabled;
        // The block keeps the dividers inside [0, 1]; the end segments keep the smallest share too.
        internal::ConstrainHandles(std::span<double>(bounds, static_cast<size_t>(dividers)), constraints);
        const bool editable = !readOnly && !disabled;
        int typeHandle = -1;
        if (editable && r.track.pressed && g.IO.KeyCtrl && r.track.forced == internal::ForcedItemState::None)
        {
            // Ctrl+click types the share before the divider the press took, instead of moving it.
            typeHandle = r.activeHandle >= 0 ? r.activeHandle : r.hoveredHandle;
            if (typeHandle >= 0)
            {
                std::copy(before, before + dividers, bounds);
                if (g.ActiveId != 0)
                    ImGui::ClearActiveID();
            }
        }
        for (int i = 0; i < dividers && typeHandle < 0; ++i)
        {
            if (editable && handles[i].keyboardPressed && internal::SliderWantsTyping(handles[i]))
                typeHandle = i;
        }
        if (typeHandle >= 0)
            internal::SliderBeginTyping(id, typeHandle);
    }
    else
    {
        // One segment or none: the track alone, read-only.
        internal::ItemOptions trackOptions;
        trackOptions.press = internal::PressMode::Click;
        trackOptions.navigable = false;
        r.track = internal::PartBehavior(trackId, frame.control, trackOptions);
        disabled = disabled || r.track.disabled;
    }
    window->DC.CursorPos = cursor;

    bool changed = false;
    for (int i = 0; i < dividers; ++i)
        changed = changed || bounds[i] != before[i];
    const bool mixedShown = o.mixed && !changed;

    // ---- Draw and the caption parts ----
    const internal::SliderLook look = internal::PickSliderLook(disabled, readOnly);
    ImDrawList* drawList = window->DrawList;
    double shown[kAllocationMaxSegments] = {};
    for (int k = 0; k < n; ++k)
        shown[k] = mixedShown ? 1.0 / n : shareOf(bounds, k);
    const auto fractionAt = [&](int k) {  // the start of segment k on the track
        double sum = 0.0;
        for (int j = 0; j < k; ++j)
            sum += shown[j];
        return std::clamp(sum, 0.0, 1.0);
    };
    const bool visible = ImGui::IsRectVisible(widget.Min, widget.Max);
    if (!typing && visible)
    {
        internal::DrawSliderTrack(drawList, frame.track, Orientation::Horizontal, m, 0.0, 0.0, look, o.error != nullptr);
        for (int k = 0; k < n; ++k)
        {
            ImU32 color = !o.segmentColors.empty() ? o.segmentColors[static_cast<size_t>(k) % o.segmentColors.size()] : 0;
            if (color == 0)
                color = ToU32(internal::SeriesColor(c, k));
            if (look == internal::SliderLook::Disabled)
                color = ToU32(c.fill[3]);
            internal::DrawSliderTrackStretch(drawList, frame.track, Orientation::Horizontal, m, fractionAt(k),
                                             fractionAt(k) + shown[k], color);
        }
        if (dividers > 0)
        {
            int order[kMaxHandles];
            const int top = r.activeHandle >= 0 ? r.activeHandle : r.focusedHandle;
            internal::HandleDrawOrder(std::span<const double>(bounds, static_cast<size_t>(dividers)), constraints, top,
                                      std::span<int>(order, static_cast<size_t>(dividers)));
            for (int k = 0; k < dividers; ++k)
            {
                const int i = order[k];
                const ImRect knob = internal::SliderKnobRect(frame.track, Orientation::Horizontal, frame.knob,
                                                             mixedShown ? (i + 1.0) / n : bounds[i]);
                if (mixedShown)
                    internal::DrawFocusRing(drawList, knob, knob.GetWidth() * 0.5f, handles[i].focusAmount, 0,
                                            internal::RingShape::Round);
                else
                    internal::DrawSliderKnob(drawList, knob, handles[i], look);
            }
        }
    }

    // Captions: the label and the share of each segment, centred on it, cut to its width; the
    // share alone when the label does not fit. Each is a part that a click types into.
    {
        internal::ControlTextScope caption(TextStyle::Caption);
        const ImU32 color = ToU32(internal::ControlSecondaryColor(disabled));
        const float pad = PxSnapped(t.spacing.xs);
        ImGui::PushOverrideID(id);
        for (int k = 0; k < n && !typing; ++k)
        {
            const float a = k == 0 ? captions.Min.x : internal::SliderTrackPos(frame.track, Orientation::Horizontal, fractionAt(k));
            const float b = k == n - 1 ? captions.Max.x
                                       : internal::SliderTrackPos(frame.track, Orientation::Horizontal, fractionAt(k) + shown[k]);
            const ImRect cell(a, captions.Min.y, std::max(a, b), captions.Max.y);
            char percent[64];
            internal::FormatNumber(percent, sizeof(percent), shown[k] * kAllocationPercent, format, o.formatter,
                                   o.formatterUser, false, false);
            const char* name = names[k] != nullptr ? names[k] : "";
            const char* nameEnd = ImGui::FindRenderedTextEnd(name);
            char full[internal::kFittedTextSize];
            if (nameEnd != name)
                std::snprintf(full, sizeof(full), "%.*s %s", static_cast<int>(nameEnd - name), name, percent);
            else
                std::snprintf(full, sizeof(full), "%s", percent);
            if (mixedShown)
                std::snprintf(full, sizeof(full), "%.*s", static_cast<int>(nameEnd - name), name);
            const float room = std::max(0.0f, cell.GetWidth() - pad);
            internal::FittedText fitted = internal::FitText(full, nullptr, room);
            if (fitted.truncated && !mixedShown)
            {
                // The share matters more than the name: keep it whole when the two do not fit.
                const internal::FittedText alone = internal::FitText(percent, nullptr, room);
                if (!alone.truncated)
                    fitted = alone;
            }
            internal::ItemOptions captionOptions;
            captionOptions.navigable = false;
            captionOptions.hitTarget = false;
            captionOptions.readOnly = readOnly;
            const internal::ItemState part = internal::PartBehavior(ImGui::GetID(k + kMaxHandles), cell, captionOptions);
            if (part.pressed && !readOnly && !part.disabled && part.forced == internal::ForcedItemState::None)
                internal::SliderBeginTyping(id, k);
            if (visible && cell.GetWidth() > 0.0f)
            {
                drawList->PushClipRect(cell.Min, cell.Max, true);
                drawList->AddText(ImVec2(std::round(cell.GetCenter().x - fitted.width * 0.5f), cell.Min.y), color, fitted.text);
                drawList->PopClipRect();
            }
            internal::CutTextTooltip(part, full, nullptr, fitted.truncated);
        }
        ImGui::PopID();
    }
    window->DC.CursorPos = cursor;

    if (frame.label.text != nullptr)
        internal::DrawFrontLabel(drawList, frame.label, frame.labelPos, m.control.size, disabled);
    const char* labelEnd = ImGui::FindRenderedTextEnd(label);
    bool focusVisible = false;
    for (int i = 0; i < dividers; ++i)
        focusVisible = focusVisible || handles[i].focusVisible;
    internal::CutTextTooltip(id, frame.widget, r.track.hovered, focusVisible, label, labelEnd, frame.label.shown.truncated);
    const internal::ControlMessage message = internal::PickControlMessage(o.error, o.helpText);
    if (message.IsSet())
        internal::DrawControlMessage(message, ImVec2(frame.control.Min.x, widget.Max.y), widget.Max.x - frame.control.Min.x,
                                     disabled, id, widget);
    ImGui::EndGroup();
    internal::FinishWidgetGroup(id);

    if (!changed || readOnly || disabled)
        return false;
    // Every share is written, and the last one closes the total at 1 in float.
    double sum = 0.0;
    for (int k = 0; k + 1 < n; ++k)
    {
        const float share = static_cast<float>(shareOf(bounds, k));
        shares[static_cast<size_t>(k)] = share;
        sum += static_cast<double>(share);
    }
    shares[static_cast<size_t>(n - 1)] = static_cast<float>(std::max(0.0, 1.0 - sum));
    return true;
}

} // namespace

bool AllocationBar(const char* label, std::span<float> shares, std::span<const ChoiceItem> items,
                   const AllocationBarOptions& options)
{
    const char* names[kAllocationMaxSegments] = {};
    const int count = std::min(static_cast<int>(items.size()), kAllocationMaxSegments);
    for (int k = 0; k < count; ++k)
        names[k] = items[static_cast<size_t>(k)].label;
    return AllocationBarCore(label, shares, names, count, options);
}

bool AllocationBar(const char* label, std::span<float> shares, std::span<const char* const> items,
                   const AllocationBarOptions& options)
{
    const char* names[kAllocationMaxSegments] = {};
    const int count = std::min(static_cast<int>(items.size()), kAllocationMaxSegments);
    for (int k = 0; k < count; ++k)
        names[k] = items[static_cast<size_t>(k)];
    return AllocationBarCore(label, shares, names, count, options);
}

} // namespace facet
