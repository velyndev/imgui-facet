// ArcRangeSlider: the two-handle range of RangeSlider bent into an arc with the geometry of the
// Knob: the track arc along the sweep (open at the bottom unless it is a full circle), the band
// between the two handles in the accent, both values inside the dial and the label under it.
//
// Handle 0 is *low and handle 1 *high, each its own item and Tab stop under the widget id; the
// band (part 2) drags both. Without wrap the handles follow the multi-handle model
// (src/internal/multi_handle.h): order, minimum gap, clamp or push, and the band stops as a whole
// at the ends. With wrap the range is a circle: the band runs clockwise from *low to *high, through
// max to min when *low is above *high, and the handles still never pass each other: both arcs
// between them keep the minimum gap, and Push turns the other handle along.
//
// A dial item under the parts takes every mouse press: on a handle it grabs it, on the band it
// grabs the band, elsewhere on the ring it moves the nearer handle there. Drags follow the pointer
// angle; the keys are the value drag core's (src/internal/value_drag.h) with all four arrows, as a
// knob has no direction on screen.

#include "facet/widgets/sliders.h"

#include <algorithm>
#include <cmath>
#include <span>
#include <type_traits>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/anim.h"
#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/multi_handle.h"
#include "internal/state_storage.h"
#include "internal/text_field_number.h"
#include "internal/value_drag.h"
#include "widgets/sliders/range_core.h"
#include "widgets/sliders/slider_core.h"

namespace facet
{
namespace
{

constexpr float kArcRangePi = 3.14159265358979f;
// The default dial is four control heights across, twice a knob: two handles and both values,
// three digits each, have to fit in it.
constexpr float kArcRangeDiameterHeights = 4.0f;
// A handle is at most this share of the dial's diameter, so four fit across a small dial.
constexpr float kArcRangeKnobShare = 0.25f;
// The parts: the two handles, then the band.
constexpr int kArcRangeBandPart = 2;
// Animation sub key of the hover amount of a part: the dial item takes the pointer for all of them.
constexpr int kArcRangeHoverSub = 0x0D00;
// The smallest arc that is drawn, as a share of the range: a wrapped band of almost a full turn
// never closes into a ring that looks like an empty one.
constexpr double kArcRangeFullEpsilon = 1e-9;

// One drag of a handle or of the band, by the pointer angle.
struct ArcRangeDrag
{
    ImGuiID part = 0;
    double pressFraction = 0.0;  // the pointer's fraction along the sweep at the press
    double offset = 0.0;         // handle fraction minus pointer fraction at the press
    double start[2] = {0.0, 0.0};
};

ArcRangeDrag& CurrentArcRangeDrag()
{
    static ArcRangeDrag drag;
    return drag;
}

// The values when an edit began, for Escape (key sessions and drags alike).
struct ArcRangeSnapshot
{
    double values[2] = {0.0, 0.0};
    bool set = false;
};

// The dial's angles: `start` at fraction 0 (radians, clockwise on screen from the +x axis) and
// `sweep` across the range.
struct ArcRangeGeometry
{
    ImVec2 center;
    float radius = 0.0f;  // of the track arc
    float start = 0.0f;
    float sweep = 0.0f;
    bool full = false;    // a closed circle
};

ImVec2 ArcRangePoint(const ArcRangeGeometry& a, double fraction)
{
    const float angle = a.start + a.sweep * static_cast<float>(fraction);
    return ImVec2(a.center.x + std::cos(angle) * a.radius, a.center.y + std::sin(angle) * a.radius);
}

// The fraction along the sweep of a screen point: on a full circle the angle from the start; on an
// open arc a point in the gap at the bottom goes to the nearer end.
double ArcRangePointerFraction(const ArcRangeGeometry& a, ImVec2 p)
{
    float along = std::atan2(p.y - a.center.y, p.x - a.center.x) - a.start;
    const float turn = 2.0f * kArcRangePi;
    along -= turn * std::floor(along / turn);
    if (a.full)
        return std::clamp(static_cast<double>(along / turn), 0.0, 1.0);
    if (along <= a.sweep)
        return along / a.sweep;
    return along - a.sweep < (turn - a.sweep) * 0.5f ? 1.0 : 0.0;
}

// The band length from low clockwise to high on a circle of `range`, in [0, range).
double ArcRangeBand(double low, double high, double range)
{
    double d = std::fmod(high - low, range);
    if (d < 0.0)
        d += range;
    return d;
}

// Moves handle `index` of a wrapped pair toward `target` by the shorter way round, keeping both
// arcs between the handles at least `gap`; Push turns the other handle along instead of stopping.
// Returns true when a value changed.
bool ArcRangeMoveWrapped(double values[2], int index, double target, const internal::ValueMapping& m, double gap, bool push)
{
    const double range = m.max - m.min;
    if (!(range > 0.0) || std::isnan(target) || index < 0 || index > 1)
        return false;
    const double before[2] = {values[0], values[1]};
    const double d = ArcRangeBand(values[0], values[1], range);
    const double delta = std::remainder(target - values[index], range);
    const double lo = std::min(gap, range * 0.5);
    const double hi = range - std::max(lo, range * kArcRangeFullEpsilon);
    const double moved = index == 0 ? d - delta : d + delta;
    if (!push)
    {
        const double clamped = std::clamp(moved, lo, std::max(lo, hi));
        const double applied = index == 0 ? d - clamped : clamped - d;
        values[index] = internal::WrapValue(m, values[index] + applied);
    }
    else
    {
        values[index] = internal::WrapValue(m, values[index] + delta);
        if (moved < lo)
            values[1 - index] = internal::WrapValue(m, index == 0 ? values[0] + lo : values[1] - lo);
        else if (moved > hi)
            values[1 - index] = internal::WrapValue(m, index == 0 ? values[0] + hi : values[1] - hi);
    }
    return values[0] != before[0] || values[1] != before[1];
}

// Repairs a wrapped pair: both arcs at least the gap.
void ArcRangeConstrainWrapped(double values[2], const internal::ValueMapping& m, double gap)
{
    const double range = m.max - m.min;
    values[0] = internal::WrapValue(m, std::isnan(values[0]) ? m.min : values[0]);
    values[1] = internal::WrapValue(m, std::isnan(values[1]) ? m.min : values[1]);
    if (!(range > 0.0))
        return;
    const double lo = std::min(gap, range * 0.5);
    const double hi = range - std::max(lo, range * kArcRangeFullEpsilon);
    const double d = ArcRangeBand(values[0], values[1], range);
    if (d < lo)
        values[1] = internal::WrapValue(m, values[0] + lo);
    else if (d > hi)
        values[1] = internal::WrapValue(m, values[0] + hi);
}

template <class Value>
bool ArcRangeSliderImpl(const char* label, Value* low, Value* high, double min, double max, const ArcRangeSliderOptions& o)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    ImGuiContext& g = *GImGui;
    const internal::NextItemData next = internal::TakeNextItemData();
    const internal::SliderMetrics m = internal::GetSliderMetrics(o.size);
    if (min > max)
        std::swap(min, max);
    const ImGuiID id = window->GetID(label);
    const Theme& t = T();
    const Colors& c = t.colors;

    const float sweepDegrees = o.sweep > 0.0f ? std::min(o.sweep, 360.0f) : t.metrics.dialSweep;
    const bool full = sweepDegrees >= 360.0f;
    // Wrap needs a closed circle: on an open arc min and max are apart.
    const bool wrap = o.wrap && full;
    internal::ValueMapping mapping;
    mapping.min = min;
    mapping.max = max;
    mapping.scale = wrap ? ValueScale::Linear : o.scale;
    mapping.step = o.step;
    mapping.wrap = wrap;
    mapping = internal::NormalizeMapping(mapping);
    const double range = mapping.max - mapping.min;
    const bool missing = low == nullptr || high == nullptr;
    const bool readOnly = o.readOnly || missing || !(range > 0.0) || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    internal::RangeTrackParams params;
    params.minGap = o.minGap;
    params.collision = o.collision;
    const internal::HandleConstraints constraints = internal::RangeConstraints(params, mapping);
    const double gap = wrap ? (std::isnan(o.minGap) ? 0.0 : std::clamp(o.minGap, 0.0, range * 0.5)) : 0.0;
    const bool push = o.collision == HandleCollision::Push;

    // What the caller passed, as drawn: repaired, written only for the handles an edit moves.
    const double raw[2] = {low != nullptr ? internal::SliderValueIn(*low) : mapping.min,
                           high != nullptr ? internal::SliderValueIn(*high) : mapping.max};
    double drawn[2] = {std::isnan(raw[0]) ? mapping.min : raw[0], std::isnan(raw[1]) ? mapping.max : raw[1]};
    if (wrap)
        ArcRangeConstrainWrapped(drawn, mapping, gap);
    else
        internal::ConstrainHandles(std::span<double>(drawn, 2), constraints);
    double values[2] = {drawn[0], drawn[1]};
    // Moves handle i to `target` by the rules of this slider.
    const auto moveHandle = [&](double* v, int i, double target) {
        if (wrap)
            return ArcRangeMoveWrapped(v, i, target, mapping, gap, push);
        return internal::MoveHandle(std::span<double>(v, 2), i, target, constraints);
    };

    // Size: the dial, then the value line when the values do not fit inside, then the label line.
    float diameter = std::round(o.diameter > 0.0f ? Px(o.diameter) : m.control.height * kArcRangeDiameterHeights);
    const float available = ImGui::GetContentRegionAvail().x;
    if (available >= 1.0f)
        diameter = std::min(diameter, std::floor(available));
    diameter = std::max(2.0f, diameter);
    // Handles shrink on a small dial, so four of them would still fit across it.
    const float knob = std::min(m.knob, std::max(2.0f, std::floor(diameter * kArcRangeKnobShare * 0.5f) * 2.0f));
    const char* labelEnd = ImGui::FindRenderedTextEnd(label);
    const bool hasLabel = labelEnd != label;
    float caption = 0.0f;
    float valueWidth = 0.0f;
    {
        internal::ControlTextScope footnote(TextStyle::Footnote);
        caption = std::ceil(ImGui::GetFontSize());
        valueWidth = std::max(internal::SliderValueWidth(id, mapping.min, mapping.max, drawn[0], o.format, o.formatter,
                                                         o.formatterUser, false),
                              internal::SliderValueWidth(id, mapping.min, mapping.max, drawn[1], o.format, o.formatter,
                                                         o.formatterUser, false));
    }
    const float columnWidth = internal::RangeValueColumnWidth(valueWidth);
    const float gapLine = PxSnapped(t.spacing.xs);
    const float arcRadius = diameter * 0.5f - knob * 0.5f;
    // Inside when the column fits within the inner edge of the track and its handles.
    const float inner = std::max(0.0f, arcRadius - knob * 0.5f - m.track) * 2.0f;
    const bool innerText = columnWidth <= inner && caption <= inner;
    const float valueLine = innerText ? 0.0f : gapLine + caption;
    const float labelLine = hasLabel ? gapLine + caption : 0.0f;
    const ImVec2 origin(std::round(window->DC.CursorPos.x), std::round(window->DC.CursorPos.y));
    const float widgetWidth = std::max(diameter, innerText ? 0.0f : std::min(columnWidth, std::max(diameter, available)));
    const ImRect dial(origin, ImVec2(origin.x + diameter, origin.y + diameter));
    const ImRect widget(origin, ImVec2(origin.x + widgetWidth, dial.Max.y + valueLine + labelLine));

    ArcRangeGeometry arc;
    arc.center = ImVec2(std::round(dial.GetCenter().x), std::round(dial.GetCenter().y));
    arc.radius = arcRadius;
    arc.sweep = sweepDegrees * kArcRangePi / 180.0f;
    arc.start = kArcRangePi * 0.5f + (2.0f * kArcRangePi - arc.sweep) * 0.5f;
    arc.full = full;
    const internal::ForcedItemRequest forced = internal::TakeForcedItemState();

    ImGui::BeginGroup();
    ImGui::ItemSize(widget.GetSize());
    ImGui::PushOverrideID(id);
    const ImGuiID handleIds[2] = {ImGui::GetID(0), ImGui::GetID(1)};
    const ImGuiID bandId = ImGui::GetID("##band");
    const ImGuiID dialId = ImGui::GetID("##dial");
    ImGui::PopID();

    const double fractions[2] = {internal::ValueToFraction(mapping, values[0]), internal::ValueToFraction(mapping, values[1])};
    // On the band: from low clockwise to high, through the end when it wraps.
    const auto onBand = [&](double f) {
        if (fractions[1] >= fractions[0])
            return f > fractions[0] && f < fractions[1];
        return f > fractions[0] || f < fractions[1];
    };
    ArcRangeSnapshot& snapshot = internal::GetState<ArcRangeSnapshot>(id);
    const auto takeSnapshot = [&]() {
        snapshot.values[0] = drawn[0];
        snapshot.values[1] = drawn[1];
        snapshot.set = true;
    };
    ArcRangeDrag& drag = CurrentArcRangeDrag();
    const bool typing = internal::SliderTypingPart(id) == 0 || internal::SliderTypingPart(id) == 1;
    internal::ItemState handles[2];
    internal::ItemState band;
    internal::ItemState dialItem;
    int activeHandle = -1;
    int focusedHandle = -1;
    int hoveredPart = -1;
    bool disabled = (next.itemFlags & ImGuiItemFlags_Disabled) != 0;

    if (typing)
    {
        const int part = internal::SliderTypingPart(id);
        internal::NumberFieldParams field;
        field.format = o.format;
        field.formatter = o.formatter;
        field.formatterUser = o.formatterUser;
        field.min = mapping.min;
        field.max = mapping.max;
        field.step = 0.0;
        field.size = o.size;
        const float half = std::round(Px(t.metrics.numberFieldWidth) * 0.5f);
        const float top = std::round(arc.center.y - m.control.height * 0.5f);
        const ImRect rect(arc.center.x - half, top, arc.center.x + half, top + m.control.height);
        const internal::SliderTypeResult typed =
            internal::SliderTypeField(id, part, rect, std::isfinite(raw[part]) ? raw[part] : drawn[part], field, handleIds[part]);
        if (typed.changed && !readOnly && !disabled)
        {
            if (wrap)
                moveHandle(values, part, internal::SnapValue(mapping, internal::WrapValue(mapping, typed.value)));
            else
                internal::ApplyTypedHandleValue(values, part, typed.value, mapping, constraints, false);
        }
    }
    else
    {
        // The dial takes every press and hands it to the part it picks.
        internal::ItemOptions dialOptions;
        dialOptions.press = internal::PressMode::Click;
        dialOptions.navigable = false;
        dialOptions.part = -2;
        dialOptions.forced = &forced;
        dialOptions.readOnly = readOnly;
        dialItem = internal::PartBehavior(dialId, dial, dialOptions);
        disabled = disabled || dialItem.disabled;
        const float grab = std::max(knob, Px(t.metrics.minHitTarget)) * 0.5f;
        const ImVec2 mouse = g.IO.MousePos;
        int pick = -1;
        bool pickBand = false;
        const float r = std::sqrt((mouse.x - arc.center.x) * (mouse.x - arc.center.x) +
                                  (mouse.y - arc.center.y) * (mouse.y - arc.center.y));
        const bool onRing = std::fabs(r - arc.radius) <= grab;
        const double pointer = ArcRangePointerFraction(arc, mouse);
        if (dialItem.hovered || dialItem.pressed)
        {
            // The top handle first when both are under the pointer.
            float best = grab;
            for (const int i : {1, 0})
            {
                const ImVec2 p = ArcRangePoint(arc, fractions[i]);
                const float d = std::sqrt((mouse.x - p.x) * (mouse.x - p.x) + (mouse.y - p.y) * (mouse.y - p.y));
                if (d < best)
                {
                    best = d;
                    pick = i;
                }
            }
            if (pick < 0 && onRing && o.dragSpan && onBand(pointer))
                pickBand = true;
            if (pick < 0 && !pickBand && onRing)
            {
                // The nearer handle, the shorter way round on a circle.
                double d0 = std::fabs(pointer - fractions[0]);
                double d1 = std::fabs(pointer - fractions[1]);
                if (full)
                {
                    d0 = std::min(d0, 1.0 - d0);
                    d1 = std::min(d1, 1.0 - d1);
                }
                pick = d1 < d0 ? 1 : 0;
            }
        }
        if (dialItem.pressed && !disabled && !readOnly && dialItem.forced == internal::ForcedItemState::None &&
            (pick >= 0 || pickBand))
        {
            takeSnapshot();
            drag = {};
            drag.part = pickBand ? bandId : handleIds[pick];
            drag.pressFraction = pointer;
            drag.start[0] = drawn[0];
            drag.start[1] = drawn[1];
            const bool onHandle = pick >= 0 && [&] {
                const ImVec2 p = ArcRangePoint(arc, fractions[pick]);
                return std::sqrt((mouse.x - p.x) * (mouse.x - p.x) + (mouse.y - p.y) * (mouse.y - p.y)) < grab;
            }();
            if (pick >= 0 && !onHandle)
            {
                // A press on the ring away from the handles moves the nearer one there first.
                moveHandle(values, pick, internal::FractionToValue(mapping, pointer));
                ImGui::MarkItemEdited(handleIds[pick]);
                drag.start[0] = values[0];
                drag.start[1] = values[1];
            }
            drag.offset = pick >= 0 ? internal::ValueToFraction(mapping, values[pick]) - pointer : 0.0;
            internal::HandOverMousePress(drag.part, window, !pickBand);
        }
        const bool partActive = g.ActiveId == bandId || g.ActiveId == handleIds[0] || g.ActiveId == handleIds[1];
        if (dialItem.hovered && !partActive)
            hoveredPart = pickBand ? kArcRangeBandPart : pick;

        // The handles: keys through the value drag core, drags by the pointer angle.
        for (int i = 0; i < 2; ++i)
        {
            internal::ItemOptions handleOptions;
            handleOptions.press = internal::PressMode::Click;
            handleOptions.hitTarget = false;
            handleOptions.part = i;
            handleOptions.forced = &forced;
            handleOptions.readOnly = readOnly;
            const ImVec2 p = ArcRangePoint(arc, internal::ValueToFraction(mapping, values[i]));
            const ImRect rect(std::round(p.x) - std::round(knob * 0.5f), std::round(p.y) - std::round(knob * 0.5f),
                              std::round(p.x) - std::round(knob * 0.5f) + knob, std::round(p.y) - std::round(knob * 0.5f) + knob);
            const bool wasActive = g.ActiveId == handleIds[i];
            internal::ItemState& handle = handles[i];
            handle = internal::PartBehavior(handleIds[i], rect, handleOptions);
            const bool mouseDrag = g.ActiveId == handleIds[i] && g.ActiveIdSource == ImGuiInputSource_Mouse && handle.held &&
                                   handle.forced == internal::ForcedItemState::None;
            internal::ValueDragOptions keys;
            keys.mapping = mapping;
            keys.orientation = Orientation::Vertical;
            keys.crossArrows = true;
            keys.wheel = false;
            keys.readOnly = readOnly;
            if (!wrap)
                internal::HandleMoveLimits(std::span<const double>(values, 2), i, constraints, &keys.clampLow, &keys.clampHigh);
            internal::ItemState input = handle;
            input.held = false;  // the pointer angle below drives a drag, not a straight track
            double value = values[i];
            const internal::ValueDragResult edit = internal::ValueDragBehavior(input, &value, dial, keys);
            if (!wasActive && g.ActiveId == handleIds[i] && !mouseDrag)
                takeSnapshot();
            if (edit.canceled)
            {
                if (snapshot.set)
                {
                    values[0] = snapshot.values[0];
                    values[1] = snapshot.values[1];
                }
            }
            else if (edit.changed)
            {
                moveHandle(values, i, value);
            }
            if (mouseDrag && !readOnly && drag.part == handleIds[i] && ImGui::IsMousePosValid(&g.IO.MousePos))
            {
                const double now = ArcRangePointerFraction(arc, g.IO.MousePos);
                double target = 0.0;
                if (now == drag.pressFraction)
                {
                    target = drag.start[i];
                }
                else
                {
                    double f = now + drag.offset;
                    f = full && wrap ? f - std::floor(f) : std::clamp(f, 0.0, 1.0);
                    target = internal::FractionToValue(mapping, f);
                }
                if (target != values[i] && moveHandle(values, i, target))
                    ImGui::MarkItemEdited(handleIds[i]);
                ImGui::SetKeyOwner(ImGuiKey_Escape, handleIds[i]);
                if (ImGui::IsKeyPressed(ImGuiKey_Escape, ImGuiInputFlags_None, handleIds[i]))
                {
                    if (snapshot.set)
                    {
                        values[0] = snapshot.values[0];
                        values[1] = snapshot.values[1];
                        ImGui::MarkItemEdited(handleIds[i]);
                    }
                    internal::MarkEditCanceled(handleIds[i]);
                    ImGui::ClearActiveID();
                    drag = {};
                }
            }
            if (g.ActiveId == handleIds[i])
                activeHandle = i;
            if (handle.focused)
                focusedHandle = i;
            handle.hovered = handle.hovered || i == hoveredPart;
            handle.hoverAmount = std::max(handle.hoverAmount, Tween(handleIds[i], kArcRangeHoverSub,
                                                                    i == hoveredPart ? 1.0f : 0.0f, t.motion.durationFast));
        }

        // The band: rotates both handles, keeping their distance.
        internal::ItemOptions bandOptions;
        bandOptions.press = internal::PressMode::Click;
        bandOptions.navigable = false;
        bandOptions.hitTarget = false;
        bandOptions.part = kArcRangeBandPart;
        bandOptions.forced = &forced;
        bandOptions.readOnly = readOnly;
        band = internal::PartBehavior(bandId, dial, bandOptions);
        if (g.ActiveId == bandId && band.held && band.forced == internal::ForcedItemState::None && drag.part == bandId &&
            !readOnly && ImGui::IsMousePosValid(&g.IO.MousePos))
        {
            const double now = ArcRangePointerFraction(arc, g.IO.MousePos);
            double moved = now - drag.pressFraction;
            if (full)
                moved = std::remainder(moved, 1.0);
            double next2[2] = {drag.start[0], drag.start[1]};
            if (moved != 0.0)
            {
                double delta = moved * range;
                if (mapping.step > 0.0)
                    delta = std::round(delta / mapping.step) * mapping.step;
                if (wrap)
                {
                    next2[0] = internal::WrapValue(mapping, next2[0] + delta);
                    next2[1] = internal::WrapValue(mapping, next2[1] + delta);
                }
                else
                {
                    internal::MoveSpan(std::span<double>(next2, 2), 0, 1, delta, constraints);
                }
            }
            if (next2[0] != values[0] || next2[1] != values[1])
            {
                values[0] = next2[0];
                values[1] = next2[1];
                ImGui::MarkItemEdited(bandId);
            }
            ImGui::SetKeyOwner(ImGuiKey_Escape, bandId);
            if (ImGui::IsKeyPressed(ImGuiKey_Escape, ImGuiInputFlags_None, bandId))
            {
                values[0] = drag.start[0];
                values[1] = drag.start[1];
                internal::MarkEditCanceled(bandId);
                ImGui::ClearActiveID();
                drag = {};
            }
        }
        if (drag.part != 0 && g.ActiveId != drag.part)
            drag = {};
        band.hovered = band.hovered || hoveredPart == kArcRangeBandPart;
        band.hoverAmount = std::max(band.hoverAmount, Tween(bandId, kArcRangeHoverSub, hoveredPart == kArcRangeBandPart ? 1.0f : 0.0f,
                                                            t.motion.durationFast));

        // Ctrl+click or Enter on a handle types its value.
        const bool editable = !readOnly && !disabled;
        int typeHandle = -1;
        if (editable && dialItem.pressed && g.IO.KeyCtrl && dialItem.forced == internal::ForcedItemState::None && pick >= 0)
        {
            typeHandle = pick;
            values[0] = drawn[0];
            values[1] = drawn[1];
            if (g.ActiveId != 0)
                ImGui::ClearActiveID();
            drag = {};
        }
        for (int i = 0; i < 2 && typeHandle < 0; ++i)
        {
            if (editable && handles[i].keyboardPressed && internal::SliderWantsTyping(handles[i]))
                typeHandle = i;
        }
        if (typeHandle >= 0)
            internal::SliderBeginTyping(id, typeHandle);
    }

    const bool written[2] = {values[0] != drawn[0], values[1] != drawn[1]};
    const bool changed = written[0] || written[1];
    if (changed && !missing && !readOnly)
    {
        Value* targets[2] = {low, high};
        for (int i = 0; i < 2; ++i)
        {
            if (!written[i])
                continue;
            if constexpr (std::is_integral_v<Value>)
                *targets[i] = static_cast<Value>(std::llround(values[i]));
            else
                *targets[i] = static_cast<Value>(values[i]);
        }
    }

    // ---- Draw ----
    const internal::SliderLook look = internal::PickSliderLook(disabled, readOnly);
    const bool mixedShown = o.mixed && !changed;
    ImDrawList* drawList = window->DrawList;
    const bool error = o.error != nullptr;
    if (!typing && ImGui::IsRectVisible(widget.Min, widget.Max))
    {
        const int segments = std::max(24, static_cast<int>(arc.radius));
        const ImVec4 rest = look == internal::SliderLook::Disabled ? c.fill[3] : (error ? c.destructive : ControlBoundaryColor(c));
        if (full)
            drawList->AddCircle(arc.center, arc.radius, ToU32(rest), segments * 2, m.track);
        else
        {
            drawList->PathArcTo(arc.center, arc.radius, arc.start, arc.start + arc.sweep, segments);
            drawList->PathStroke(ToU32(rest), m.track);
        }
        const double f0 = internal::ValueToFraction(mapping, values[0]);
        const double f1 = internal::ValueToFraction(mapping, values[1]);
        if (!mixedShown)
        {
            // The band in the accent, thicker while a press would grab it or drags it.
            const ImVec4 fill = look == internal::SliderLook::Disabled ? c.labelTertiary
                                : (look == internal::SliderLook::ReadOnly ? c.labelSecondary : c.accent);
            const float bandAmount = o.dragSpan && look == internal::SliderLook::Normal ? std::max(band.hoverAmount, band.pressAmount) : 0.0f;
            const float thickness = std::round(m.track + PxSnapped(t.spacing.xs) * bandAmount);
            const double length = f1 >= f0 ? f1 - f0 : 1.0 - f0 + f1;
            if (length > 0.0)
            {
                const float a = arc.start + arc.sweep * static_cast<float>(f0);
                drawList->PathArcTo(arc.center, arc.radius, a, a + arc.sweep * static_cast<float>(length), segments);
                drawList->PathStroke(ToU32(fill), thickness);
            }
            const int top = activeHandle >= 0 ? activeHandle : (focusedHandle >= 0 ? focusedHandle : 1);
            for (const int i : {1 - top, top})
            {
                const ImVec2 p = ArcRangePoint(arc, i == 0 ? f0 : f1);
                const ImRect rect(std::round(p.x) - std::round(knob * 0.5f), std::round(p.y) - std::round(knob * 0.5f),
                                  std::round(p.x) - std::round(knob * 0.5f) + knob, std::round(p.y) - std::round(knob * 0.5f) + knob);
                internal::DrawSliderKnob(drawList, rect, handles[i], look);
            }
        }
        else
        {
            for (int i = 0; i < 2; ++i)
            {
                const ImVec2 p = ArcRangePoint(arc, i == 0 ? f0 : f1);
                const ImRect rect(p.x - knob * 0.5f, p.y - knob * 0.5f, p.x + knob * 0.5f, p.y + knob * 0.5f);
                internal::DrawFocusRing(drawList, rect, knob * 0.5f, handles[i].focusAmount, 0, internal::RingShape::Round);
            }
        }
        // Both values: inside the dial, or on a line under it.
        internal::ControlTextScope footnote(TextStyle::Footnote);
        if (!mixedShown)
        {
            char lowText[64];
            char highText[64];
            internal::FormatNumber(lowText, sizeof(lowText), values[0], o.format, o.formatter, o.formatterUser, false, false);
            internal::FormatNumber(highText, sizeof(highText), values[1], o.format, o.formatter, o.formatterUser, false, false);
            const float top = innerText ? std::round(arc.center.y - caption * 0.5f) : dial.Max.y + gapLine;
            const float width = std::min(columnWidth, widget.GetWidth());
            const float left = std::round(innerText ? arc.center.x - columnWidth * 0.5f : widget.Min.x + (widget.GetWidth() - width) * 0.5f);
            internal::DrawRangeValueText(drawList, ImRect(left, top, left + width, top + caption), valueWidth, lowText, highText,
                                         disabled);
        }
    }
    bool labelCut = false;
    if (hasLabel)
    {
        internal::ControlTextScope footnote(TextStyle::Footnote);
        const internal::FittedText shown = internal::FitText(label, labelEnd, widget.GetWidth());
        labelCut = shown.truncated;
        drawList->AddText(ImVec2(std::round(widget.GetCenter().x - shown.width * 0.5f), widget.Max.y - caption),
                          ToU32(internal::ControlSecondaryColor(disabled)), shown.text);
    }
    internal::CutTextTooltip(id, widget, dialItem.hovered, handles[0].focusVisible || handles[1].focusVisible, label, labelEnd,
                             labelCut);
    const internal::ControlMessage message = internal::PickControlMessage(o.error, o.helpText);
    if (message.IsSet())
        internal::DrawControlMessage(message, ImVec2(widget.Min.x, widget.Max.y),
                                     std::max(widget.GetWidth(), std::min(std::max(available, 1.0f), Px(t.layout.minRowControl))),
                                     disabled, id, widget);
    ImGui::EndGroup();
    internal::FinishWidgetGroup(id);
    return changed && !missing && !readOnly;
}

} // namespace

void internal::ResetArcRangeSliderDrag()
{
    CurrentArcRangeDrag() = {};
}

bool ArcRangeSlider(const char* label, float* low, float* high, float min, float max, const ArcRangeSliderOptions& options)
{
    return ArcRangeSliderImpl(label, low, high, internal::SliderFloatToDouble(min), internal::SliderFloatToDouble(max), options);
}

} // namespace facet
