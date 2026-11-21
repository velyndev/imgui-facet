// JoystickPad: a round (or square) pad with a stick; *value is the stick's deflection in [-1, 1]
// per axis, x to the right and y up. The stick is the slider knob on a line from the centre in the
// accent, so the vector reads at a glance.
//
// Input is the two-axis value drag (src/internal/value_drag.h) on the raw stick position: a press
// on the stick grabs it, a press elsewhere moves it there first; with `circular` it stays inside
// the unit circle. With springReturn the arrows hold the stick toward the edge while they are down
// (PadKeys::Hold), and the end of every drag or key session writes {0, 0}; the stick then springs
// back to the centre through the animation store (frame-rate independent, at once with reduced
// motion). Without springReturn the arrows step the stick like an XYPad and it stays where it is left.
//
// The deadzone: inside `deadzone` of the radius the value is 0; outside, the distance from the
// deadzone's edge is scaled up to the full radius, so the value starts from 0 without a jump.

#include "facet/widgets/sliders.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/anim.h"
#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/state_storage.h"
#include "internal/value_drag.h"
#include "widgets/sliders/pad_core.h"
#include "widgets/sliders/slider_core.h"

namespace facet
{
namespace
{

// The largest deadzone, as a share of the radius: a stick needs some travel outside it.
constexpr double kJoystickMaxDeadzone = 0.95;
// Animation sub keys of the drawn stick position.
constexpr int kJoystickStickSubX = 0x0E00;
constexpr int kJoystickStickSubY = 0x0E01;

// The stick as the widget keeps it between frames.
struct JoystickState
{
    double rawX = 0.0;      // the stick position before the deadzone, [-1, 1] per axis
    double rawY = 0.0;
    ImVec2 reported;        // the value written or read last frame, to see a change by the caller
    bool known = false;
    ImVec2 drawn;           // the drawn stick offset, [-1, 1] per axis (JoystickPadStickOffset)
};

// The value of a raw stick position: 0 inside the deadzone, the rest scaled to the full radius,
// then kept inside the unit circle or square.
ImVec2 JoystickValue(double x, double y, double deadzone, bool circular)
{
    const double r = std::sqrt(x * x + y * y);
    if (!(r > deadzone) || r <= 0.0)
        return ImVec2(0.0f, 0.0f);
    const double scaled = deadzone > 0.0 ? (r - deadzone) / (1.0 - deadzone) : r;
    double vx = x / r * scaled;
    double vy = y / r * scaled;
    if (deadzone <= 0.0)
    {
        vx = x;
        vy = y;
    }
    if (circular)
    {
        const double length = std::sqrt(vx * vx + vy * vy);
        if (length > 1.0)
        {
            vx /= length;
            vy /= length;
        }
    }
    return ImVec2(static_cast<float>(std::clamp(vx, -1.0, 1.0)), static_cast<float>(std::clamp(vy, -1.0, 1.0)));
}

// The raw stick position that gives `value`: the inverse of JoystickValue outside the deadzone.
void JoystickRaw(ImVec2 value, double deadzone, double* x, double* y)
{
    const double vx = std::isfinite(value.x) ? std::clamp(static_cast<double>(value.x), -1.0, 1.0) : 0.0;
    const double vy = std::isfinite(value.y) ? std::clamp(static_cast<double>(value.y), -1.0, 1.0) : 0.0;
    const double length = std::sqrt(vx * vx + vy * vy);
    if (length <= 0.0 || deadzone <= 0.0)
    {
        *x = vx;
        *y = vy;
        return;
    }
    const double r = deadzone + length * (1.0 - deadzone);
    *x = vx / length * r;
    *y = vy / length * r;
}

} // namespace

ImVec2 internal::JoystickPadStickOffset(ImGuiID id)
{
    return GetState<JoystickState>(id).drawn;
}

bool JoystickPad(const char* label, ImVec2* value, const JoystickPadOptions& o)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const internal::NextItemData next = internal::TakeNextItemData();
    const internal::SliderMetrics m = internal::GetSliderMetrics(ControlSize::Inherit);
    const ImGuiID id = window->GetID(label);
    const Theme& t = T();
    const Colors& c = t.colors;
    const bool readOnly = o.readOnly || value == nullptr || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    const double deadzone = std::isfinite(o.deadzone) ? std::clamp(static_cast<double>(o.deadzone), 0.0, kJoystickMaxDeadzone) : 0.0;

    // The size: `diameter`, or the pad token at most the available width.
    float diameter = 0.0f;
    if (o.diameter > 0.0f)
        diameter = std::round(Px(o.diameter));
    else
        diameter = std::min(std::round(Px(t.metrics.padSize)), std::floor(std::max(1.0f, ImGui::GetContentRegionAvail().x)));
    diameter = std::max(2.0f, diameter);
    const internal::PadFrame pad = internal::LayoutPadFrame(window->DC.CursorPos, diameter, diameter, m);

    // The stick follows the caller's value until an edit, so a value set from outside shows.
    JoystickState& state = internal::GetState<JoystickState>(id);
    const ImVec2 current = value != nullptr ? *value : ImVec2(0.0f, 0.0f);
    if (!state.known || current.x != state.reported.x || current.y != state.reported.y)
    {
        JoystickRaw(current, deadzone, &state.rawX, &state.rawY);
        state.reported = current;
        state.known = true;
    }

    internal::ItemOptions itemOptions;
    itemOptions.press = internal::PressMode::Click;
    itemOptions.readOnly = readOnly;
    ImGui::BeginGroup();
    ImGui::ItemSize(pad.frame.GetSize());
    internal::ApplyNextItemData(next);
    const internal::ItemState item = internal::PartBehavior(id, pad.frame, itemOptions);
    internal::ValueDrag2DOptions drag;
    drag.mappingX = {.min = -1.0, .max = 1.0};
    drag.mappingY = {.min = -1.0, .max = 1.0};
    drag.grabRadius = pad.knob * 0.5f;
    drag.disc = o.circular;
    drag.keys = o.springReturn ? internal::PadKeys::Hold : internal::PadKeys::Step;
    drag.readOnly = readOnly;
    const internal::ValueDrag2DResult edit = internal::ValueDrag2DBehavior(item, &state.rawX, &state.rawY, pad.area, drag);
    // With springReturn the end of a drag or key session sends the stick home.
    const bool held = edit.dragging || edit.keyboard;
    if (o.springReturn && edit.ended && !held)
    {
        state.rawX = 0.0;
        state.rawY = 0.0;
    }
    bool written = false;
    if (!readOnly && value != nullptr)
    {
        const ImVec2 next2 = JoystickValue(state.rawX, state.rawY, deadzone, o.circular);
        if (next2.x != current.x || next2.y != current.y)
        {
            *value = next2;
            written = true;
            if (!edit.changed)
                ImGui::MarkItemEdited(id);
        }
        state.reported = *value;
    }

    // ---- Draw ----
    const bool disabled = item.disabled;
    const internal::SliderLook look = internal::PickSliderLook(disabled, readOnly);
    // The stick sits on the pointer or the keys while held, and springs home afterwards: a spring
    // with no response follows its target at once, so it starts from where the stick was let go.
    const SpringSpec follow{0.0f, 1.0f};
    const SpringSpec& spec = held ? follow : t.motion.standard;
    double fx = std::clamp(state.rawX, -1.0, 1.0);
    double fy = std::clamp(state.rawY, -1.0, 1.0);
    if (o.circular)
    {
        const double r = std::sqrt(fx * fx + fy * fy);
        if (r > 1.0)
        {
            fx /= r;
            fy /= r;
        }
    }
    state.drawn = ImVec2(Spring(id, kJoystickStickSubX, static_cast<float>(fx), spec),
                         Spring(id, kJoystickStickSubY, static_cast<float>(fy), spec));
    ImDrawList* drawList = window->DrawList;
    if (item.visible)
    {
        const ImVec2 center(std::round(pad.frame.GetCenter().x), std::round(pad.frame.GetCenter().y));
        const float travel = pad.area.GetWidth() * 0.5f;
        if (o.circular)
        {
            // A round field: the field fill and boundary of a text field, drawn as a circle.
            const float radius = diameter * 0.5f;
            const ImVec4 fill = disabled ? c.fill[3] : Mix(c.fill[2], c.fill[1], item.hoverAmount);
            const ImVec4 outline = o.error != nullptr ? c.destructive : (disabled ? c.separator : ControlBoundaryColor(c));
            if (!readOnly || disabled)
                drawList->AddCircleFilled(center, radius, ToU32(fill), 0);
            drawList->AddCircle(center, radius - HairlineThickness() * 0.5f, ToU32(outline), 0, HairlineThickness());
        }
        else
        {
            internal::FieldFrameLook frameLook;
            frameLook.hoverAmount = item.hoverAmount;
            frameLook.disabled = disabled;
            frameLook.readOnly = readOnly && !disabled;
            frameLook.error = o.error != nullptr;
            internal::DrawFieldFrame(drawList, pad.frame, frameLook);
        }
        // The centre cross and the deadzone ring, in separator hairlines: where 0 is.
        const ImU32 guide = ToU32(c.separator);
        const float cross = std::round(pad.knob * 0.5f);
        HairlineH(drawList, center.x - cross, center.x + cross, center.y, guide);
        HairlineV(drawList, center.x, center.y - cross, center.y + cross, guide);
        if (o.showDeadzone && deadzone > 0.0)
        {
            const float r = static_cast<float>(deadzone) * travel;
            if (o.circular)
                drawList->AddCircle(center, r, ToU32(disabled ? c.labelQuaternary : c.labelTertiary), 0, HairlineThickness());
            else
                StrokeRect(drawList, ImVec2(center.x - r, center.y - r), ImVec2(center.x + r, center.y + r),
                           ToU32(disabled ? c.labelQuaternary : c.labelTertiary), 0.0f, HairlineThickness());
        }
        // The vector from the centre to the stick, in the slider fill color, then the stick.
        const ImVec2 stick(center.x + state.drawn.x * travel, center.y - state.drawn.y * travel);
        const ImU32 fillColor = ToU32(look == internal::SliderLook::Disabled ? c.labelTertiary
                                      : (look == internal::SliderLook::ReadOnly ? c.labelSecondary : c.accent));
        if (std::fabs(stick.x - center.x) + std::fabs(stick.y - center.y) >= 1.0f)
            drawList->AddLine(center, stick, fillColor, m.track);
        internal::DrawSliderKnob(drawList, internal::PadKnobRect(stick, pad.knob), item, look);
    }

    const char* labelEnd = ImGui::FindRenderedTextEnd(label);
    internal::CutTextTooltip(id, pad.frame, item.hovered, item.focusVisible, label, labelEnd, labelEnd != label);
    const internal::ControlMessage message = internal::PickControlMessage(o.error, o.helpText);
    if (message.IsSet())
        internal::DrawControlMessage(message, ImVec2(pad.frame.Min.x, pad.frame.Max.y), pad.frame.GetWidth(), disabled, id,
                                     pad.frame);
    ImGui::EndGroup();
    internal::FinishWidgetGroup(id);
    return written;
}

} // namespace facet
