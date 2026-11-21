// Menu rows (popup_menu.h) on the popup host, the item behaviour and the focus ring.

#include "internal/popup_menu.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "facet/widgets/common.h"
#include "internal/focus.h"
#include "internal/list_view.h"
#include "internal/popup_host.h"
#include "internal/state_storage.h"

namespace facet::internal
{
namespace
{

// Bytes of a row label kept for typeahead; a typed text longer than this matches no row.
constexpr int kMenuTypeaheadLabel = 32;
// The dot of a radio row: this share of the icon slot, so it reads lighter than a check mark.
constexpr float kMenuRadioDotShare = 0.4f;

struct MenuTypeaheadRow
{
    ImGuiID id = 0;
    bool disabled = false;
    char label[kMenuTypeaheadLabel] = {};
};

struct MenuLevelState
{
    ImGuiID hoverRow = 0;      // the row the pointer moved onto last
    double hoverSince = 0.0;
    ImGuiID openChild = 0;     // the submenu a row of this level opened last
    ImGuiID parent = 0;        // the level whose row opened this one
    ImGuiID parentRow = 0;     // that row
    bool slot = false;         // a row of the last frame had an icon or a check mark
    bool slotNext = false;     // the same for this frame
    int frame = -1;            // frame of the last MenuLevelBegin
    // Typeahead: the typed text, and the rows of the last frame and of this one.
    ListTypeahead typeahead;
    MenuTypeaheadRow rows[kMenuTypeaheadRows];
    int rowCount = 0;
    MenuTypeaheadRow rowsNext[kMenuTypeaheadRows];
    int rowCountNext = 0;
};

MenuLevelState& LevelState(ImGuiID level)
{
    return GetState<MenuLevelState>(ImHashStr("##facet.menu", 0, level));
}

void TypeaheadEntry(void* user, int index, ListEntryInfo* out)
{
    const MenuLevelState& state = *static_cast<const MenuLevelState*>(user);
    out->label = state.rows[index].label;
    out->disabled = state.rows[index].disabled;
    out->id = state.rows[index].id;
}

// Typed characters move the focus to the next row whose label starts with the typed text.
void MenuTypeahead(MenuLevelState& state)
{
    const ImGuiContext& g = *GImGui;
    if (!ImGui::IsWindowFocused() || g.ActiveId != 0 || state.rowCount == 0)
        return;
    if (!state.typeahead.Update())
        return;
    int from = -1;
    for (int i = 0; i < state.rowCount; ++i)
        if (state.rows[i].id == g.NavId)
            from = i;
    const int found = state.typeahead.Find(state.rowCount, std::max(from, 0), TypeaheadEntry, &state);
    if (found < 0 || found == from)
        return;
    ImGui::SetFocusID(state.rows[found].id, ImGui::GetCurrentWindow());
    ImGui::SetNavCursorVisible(true);
    SetKeyboardModality(true);
}

bool InFrame()
{
    const ImGuiContext* g = ImGui::GetCurrentContext();
    return g != nullptr && g->WithinFrameScope && g->CurrentWindow != nullptr;
}

} // namespace

ImU32 ChoiceCheckColor(bool disabled)
{
    const Colors& c = T().colors;
    return ToU32(disabled ? c.labelTertiary : c.accentText);
}

void MenuLevelBegin(ImGuiID level)
{
    if (!InFrame() || level == 0)
        return;
    MenuLevelState& state = LevelState(level);
    const int frame = ImGui::GetFrameCount();
    if (state.frame != frame)
    {
        state.slot = state.slotNext;
        state.slotNext = false;
        // The rows of the frame before become the ones typeahead searches.
        std::copy(state.rowsNext, state.rowsNext + state.rowCountNext, state.rows);
        state.rowCount = state.rowCountNext;
        state.rowCountNext = 0;
        state.frame = frame;
        MenuTypeahead(state);
    }
    // The Left arrow inside a submenu closes it; the popup host gives the focus back to its row.
    if (state.parent != 0 && ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false))
    {
        ClosePopupLevel(level);
        LevelState(state.parent).hoverRow = 0;
        // The press is used: the level below (a menu of a bar switching menus on Left) must not see it.
        ImGui::SetKeyOwner(ImGuiKey_LeftArrow, level, ImGuiInputFlags_LockThisFrame);
    }
}

MenuRowResult MenuRow(ImGuiID level, const char* label, const MenuRowDesc& desc)
{
    MenuRowResult result;
    if (!InFrame() || level == 0)
        return result;
    if (label == nullptr)
        label = "";
    const Theme& t = T();
    const Colors& c = t.colors;
    MenuLevelState& state = LevelState(level);
    const bool hasSlotContent = desc.icon.IsValid() || desc.checked || desc.slot;
    state.slotNext = state.slotNext || hasSlotContent;
    const bool slot = state.slot || hasSlotContent;

    const char* end = ImGui::FindRenderedTextEnd(label);
    PushTextStyle(TextStyle::Body);
    const float padding = Px(t.spacing.sm);
    const float height = ControlHeight(ControlSize::Regular);
    const float iconSize = Px(t.type[TextStyle::Body].size);
    const float slotWidth = slot ? iconSize + padding : 0.0f;
    const float labelWidth = ImGui::CalcTextSize(label, end).x;
    const float shortcutWidth = desc.shortcut != nullptr ? ImGui::CalcTextSize(desc.shortcut).x : 0.0f;
    const float shortcutSpan = desc.shortcut != nullptr ? Px(t.overlay.menuShortcutGap) + shortcutWidth : 0.0f;
    const float chevronSpan = desc.submenu != 0 ? iconSize + padding : 0.0f;
    const float natural = padding + slotWidth + labelWidth + shortcutSpan + chevronSpan + padding;
    const float width = std::max(natural, ImGui::GetContentRegionAvail().x);

    ItemOptions options;
    options.hitTarget = false;
    options.disabled = desc.disabled;
    const ItemState row = ItemBehavior(ImGui::GetID(label), ImVec2(width, height), options);
    result.item = row;
    if (state.rowCountNext < kMenuTypeaheadRows)
    {
        MenuTypeaheadRow& entry = state.rowsNext[state.rowCountNext++];
        entry.id = row.id;
        entry.disabled = row.disabled;
        const std::size_t length = std::min(static_cast<std::size_t>(end - label), sizeof(entry.label) - 1);
        std::memcpy(entry.label, label, length);
        entry.label[length] = '\0';
    }

    // ---- Behaviour ----
    const double now = ImGui::GetTime();
    const float delay = t.overlay.submenuDelay * t.motion.scale;
    if (desc.submenu != 0 && PopupLevelClosedReason(desc.submenu) == PopupCloseReason::Escape)
        state.hoverRow = 0;
    const ImVec2 delta = ImGui::GetIO().MouseDelta;
    if (row.hovered && state.hoverRow != row.id && (delta.x != 0.0f || delta.y != 0.0f))
    {
        state.hoverRow = row.id;
        state.hoverSince = now;
    }
    const bool rested = row.hovered && !row.disabled && state.hoverRow == row.id && now - state.hoverSince >= delay;
    bool submenuShown = false;
    if (desc.submenu != 0)
    {
        const ImGuiID child = desc.submenu;
        const bool open = IsPopupLevelOpen(child);
        PopupAnchor anchor;
        anchor.rect = row.rect;
        anchor.side = PopupSide::After;
        // The child's first row lines up with this row: offset by the level padding.
        anchor.rect.Min.y -= Px(t.spacing.xs);
        const bool keyOpen = row.focused && ImGui::IsKeyPressed(ImGuiKey_RightArrow, false);
        // A submenu the pointer opened by resting on its row leaves the keyboard focus in this
        // level, so the arrows and typeahead keep working here; a click or a key moves it inside.
        const PopupOptions childOptions{.takeFocus = row.pressed || keyOpen, .minWidth = Px(t.overlay.menuMinWidth)};
        if (row.open)
            ForcePopupLevelOpen(child, anchor, childOptions);
        else if (!row.disabled && (row.pressed || keyOpen || rested) && !open)
            OpenPopupLevel(child, anchor, childOptions);
        if (keyOpen && !row.disabled)
            ImGui::SetKeyOwner(ImGuiKey_RightArrow, row.id, ImGuiInputFlags_LockThisFrame);
        SetPopupLevelAnchor(child, anchor);
        MenuLevelState& childState = LevelState(child);
        childState.parent = level;
        childState.parentRow = row.id;
        submenuShown = row.open || IsPopupLevelOpen(child);
        if (submenuShown)
            state.openChild = child;
    }
    else
    {
        // Resting on a leaf closes the level's open submenu, unless the pointer is on its way there.
        if (rested && state.openChild != 0 && IsPopupLevelOpen(state.openChild) &&
            !PopupPointerAimsAt(PopupLevelRect(state.openChild)))
            ClosePopupLevel(state.openChild);
        if (row.pressed && !row.disabled)
        {
            result.pressed = true;
            ClosePopupLevelChain(level);
        }
    }
    result.submenuOpen = submenuShown;

    // ---- Look ----
    if (row.visible)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const float radius = Px(t.radii.element);
        const float feedback =
            row.disabled ? 0.0f : std::max({row.hoverAmount, row.pressAmount, submenuShown ? 1.0f : 0.0f});
        if (feedback > 0.0f)
        {
            const ImVec4 fill = Mix(c.fill[2], c.fill[1], row.pressAmount);
            FillRect(drawList, row.rect.Min, row.rect.Max, ToU32(WithAlpha(fill, fill.w * feedback)), radius);
        }
        const bool destructive = desc.destructive && !row.disabled;
        const ImU32 text = ToU32(row.disabled ? c.labelTertiary : (destructive ? c.destructive : c.label));
        const ImU32 secondary = ToU32(row.disabled ? c.labelTertiary : c.labelSecondary);
        const float iconY = row.rect.Min.y + (height - iconSize) * 0.5f;
        float x = row.rect.Min.x + padding;
        if (desc.checked && desc.radio)
        {
            const float dot = std::max(1.0f, std::round(iconSize * kMenuRadioDotShare));
            drawList->AddCircleFilled(ImVec2(x + iconSize * 0.5f, iconY + iconSize * 0.5f), dot * 0.5f,
                                      ChoiceCheckColor(row.disabled));
        }
        else if (desc.checked)
        {
            DrawIcon(drawList, Icon("check"), ImVec2(x, iconY), iconSize, ChoiceCheckColor(row.disabled));
        }
        else if (desc.icon.IsValid())
        {
            DrawIcon(drawList, desc.icon, ImVec2(x, iconY), iconSize, destructive ? text : secondary);
        }
        x += slotWidth;
        const float textY = CenteredTextY(row.rect.Min.y, height);
        drawList->AddText(ImVec2(x, textY), text, label, end);
        float right = row.rect.Max.x - padding;
        if (desc.submenu != 0)
        {
            DrawIcon(drawList, Icon("chevron-right"), ImVec2(right - iconSize, iconY), iconSize, secondary);
            right -= iconSize + padding;
        }
        if (desc.shortcut != nullptr)
            drawList->AddText(ImVec2(right - shortcutWidth, textY), secondary, desc.shortcut);
        ImRect ring = row.rect;
        ring.Expand(-FocusRingOutset());
        DrawFocusRing(drawList, ring, std::max(0.0f, radius - FocusRingOutset()), row.focusAmount);
    }
    PopTextStyle();
    return result;
}

} // namespace facet::internal
