// Parts shared by the selection family (selection_parts.h).

#include "widgets/selection/selection_parts.h"

#include <algorithm>
#include <cmath>

#include "facet/core/anim.h"
#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/icons.h"
#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/state_storage.h"

namespace facet::internal
{
namespace
{

// The corner of a checkbox: radii.element, at most this share of the box side, so a box never
// reads as the circle of a radio mark.
constexpr float kCheckboxRadiusShare = 0.25f;
// The dot of a selected radio mark: this share of the mark's diameter.
constexpr float kRadioDotShare = 0.375f;
// Animation sub key of a mark's on amount (below kItemAnimSubBase).
constexpr int kMarkSub = 0;

struct SelectionMixedState
{
    bool cleared = false;  // an edit wrote a definite value since `mixed` was switched on
};

// The colors of a mark for its state; `onAmount` blends the off look into the on look.
struct MarkColors
{
    ImVec4 fill;
    ImVec4 outline;
    ImVec4 glyph;
};

MarkColors MarkLook(float onAmount, const ItemState& item)
{
    const Colors& c = T().colors;
    MarkColors look;
    if (item.disabled)
    {
        look.fill = c.fill[3];
        look.outline = c.separator;
        look.glyph = WithAlpha(c.labelTertiary, c.labelTertiary.w * onAmount);
        return look;
    }
    const ImVec4 clear = WithAlpha(c.fill[1], 0.0f);
    const ImVec4 offFill = item.readOnly ? clear : StateFill(clear, c.fill[1], c.fill[0], item);
    const ImVec4 offOutline = ControlBoundaryColor(c);
    // Read-only marks are labelSecondary, so a value that cannot change does not claim the accent.
    const ImVec4 onFill = item.readOnly ? c.labelSecondary : StateFill(c.accent, c.accentHover, c.accentPressed, item);
    const ImVec4 onOutline = item.readOnly ? c.labelSecondary : c.focusRing;
    const ImVec4 glyph = item.readOnly ? c.bg : c.onAccent;
    look.fill = onAmount <= 0.0f ? offFill : (onAmount >= 1.0f ? onFill : Mix(offFill, onFill, onAmount));
    look.outline = onAmount <= 0.0f ? offOutline : (onAmount >= 1.0f ? onOutline : Mix(offOutline, onOutline, onAmount));
    look.glyph = WithAlpha(glyph, glyph.w * onAmount);
    return look;
}

} // namespace

int SelectionRovingKeyStep(ImGuiID focusedId, const SelectionRovingKeys& keys, int* step, bool* toEnd)
{
    *step = 0;
    *toEnd = false;
    const auto take = [focusedId](ImGuiKey key) {
        ImGui::SetKeyOwner(key, focusedId);
        return ImGui::IsKeyPressed(key, ImGuiInputFlags_Repeat, focusedId);
    };
    if (keys.leftRight)
    {
        const bool right = take(ImGuiKey_RightArrow);
        const bool left = take(ImGuiKey_LeftArrow);
        if (right != left)
            *step = right ? 1 : -1;
    }
    if (keys.upDown)
    {
        const bool down = take(ImGuiKey_DownArrow);
        const bool up = take(ImGuiKey_UpArrow);
        if (*step == 0 && down != up)
            *step = down ? 1 : -1;
    }
    if (keys.homeEnd)
    {
        const bool home = take(ImGuiKey_Home);
        const bool end = take(ImGuiKey_End);
        if (*step == 0 && home != end)
        {
            *step = end ? 1 : -1;
            *toEnd = true;
        }
    }
    return *step != 0 ? 0 : -1;
}

void SelectionMoveFocus(ImGuiID id)
{
    ImGui::SetFocusID(id, ImGui::GetCurrentWindow());
    ImGui::SetNavCursorVisible(true);
    SetKeyboardModality(true);
}

bool SelectionMixedShown(ImGuiID id, bool mixed, bool edited)
{
    if (!mixed)
    {
        // Switched off: the next time it is switched on, the look shows again.
        if (void* found = GlobalStateStore().Find(id, StateTypeOf<SelectionMixedState>()))
            static_cast<SelectionMixedState*>(found)->cleared = false;
        return false;
    }
    SelectionMixedState& state = GetState<SelectionMixedState>(id);
    if (edited)
        state.cleared = true;
    return !state.cleared;
}

ImRect SelectionMarkRect(float x, float centerY)
{
    const float side = std::max(1.0f, PxSnapped(T().metrics.checkboxSize));
    const float left = std::round(x);
    const float top = std::round(centerY - side * 0.5f);
    return ImRect(left, top, left + side, top + side);
}

void DrawSelectionCheckbox(ImDrawList* drawList, const ImRect& box, SelectionMark shown, float onAmount,
                           const ItemState& item)
{
    const MarkColors look = MarkLook(onAmount, item);
    const float radius = std::min(Px(T().radii.element), std::round(box.GetWidth() * kCheckboxRadiusShare));
    FillRect(drawList, box.Min, box.Max, ToU32(look.fill), radius);
    StrokeRect(drawList, box.Min, box.Max, ToU32(look.outline), radius, HairlineThickness());
    if (shown != SelectionMark::Off && look.glyph.w > 0.0f)
    {
        // The glyphs are drawn at the box size: their artwork keeps a margin to the box edge.
        const Icon glyph = shown == SelectionMark::Mixed ? Icon("minus") : Icon("check");
        DrawControlIcon(drawList, glyph, box.Min.x, box.GetCenter().y, box.GetWidth(), ToU32(look.glyph));
    }
}

void DrawSelectionRadio(ImDrawList* drawList, const ImRect& box, float onAmount, const ItemState& item)
{
    const MarkColors look = MarkLook(onAmount, item);
    const ImVec2 center = box.GetCenter();
    const float radius = box.GetWidth() * 0.5f;
    drawList->AddCircleFilled(center, radius, ToU32(look.fill));
    drawList->AddCircle(center, radius, ToU32(look.outline), 0, HairlineThickness());
    if (look.glyph.w > 0.0f)
    {
        // The dot grows in with the fill, so the change reads as one motion.
        const float dot = std::max(HairlineThickness(), radius * 2.0f * kRadioDotShare) * 0.5f;
        drawList->AddCircleFilled(center, dot * std::clamp(onAmount, 0.0f, 1.0f), ToU32(look.glyph));
    }
}

float SelectionMarkAmount(ImGuiID id, bool on)
{
    return Tween(id, kMarkSub, on ? 1.0f : 0.0f, T().motion.durationFast);
}

} // namespace facet::internal
