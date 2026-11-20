// The two-handle track of the range widgets (range_core.h).

#include "widgets/sliders/range_core.h"

#include <algorithm>
#include <cmath>
#include <span>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/focus.h"

namespace facet::internal
{
namespace
{

// Stretches of more ticks than this are left unmarked: they would merge into a line.
constexpr int kRangeMaxTicks = 21;

} // namespace

HandleConstraints RangeConstraints(const RangeTrackParams& p, const ValueMapping& mapping)
{
    HandleConstraints c;
    c.min = mapping.min;
    c.max = mapping.max;
    c.minGap = std::isnan(p.minGap) ? 0.0 : std::max(0.0, p.minGap);
    c.collision = p.collision == HandleCollision::Push ? CollisionMode::Push : CollisionMode::Clamp;
    c.mirror = p.mirror;
    c.center = (mapping.min + mapping.max) * 0.5;
    c.fixedSpan = p.fixedSpan;
    return c;
}

bool ApplyTypedHandleValue(double values[2], int index, double typed, const ValueMapping& mapping,
                           const HandleConstraints& constraints, bool allowOutOfRange)
{
    if (std::isnan(typed) || index < 0 || index > 1)
        return false;
    double t = typed;
    if (mapping.integer)
        t = std::round(t);
    HandleConstraints c = constraints;
    if (allowOutOfRange)
    {
        c.min = std::min(c.min, t);
        c.max = std::max(c.max, t);
    }
    else
    {
        t = SnapValue(mapping, t);
    }
    return MoveHandle(std::span<double>(values, 2), index, t, c);
}

float RangeValueColumnWidth(float valueWidth)
{
    const float gap = PxSnapped(T().spacing.xs);
    return valueWidth * 2.0f + PxSnapped(T().spacing.sm) + gap * 2.0f;
}

void DrawRangeValueText(ImDrawList* drawList, const ImRect& column, float valueWidth, const char* low,
                        const char* high, bool disabled)
{
    if (column.GetWidth() <= 0.0f || low == nullptr || high == nullptr || (low[0] == '\0' && high[0] == '\0'))
        return;
    const float gap = PxSnapped(T().spacing.xs);
    const float bar = PxSnapped(T().spacing.sm);
    const ImRect highSlot(column.Max.x - valueWidth, column.Min.y, column.Max.x, column.Max.y);
    const float barMax = highSlot.Min.x - gap;
    const ImRect lowSlot(barMax - bar - gap - valueWidth, column.Min.y, barMax - bar - gap, column.Max.y);
    DrawSliderValueText(drawList, lowSlot, low, disabled);
    DrawSliderValueText(drawList, highSlot, high, disabled);
    // A short bar between the two values reads as "from ... to" without a word to translate.
    const float y = std::round(column.GetCenter().y);
    const float thickness = std::max(1.0f, HairlineThickness());
    drawList->PushClipRect(column.Min, column.Max, true);
    drawList->AddRectFilled(ImVec2(barMax - bar, y), ImVec2(barMax, y + thickness), ToU32(ControlSecondaryColor(disabled)));
    drawList->PopClipRect();
}

RangeTrackResult RangeTrackBehavior(ImGuiID id, const SliderFrame& frame, const SliderMetrics& m, double low,
                                    double high, double min, double max, const RangeTrackParams& p)
{
    RangeTrackResult r;
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    const ImVec2 cursor = window->DC.CursorPos;
    if (min > max)
        std::swap(min, max);

    ValueMapping mapping;
    mapping.min = min;
    mapping.max = max;
    mapping.scale = p.scale;
    mapping.step = p.step;
    mapping.integer = p.integer;
    mapping = NormalizeMapping(mapping);
    r.mapping = mapping;
    const bool readOnly = p.readOnly || !(mapping.max > mapping.min) || (g.CurrentItemFlags & ImGuiItemFlags_ReadOnly) != 0;
    r.disabled = (g.CurrentItemFlags & ImGuiItemFlags_Disabled) != 0;
    r.readOnly = readOnly;
    const HandleConstraints constraints = RangeConstraints(p, mapping);

    // What the caller passed, as drawn: repaired, never written unless edited.
    const double raw[2] = {low, high};
    double drawn[2] = {std::isnan(low) ? mapping.min : low, std::isnan(high) ? mapping.max : high};
    ConstrainHandles(std::span<double>(drawn, 2), constraints);
    double values[2] = {drawn[0], drawn[1]};

    ImGui::PushOverrideID(id);
    r.handleIds[0] = ImGui::GetID(0);
    r.handleIds[1] = ImGui::GetID(1);
    ImGui::PopID();

    const int typingPart = SliderTypingPart(id);
    if (typingPart == 0 || typingPart == 1)
    {
        r.typing = true;
        NumberFieldParams field;
        field.format = p.format;
        field.formatter = p.formatter;
        field.formatterUser = p.formatterUser;
        if (!p.allowOutOfRange)
        {
            field.min = mapping.min;
            field.max = mapping.max;
        }
        field.integer = p.integer;
        field.step = 0.0;
        field.size = p.size;
        const double start = std::isfinite(raw[typingPart]) ? raw[typingPart] : drawn[typingPart];
        ImRect rect = frame.control;
        if (p.orientation == Orientation::Vertical)
        {
            const float half = std::round(Px(T().metrics.numberFieldWidth) * 0.5f);
            const ImVec2 c(std::round(frame.widget.GetCenter().x), std::round(frame.widget.GetCenter().y));
            rect = ImRect(c.x - half, c.y - std::round(m.control.height * 0.5f), c.x + half,
                          c.y - std::round(m.control.height * 0.5f) + m.control.height);
        }
        const SliderTypeResult typed = SliderTypeField(id, typingPart, rect, start, field, r.handleIds[typingPart]);
        if (typed.changed && !readOnly && !r.disabled)
            ApplyTypedHandleValue(values, typingPart, typed.value, mapping, constraints, p.allowOutOfRange);
    }
    else
    {
        MultiHandleOptions options;
        options.constraints = constraints;
        options.drag.mapping = mapping;
        options.drag.orientation = p.orientation;
        options.drag.readOnly = readOnly;
        options.handleSize = frame.knob;
        options.spanDrag = p.dragSpan;
        ImGui::SetCursorScreenPos(frame.control.Min);
        r.multi = MultiHandleBehavior(id, frame.control, frame.track, std::span<double>(values, 2),
                                      std::span<ItemState>(r.handles, 2), options);
        r.disabled = r.disabled || r.multi.track.disabled;
        const bool editable = !readOnly && !r.disabled;
        int typeHandle = -1;
        if (editable && r.multi.track.pressed && g.IO.KeyCtrl && r.multi.track.forced == ForcedItemState::None)
        {
            // Ctrl+click types the value of the handle the press took, instead of moving it.
            typeHandle = r.multi.activeHandle >= 0 ? r.multi.activeHandle : r.multi.hoveredHandle;
            if (typeHandle >= 0)
            {
                values[0] = drawn[0];
                values[1] = drawn[1];
                if (g.ActiveId != 0)
                    ImGui::ClearActiveID();
            }
        }
        for (int i = 0; i < 2 && typeHandle < 0; ++i)
        {
            if (editable && r.handles[i].keyboardPressed && SliderWantsTyping(r.handles[i]))
                typeHandle = i;
        }
        if (typeHandle >= 0)
        {
            SliderBeginTyping(id, typeHandle);
        }
        else if (editable && p.doubleClickReset && r.multi.activeHandle >= 0 && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            // A double click on a handle sends it to its end of the range, as far as the rules let it.
            const int i = r.multi.activeHandle;
            MoveHandle(std::span<double>(values, 2), i, i == 0 ? mapping.min : mapping.max, constraints);
        }
    }
    window->DC.CursorPos = cursor;

    for (int i = 0; i < 2; ++i)
    {
        r.written[i] = values[i] != drawn[i];
        r.values[i] = r.written[i] ? values[i] : (std::isnan(raw[i]) ? drawn[i] : raw[i]);
    }
    r.changed = r.written[0] || r.written[1];
    r.mixedShown = p.mixed && !r.changed;

    if (r.typing || !ImGui::IsRectVisible(frame.widget.Min, frame.widget.Max))
        return r;

    // ---- Draw ----
    ImDrawList* drawList = window->DrawList;
    const SliderLook look = PickSliderLook(r.disabled, readOnly);
    const double f0 = ValueToFraction(mapping, values[0]);
    const double f1 = ValueToFraction(mapping, values[1]);
    if (p.drawTrack)
        DrawSliderTrack(drawList, frame.track, p.orientation, m, f0, r.mixedShown ? f0 : f1, look, p.error);
    // The span thickens while a press would grab it and while it is dragged, so it shows that it
    // moves as one piece.
    const float spanAmount = p.dragSpan ? std::max(r.multi.span.hoverAmount, r.multi.span.pressAmount) : 0.0f;
    if (look == SliderLook::Normal && spanAmount > 0.0f && !r.mixedShown)
    {
        SliderMetrics thick = m;
        thick.track = std::round(m.track + PxSnapped(T().spacing.xs) * spanAmount);
        DrawSliderTrackStretch(drawList, frame.track, p.orientation, thick, f0, f1, ToU32(T().colors.accent));
    }
    if (p.step > 0.0)
    {
        double ticks[kRangeMaxTicks + 1];
        const int count = SliderStepTicks(mapping, ticks, kRangeMaxTicks);
        DrawSliderTicks(drawList, frame.track, p.orientation, m, std::span<const double>(ticks, static_cast<size_t>(count)), look);
    }
    // The handle a press would take, or the one in use, is drawn on top.
    const int top = r.multi.activeHandle >= 0 ? r.multi.activeHandle : r.multi.focusedHandle;
    int order[2] = {0, 1};
    HandleDrawOrder(std::span<const double>(values, 2), constraints, top, std::span<int>(order, 2));
    char text[64];
    for (const int i : order)
    {
        const ImRect knob = SliderKnobRect(frame.track, p.orientation, frame.knob, i == 0 ? f0 : f1);
        if (r.mixedShown)
        {
            DrawFocusRing(drawList, knob, knob.GetWidth() * 0.5f, r.handles[i].focusAmount, 0, RingShape::Round);
            continue;
        }
        DrawSliderKnob(drawList, knob, r.handles[i], look);
        if (p.valueBubble)
        {
            FormatNumber(text, sizeof(text), r.values[i], p.format, p.formatter, p.formatterUser, p.integer, false);
            DrawSliderBubble(drawList, knob, text, p.orientation, SliderBubbleAmount(r.handleIds[i], r.handles[i]));
        }
    }
    return r;
}

} // namespace facet::internal
