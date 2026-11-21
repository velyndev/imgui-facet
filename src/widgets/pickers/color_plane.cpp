// The two-axis input of the color pickers (color_plane.h).

#include "widgets/pickers/color_plane.h"

#include <algorithm>
#include <cmath>

#include "internal/value_drag.h"

namespace facet::internal
{
namespace
{

struct ColorPlaneSession
{
    ImGuiID id = 0;
    bool mouse = false;
    bool keyboard = false;
    double startX = 0.0;
    double startY = 0.0;
    ImVec2 pointer;     // the point the handle follows: the press, plus the travel since (finer with Shift)
    ImVec2 lastMouse;
    bool pointerLost = false;
};

ColorPlaneSession& CurrentPlaneSession()
{
    static ColorPlaneSession session;
    return session;
}

double Clamp01(double value)
{
    return std::isfinite(value) ? std::clamp(value, 0.0, 1.0) : 0.0;
}

double WrapFraction(double value)
{
    if (!std::isfinite(value))
        return 0.0;
    return value - std::floor(value);
}

// Presses of `key` this frame for `owner`, with the key repeat counted from the elapsed time.
int PlanePressCount(ImGuiKey key, ImGuiID owner)
{
    if (!ImGui::TestKeyOwner(key, owner))
        return 0;
    const ImGuiIO& io = ImGui::GetIO();
    return ImGui::GetKeyPressedAmount(key, io.KeyRepeatDelay, io.KeyRepeatRate);
}

} // namespace

ColorPlaneResult ColorPlaneBehavior(const ItemState& item, double* x, double* y, const ColorPlaneOptions& options)
{
    ColorPlaneResult result;
    ImGuiContext* context = ImGui::GetCurrentContext();
    if (context == nullptr || x == nullptr || y == nullptr || item.id == 0 || options.map == nullptr)
        return result;
    ImGuiContext& g = *context;
    ImGuiIO& io = g.IO;
    ColorPlaneSession& session = CurrentPlaneSession();
    const ImGuiID id = item.id;
    const bool readOnly = options.readOnly || item.readOnly;

    if (session.id == id && g.ActiveId != id)
        session = {};
    if (item.disabled || item.forced != ForcedItemState::None)
    {
        if (session.id == id)
        {
            if (session.keyboard && g.ActiveId == id && g.ActiveIdSource == ImGuiInputSource_None)
                ImGui::ClearActiveID();
            session = {};
        }
        return result;
    }

    const auto write = [&](double nx, double ny) {
        if (readOnly || std::isnan(nx) || std::isnan(ny))
            return;
        nx = options.wrapX ? WrapFraction(nx) : Clamp01(nx);
        ny = options.twoAxes ? Clamp01(ny) : *y;
        if (options.clamp != nullptr)
            options.clamp(options.user, &nx, &ny);
        if (nx != *x || ny != *y)
        {
            *x = nx;
            *y = ny;
            result.changed = true;
            MarkWidgetEdited(id);
        }
    };
    const auto cancel = [&]() {
        if (!readOnly && (*x != session.startX || *y != session.startY))
        {
            *x = session.startX;
            *y = session.startY;
            result.changed = true;
            MarkWidgetEdited(id);
        }
        result.canceled = true;
        MarkEditCanceled(id);
        session = {};
        ImGui::ClearActiveID();
    };
    const auto mapped = [&](ImVec2 point) {
        const ImVec2 f = options.map(options.user, point);
        return std::pair<double, double>(f.x, f.y);
    };

    // ---- Mouse ----
    if (g.ActiveId == id && g.ActiveIdSource == ImGuiInputSource_Mouse && item.held)
    {
        const ImVec2 mouse = io.MousePos;
        if (session.id != id || !session.mouse)
        {
            const ImVec2 press = ImGui::IsMousePosValid(&io.MouseClickedPos[ImGuiMouseButton_Left])
                                     ? io.MouseClickedPos[ImGuiMouseButton_Left]
                                     : mouse;
            session = {};
            session.id = id;
            session.mouse = true;
            session.startX = *x;
            session.startY = *y;
            session.pointer = press;
            session.lastMouse = press;
            const auto [fx, fy] = mapped(press);
            write(fx, fy);
        }
        else if (!ImGui::IsMousePosValid(&mouse))
        {
            session.pointerLost = true;
        }
        else if (session.pointerLost)
        {
            session.pointerLost = false;
            session.lastMouse = mouse;
        }
        else if (mouse.x != session.lastMouse.x || mouse.y != session.lastMouse.y)
        {
            const float factor = io.KeyShift ? static_cast<float>(kFineDragFactor) : 1.0f;
            session.pointer.x += (mouse.x - session.lastMouse.x) * factor;
            session.pointer.y += (mouse.y - session.lastMouse.y) * factor;
            session.lastMouse = mouse;
            const auto [fx, fy] = mapped(session.pointer);
            write(fx, fy);
        }
        result.dragging = true;
        ImGui::SetKeyOwner(ImGuiKey_Escape, id);
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, ImGuiInputFlags_None, id))
        {
            cancel();
            result.dragging = false;
        }
        return result;
    }

    // ---- Keyboard ----
    const bool focused = item.focused && g.NavId == id;
    const bool keysEnabled = focused && !readOnly;
    static constexpr ImGuiKey kPlaneKeys[] = {
        ImGuiKey_LeftArrow,       ImGuiKey_RightArrow,       ImGuiKey_UpArrow,       ImGuiKey_DownArrow,
        ImGuiKey_GamepadDpadLeft, ImGuiKey_GamepadDpadRight, ImGuiKey_GamepadDpadUp, ImGuiKey_GamepadDpadDown,
        ImGuiKey_PageUp,          ImGuiKey_PageDown,         ImGuiKey_Home,          ImGuiKey_End,
    };
    if (session.id == id && session.keyboard)
    {
        bool anyDown = false;
        for (const ImGuiKey key : kPlaneKeys)
            anyDown = anyDown || ImGui::IsKeyDown(key);
        if (!anyDown || !focused)
        {
            session = {};
            ImGui::ClearActiveID();
        }
    }

    bool stepped = false;
    double nx = *x;
    double ny = *y;
    if (keysEnabled)
    {
        for (const ImGuiKey key : kPlaneKeys)
            ImGui::SetKeyOwner(key, id);
        const StepSize size = io.KeyCtrl ? StepSize::Coarse : (io.KeyShift ? StepSize::Fine : StepSize::Normal);
        ValueMapping axisX;
        axisX.wrap = options.wrapX;
        const ValueMapping axisY;
        const int right = PlanePressCount(ImGuiKey_RightArrow, id) + PlanePressCount(ImGuiKey_GamepadDpadRight, id);
        const int left = PlanePressCount(ImGuiKey_LeftArrow, id) + PlanePressCount(ImGuiKey_GamepadDpadLeft, id);
        const int up = PlanePressCount(ImGuiKey_UpArrow, id) + PlanePressCount(ImGuiKey_GamepadDpadUp, id);
        const int down = PlanePressCount(ImGuiKey_DownArrow, id) + PlanePressCount(ImGuiKey_GamepadDpadDown, id);
        const int pageUp = PlanePressCount(ImGuiKey_PageUp, id);
        const int pageDown = PlanePressCount(ImGuiKey_PageDown, id);
        const int home = PlanePressCount(ImGuiKey_Home, id);
        const int end = PlanePressCount(ImGuiKey_End, id);
        const int moreX = right + (options.twoAxes ? 0 : up);
        const int lessX = left + (options.twoAxes ? 0 : down);
        if (moreX > 0)
            nx = KeyboardStep(axisX, nx, 1, size, moreX);
        if (lessX > 0)
            nx = KeyboardStep(axisX, nx, -1, size, lessX);
        if (options.twoAxes)
        {
            if (up > 0)
                ny = KeyboardStep(axisY, ny, 1, size, up);
            if (down > 0)
                ny = KeyboardStep(axisY, ny, -1, size, down);
            if (pageUp > 0)
                ny = PageStep(axisY, ny, 1, pageUp);
            if (pageDown > 0)
                ny = PageStep(axisY, ny, -1, pageDown);
        }
        else
        {
            if (pageUp > 0)
                nx = PageStep(axisX, nx, 1, pageUp);
            if (pageDown > 0)
                nx = PageStep(axisX, nx, -1, pageDown);
        }
        if (home > 0)
            nx = 0.0;
        if (end > 0)
            nx = options.wrapX ? 0.0 : 1.0;
        stepped = moreX > 0 || lessX > 0 || pageUp > 0 || pageDown > 0 || home > 0 || end > 0 ||
                  (options.twoAxes && (up > 0 || down > 0));
        if (stepped)
            ImGui::SetNavCursorVisible(true);
    }

    if (stepped)
    {
        if (g.ActiveId != id)
        {
            // One edit session per key press with its repeats, as the value drag core keeps it.
            ImGui::SetActiveID(id, g.CurrentWindow);
            g.ActiveIdSource = ImGuiInputSource_None;
            session = {};
            session.id = id;
            session.keyboard = true;
            session.startX = *x;
            session.startY = *y;
        }
        write(nx, ny);
        result.keyboard = true;
    }

    if (session.id == id && session.keyboard)
    {
        result.keyboard = true;
        ImGui::SetKeyOwner(ImGuiKey_Escape, id);
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, ImGuiInputFlags_None, id))
        {
            cancel();
            result.keyboard = false;
        }
    }
    return result;
}

void ResetColorPlaneState()
{
    CurrentPlaneSession() = {};
}

} // namespace facet::internal
