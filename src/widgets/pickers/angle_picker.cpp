// AnglePicker: a round dial with a needle and a degrees field beside it, over an angle in radians
// (counter-clockwise from the right).
//
// The dial is one part (item behaviour) on the value drag core, like the Knob of the sliders: the
// keys run through ValueDragBehavior in delta mode over a wrapping range of degrees, so a press or a
// held key is one edit session, the arrows of both axes turn the needle (Right and Up
// counter-clockwise), Page Up and Down by ten steps, and the wheel while focused. Shift turns by
// snapDegrees instead of one degree. The pointer sets the needle itself: a press points it at the
// pointer, a drag follows it (Shift snaps to snapDegrees), and Escape during the drag restores the
// angle from the press. The degrees field is a NumberInput (inputs.h) that writes typed degrees on
// Enter, Tab or focus loss.

#include "facet/widgets/pickers.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "facet/widgets/inputs.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/state_storage.h"
#include "internal/value_drag.h"

namespace facet
{
namespace
{

using internal::ForcedItemState;

// The default dial is two control heights across, like the Knob.
constexpr float kAngleDialHeights = 2.0f;
// The field is left out when it would be narrower than this many control heights.
constexpr float kAngleFieldMinHeights = 1.5f;
// The tick marks at the quarter turns reach this share of the radius in from the rim.
constexpr float kAngleTickShare = 0.18f;
// The hover darkens the dial outline halfway from the boundary color toward label (as the Knob).
constexpr float kAngleHoverShare = 0.5f;
constexpr double kDegreesPerRadian = 180.0 / std::numbers::pi;

struct AnglePickerState
{
    double pressAngle = 0.0;  // radians at the press of a pointer drag
    bool dragging = false;
};

// Degrees in [0, 360), or in (-180, 180] for a signed range.
double AngleNormalize(double degrees, bool signedRange)
{
    if (!std::isfinite(degrees))
        return 0.0;
    double d = std::fmod(degrees, 360.0);
    if (d < 0.0)
        d += 360.0;
    if (signedRange && d > 180.0)
        d -= 360.0;
    if (d == 360.0)
        d = 0.0;
    return d;
}

} // namespace

bool AnglePicker(const char* label, float* radians, const AnglePickerOptions& options)
{
    using namespace internal;
    // 1. Entry.
    ImGuiWindow* window = ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    ImGuiContext& g = *GImGui;
    const NextItemData next = TakeNextItemData();
    const ForcedItemRequest forced = TakeForcedItemState();
    const ControlMetrics m = GetControlMetrics(options.size);
    const Theme& t = T();
    const Colors& c = t.colors;
    const ImGuiID id = window->GetID(label);
    AnglePickerState& state = GetState<AnglePickerState>(id);
    const bool readOnly = options.readOnly || radians == nullptr || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    const bool mixedFlag = options.mixed || (next.itemFlags & ImGuiItemFlags_MixedValue) != 0;
    const double snap = options.snapDegrees > 0.0f && std::isfinite(options.snapDegrees) ? options.snapDegrees : 1.0;
    const double current = AngleNormalize(radians != nullptr ? *radians * kDegreesPerRadian : 0.0, options.signedRange);

    // 2. Measure: label in front, the dial, the field; the field shrinks first in a narrow row.
    float diameter = std::round(options.diameter > 0.0f ? Px(options.diameter) : m.height * kAngleDialHeights);
    diameter = std::max(2.0f, diameter);
    const float gap = PxSnapped(t.spacing.sm);
    const float available = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    const FrontLabelFit fit = CompactFrontLabelFit(options.width, next);
    const FrontLabel natural = MeasureFrontLabel(label, m.size, available * 2.0f, fit);
    const float fieldDefault = options.field ? gap + std::round(Px(t.metrics.numberFieldWidth)) : 0.0f;
    const float width = ResolveItemWidth(options.width, next, std::min(available, natural.width + diameter + fieldDefault));
    const FrontLabel front = MeasureFrontLabel(label, m.size, width, fit);
    diameter = std::max(1.0f, std::min(diameter, width - front.width));
    const float fieldWidth = width - front.width - diameter - gap;
    const bool field = options.field && fieldWidth >= m.height * kAngleFieldMinHeights;
    const float rowHeight = std::max(diameter, m.height);
    const ImVec2 origin(std::round(window->DC.CursorPos.x), std::round(window->DC.CursorPos.y));
    const ImRect dial(origin.x + front.width, origin.y + std::round((rowHeight - diameter) * 0.5f),
                      origin.x + front.width + diameter, origin.y + std::round((rowHeight - diameter) * 0.5f) + diameter);
    const ImVec2 center = dial.GetCenter();

    // 3. The dial part.
    ImGui::BeginGroup();
    ItemOptions itemOptions;
    itemOptions.press = PressMode::Click;
    itemOptions.readOnly = readOnly;
    itemOptions.mixed = mixedFlag;
    itemOptions.part = 0;
    itemOptions.forced = &forced;
    ApplyNextItemData(next);
    const ItemState item = PartBehavior(id, dial, itemOptions);
    const bool live = !item.disabled && item.forced == ForcedItemState::None;
    bool written = false;
    double edited = current;

    // The pointer points the needle; the drag core's own drag is left out.
    const bool pointerDrag = live && g.ActiveId == id && g.ActiveIdSource == ImGuiInputSource_Mouse && item.held;
    if (pointerDrag)
    {
        if (item.activated || !state.dragging)
        {
            state.pressAngle = radians != nullptr ? *radians : 0.0;
            state.dragging = true;
        }
        if (!readOnly && ImGui::IsMousePosValid(&g.IO.MousePos))
        {
            const float dx = g.IO.MousePos.x - center.x;
            const float dy = center.y - g.IO.MousePos.y;
            if (dx * dx + dy * dy > 1.0f)
            {
                double degrees = std::atan2(static_cast<double>(dy), static_cast<double>(dx)) * kDegreesPerRadian;
                degrees = g.IO.KeyShift ? std::round(degrees / snap) * snap : std::round(degrees);
                edited = AngleNormalize(degrees, options.signedRange);
            }
        }
        ImGui::SetKeyOwner(ImGuiKey_Escape, id);
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, ImGuiInputFlags_None, id))
        {
            edited = AngleNormalize(state.pressAngle * kDegreesPerRadian, options.signedRange);
            MarkEditCanceled(id);
            ImGui::ClearActiveID();
            state.dragging = false;
        }
    }
    else
    {
        state.dragging = false;
        ItemState keys = item;
        keys.held = false;
        ValueDragOptions drag;
        drag.mapping.min = options.signedRange ? -180.0 : 0.0;
        drag.mapping.max = options.signedRange ? 180.0 : 360.0;
        drag.mapping.wrap = true;
        drag.speed = 1.0;
        // Shift steps a tenth of keyStep in delta mode: ten snaps make that one snap.
        drag.keyStep = g.IO.KeyShift ? snap / kFineDragFactor : 1.0;
        drag.crossArrows = true;
        drag.grabRadius = -1.0f;
        drag.readOnly = readOnly;
        double value = current;
        const ValueDragResult result = ValueDragBehavior(keys, &value, dial, drag);
        if (result.changed)
            edited = AngleNormalize(value, options.signedRange);
    }
    if (!readOnly && edited != current)
    {
        *radians = static_cast<float>(edited / kDegreesPerRadian);
        written = true;
        internal::MarkWidgetEdited(id);
    }
    const bool mixed = mixedFlag && !written;

    // 4. Draw the dial: face, quarter ticks, needle.
    ImDrawList* drawList = window->DrawList;
    const bool disabled = item.disabled;
    if (item.visible)
    {
        const float radius = diameter * 0.5f;
        const ImVec4 outline = disabled ? c.labelTertiary
                                        : (options.error != nullptr ? c.destructive
                                                                    : Mix(ControlBoundaryColor(c), c.label, item.hoverAmount * kAngleHoverShare));
        drawList->AddCircleFilled(center, radius, ToU32(disabled ? c.fill[3] : c.bg));
        drawList->AddCircle(center, std::max(1.0f, radius - HairlineThickness() * 0.5f), ToU32(outline), 0, HairlineThickness() * 1.5f);
        const ImU32 tick = ToU32(disabled ? c.labelQuaternary : c.labelTertiary);
        for (int q = 0; q < 4; ++q)
        {
            const float a = static_cast<float>(q) * std::numbers::pi_v<float> * 0.5f;
            const ImVec2 dir(std::cos(a), -std::sin(a));
            const float outer = radius - HairlineThickness() * 2.0f;
            const float inner = radius * (1.0f - kAngleTickShare);
            drawList->AddLine(ImVec2(center.x + dir.x * inner, center.y + dir.y * inner),
                              ImVec2(center.x + dir.x * outer, center.y + dir.y * outer), tick, HairlineThickness());
        }
        if (!mixed)
        {
            const ImVec4 ink = disabled ? c.labelTertiary : (readOnly ? c.labelSecondary : c.accent);
            const float a = static_cast<float>(edited / kDegreesPerRadian);
            const ImVec2 dir(std::cos(a), -std::sin(a));
            const float dot = std::max(1.5f, Px(t.charts.markerSize) * 0.5f);
            const float reach = std::max(0.0f, radius - dot - HairlineThickness() * 2.0f);
            const ImVec2 tip(center.x + dir.x * reach, center.y + dir.y * reach);
            drawList->AddLine(center, tip, ToU32(ink), std::max(1.0f, Px(t.charts.lineWidth)));
            drawList->AddCircleFilled(tip, dot, ToU32(ink));
            drawList->AddCircleFilled(center, std::max(1.0f, dot * 0.75f), ToU32(ink));
        }
        DrawFocusRing(drawList, dial, radius, item.focusAmount, 0, RingShape::Round);
        if (front.text != nullptr)
            DrawFrontLabel(drawList, front, ImVec2(origin.x, CenteredTextY(origin.y, rowHeight)), m.size, disabled);
    }
    CutTextTooltip(id, ImRect(origin, ImVec2(origin.x + front.width, origin.y + rowHeight)), item.hovered,
                   item.focusVisible, front.text, front.end, front.shown.truncated);

    // The degrees field beside the dial.
    if (field)
    {
        float degrees = static_cast<float>(AngleNormalize(radians != nullptr ? *radians * kDegreesPerRadian : 0.0, options.signedRange));
        ImGui::SetCursorScreenPos(ImVec2(dial.Max.x + gap, origin.y + std::round((rowHeight - m.height) * 0.5f)));
        ImGui::PushOverrideID(id);
        if (forced.part == 1)
            ForceNextItemState(forced.state);
        NumberInputOptions number;
        number.format = "%.4g";
        number.min = options.signedRange ? -180.0 : 0.0;
        number.max = options.signedRange ? 180.0 : 360.0;
        number.step = 0.0;
        number.unit = "\xC2\xB0";
        number.size = options.size;
        number.width = fieldWidth / Scale();
        number.readOnly = readOnly;
        number.mixed = mixed;
        if (NumberInput("##degrees", &degrees, number) && !readOnly)
        {
            *radians = static_cast<float>(AngleNormalize(degrees, options.signedRange) / kDegreesPerRadian);
            written = true;
        }
        ImGui::PopID();
    }

    // 5. The row as one block, the message under it, the result.
    ImGui::SetCursorScreenPos(origin);
    ImGui::ItemSize(ImVec2(width, rowHeight));
    const ControlMessage message = PickControlMessage(options.error, options.helpText);
    DrawControlMessage(message, ImVec2(dial.Min.x, origin.y + rowHeight), width - front.width, disabled, id, dial);
    ImGui::EndGroup();
    FinishWidgetGroup(id);
    if (written)
        GImGui->LastItemData.StatusFlags |= ImGuiItemStatusFlags_Edited;
    return written;
}

} // namespace facet
