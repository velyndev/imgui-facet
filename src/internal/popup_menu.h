#pragma once

// Menu rows: the row behaviour and look every menu shares, on the popup host (popup_host.h), the
// item behaviour and the focus ring. Menu, the menus of MenuBar, MenuItem, ContextMenu and the
// overflow menus of TabBar and Toolbar are built on it, so a submenu opens, stays open and closes
// the same way in each of them.
//
//     if (BeginPopupLevel(editMenu))
//     {
//         MenuLevelBegin(editMenu);
//         if (MenuRow(editMenu, "Cut", {.shortcut = "Ctrl+X"}).pressed)
//             Cut();                                       // the whole chain is closed already
//         (void)MenuRow(editMenu, "Arrange", {.submenu = arrangeMenu});
//         if (BeginPopupLevel(arrangeMenu))                // right after its row
//         {
//             MenuLevelBegin(arrangeMenu);
//             ... rows of the submenu ...
//             EndPopupLevel();
//         }
//         EndPopupLevel();
//     }
//
// What a row does, so the menus do not:
//   - A leaf row is pressed by a click, Enter or Space; it then closes its whole chain of levels
//     (ClosePopupLevelChain) and reports `pressed`.
//   - A row with a submenu opens it after the pointer rests on the row for overlay.submenuDelay
//     (times motion.scale), and at once on a click, Enter, Space or the Right arrow. The submenu
//     opens after the row, its first row lined up with the row, and follows it (the anchor is set
//     every frame). It stays open while the pointer travels toward it across other rows
//     (PopupPointerAimsAt): resting on a leaf of the same level closes it only when the pointer
//     is not on its way there. The Left arrow inside a submenu closes it and the focus goes back
//     to its row; Escape closes one level per press (the popup host). A submenu closed by a key
//     while the pointer happens to rest on its row stays closed until the pointer moves.
//   - Look: the level's full width, the hover fill (fill[2], fill[1] while pressed) that stays
//     while the row's submenu is open, an optional leading icon or check mark in a slot every row
//     of the level keeps once one row uses it, the label, the shortcut text right-aligned in
//     labelSecondary overlay.menuShortcutGap after the label, a chevron-right for a submenu, and
//     the focus ring inside the row (the level padding is narrower than the ring). Disabled rows
//     draw in labelTertiary and do nothing.
//   - Captures: a forced Open state (ForceNextItemState, item.h) on a row with a submenu shows the
//     submenu open for that frame (ForcePopupLevelOpen).
//
// State kept per level id (state_storage.h): the row the pointer rests on and since when, the
// open submenu of the level, the level's parent row, and whether the level keeps a leading slot.

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/icons.h"
#include "internal/item.h"

namespace facet::internal
{

struct MenuRowDesc
{
    const char* shortcut = nullptr;  // shortcut text at the end of the row (facet::ChordToText); null for none
    Icon icon;                       // leading icon; none for none
    ImGuiID submenu = 0;             // the popup level the row opens; 0 for a leaf
    bool checked = false;            // a check mark in the leading slot (a toggle item)
    bool disabled = false;
};

struct MenuRowResult
{
    ItemState item;            // the row's item behaviour; Dear ImGui's last item after the call
    bool pressed = false;      // a leaf was pressed: its action runs now, the chain is closed
    bool submenuOpen = false;  // the row's submenu is open (or forced open) this frame
};

// Starts the rows of `level`: call right after BeginPopupLevel(level) returned true. Inside a
// submenu the Left arrow then closes the level and gives the focus back to its row.
void MenuLevelBegin(ImGuiID level);

// One row of the menu level `level` (the id passed to BeginPopupLevel). `label` is hashed on the
// id stack and drawn up to "##". With desc.submenu, submit that level right after this call.
MenuRowResult MenuRow(ImGuiID level, const char* label, const MenuRowDesc& desc = {});

} // namespace facet::internal
