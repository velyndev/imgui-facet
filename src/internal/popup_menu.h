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
//   - Look: the level's full width and the row height of an open list (ListRowHeight, Regular),
//     with no item spacing between rows, so rows meet edge to edge as in a list and the pointer
//     never rests between two of them (other content in a level keeps the regular spacing); the hover fill (fill[2], fill[1] while pressed) that stays
//     while the row's submenu is open, an optional leading icon or check mark in a slot every row
//     of the level keeps once one row uses it, the label, the shortcut text right-aligned in
//     labelSecondary overlay.menuShortcutGap after the label, a chevron-right for a submenu, and
//     the focus ring inside the row (the level padding is narrower than the ring). The check mark
//     takes ChoiceCheckColor, the icon labelSecondary. Disabled rows
//     draw in labelTertiary and do nothing.
//   - Captures: a forced Open state (ForceNextItemState, item.h) on a row with a submenu shows the
//     submenu open for that frame (ForcePopupLevelOpen).
//   - Typeahead: while the level has the keyboard focus and no item is active, typed characters
//     gather into a text (list.typeaheadTimeout starts a new one) that moves the focus to the next
//     enabled row whose label starts with it, case-insensitive in ASCII, as lists do
//     (ListTypeaheadFind, list_view.h); typing the same letter again cycles through the rows that
//     start with it. The rows of the previous frame are searched, so a level matches from its
//     second frame on.
//   - Marks: a check mark (`checked`), or with `radio` a round dot, in the leading slot; `slot`
//     keeps the slot for a row whose mark is off, so check and radio rows line up whether or not
//     they are on. A `destructive` row draws its label and icon in the destructive color.
//
// State kept per level id (state_storage.h): the row the pointer rests on and since when, the
// open submenu of the level, the level's parent row, whether the level keeps a leading slot, the
// typed text and the rows of the last frame for typeahead.

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
    bool radio = false;              // `checked` draws a round dot instead of the check mark (a choice of several)
    bool slot = false;               // keeps the leading slot even when the row has no icon and is not checked
    bool destructive = false;        // label and icon in the destructive color
};

// How many rows of a level take part in typeahead; later rows are not matched.
inline constexpr int kMenuTypeaheadRows = 64;

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

// The color of the check mark of a chosen row, shared by menu rows and the rows of list popups
// and list boxes, so the current choice reads the same everywhere: accentText (the accent marks
// selection), labelTertiary while disabled.
ImU32 ChoiceCheckColor(bool disabled);

} // namespace facet::internal
