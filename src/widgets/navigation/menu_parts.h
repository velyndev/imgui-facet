#pragma once

// The menu stacks shared by BeginMenuBar (menu_bar.cpp), BeginMenu (menu.cpp) and the menu items
// (menu_item.cpp): the bar being submitted, with the overflow decision of its titles, and the menus
// that are open (BeginMenu returned true and EndMenu has not run yet). Names carry the family,
// because every family links into the same library.
//
// A bar lays out its titles from left to right. Which titles show comes from the fitter
// (overflow.h) over the title widths of the last frame, as for a toolbar; a title that does not
// show is listed in the bar's "more" menu, which EndMenuBar draws after the last visible title. A
// row of that menu opens the hidden menu in its place, anchored at the "more" button, so the
// content of a hidden menu is still submitted by its own BeginMenu call.

#include <imgui.h>
#include <imgui_internal.h>

#include "internal/overflow.h"
#include "widgets/navigation/navigation_parts.h"

namespace facet::internal
{

// Titles one bar lays out; later titles are left out with a usage error.
inline constexpr int kMenuBarMaxTitles = 32;
// Menus open inside one another at once (a bar menu, its submenus); deeper ones are reported.
inline constexpr int kMenuStackDepth = 16;
// Bytes of a hidden title's label kept for the "more" menu; a longer label is cut there.
inline constexpr int kMenuBarLabelSize = 64;

// What a menu bar keeps between frames, under its id.
struct MenuBarState
{
    OverflowItem items[kMenuBarMaxTitles] = {};  // the title widths of the last frame
    int count = 0;
    OverflowState overflow;
    ImRect titleRects[kMenuBarMaxTitles];        // where each title was drawn (the more button for a hidden one)
    ImGuiID titleLevels[kMenuBarMaxTitles] = {}; // the popup level of each title
    bool titleEnabled[kMenuBarMaxTitles] = {};
    ImRect moreRect;
    ImGuiID moreLevel = 0;
    int pendingOpen = -1;     // a hidden title to open from the next frame's call (a "more" row)
    int pendingFrame = -1;
    bool altArmed = false;     // Alt went down alone; its release focuses the bar
    int switchFrame = -1;      // the frame Left or Right last switched menus
    NavigationRoving roving;
};

// The bar being submitted, between BeginMenuBar and EndMenuBar.
struct MenuBarFrame
{
    bool active = false;
    ImGuiWindow* window = nullptr;
    ImGuiID id = 0;
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float height = 0.0f;
    float gap = 0.0f;
    float moreWidth = 0.0f;
    float x = 0.0f;                              // where the next visible title starts
    bool decided[kMenuBarMaxTitles] = {};        // the fitter's choice from the last frame's widths
    int decidedCount = 0;
    bool open = false;                           // one of the bar's menus was open at BeginMenuBar
    bool showMore = false;                       // the fitter moves titles into the "more" menu
    int index = 0;                               // the next title
    OverflowItem items[kMenuBarMaxTitles] = {};  // this frame's widths
    ImRect rects[kMenuBarMaxTitles];
    ImGuiID levels[kMenuBarMaxTitles] = {};
    bool enabled[kMenuBarMaxTitles] = {};
    int hidden[kMenuBarMaxTitles] = {};          // titles listed in the "more" menu, in order
    char hiddenLabels[kMenuBarMaxTitles][kMenuBarLabelSize] = {};
    int hiddenCount = 0;
};

// One menu that is open.
struct MenuFrame
{
    ImGuiID level = 0;
    ImGuiID bar = 0;                // the bar whose title opened it; 0 for a submenu or a lone menu
    int title = -1;                 // its title index in that bar
    bool focusedSubmenuRow = false; // a row of this menu that opens a submenu has the keyboard focus
};

// The innermost bar being submitted to the current window, or null.
MenuBarFrame* CurrentMenuBar();
MenuBarState& MenuBarStateOf(ImGuiID barId);

// The menu stack: BeginMenu pushes when it returns true, EndMenu pops.
void PushMenuFrame(const MenuFrame& frame);
// The innermost open menu, or null.
MenuFrame* CurrentMenuFrame();
// Removes the innermost frame and returns a copy; a default frame when the stack is empty.
MenuFrame PopMenuFrame();

// Opens the menu level `level` below `anchor` (a title or the "more" button), at least
// overlay.menuMinWidth wide, or moves it there when it is open. The level takes the keyboard focus.
void OpenMenuLevel(ImGuiID level, const ImRect& anchor);
// Keeps an open menu level at `anchor` (it scrolls or the bar resizes).
void PlaceMenuLevel(ImGuiID level, const ImRect& anchor);
// Shows `level` at `anchor` for this frame only (a forced Open state in a capture).
void ForceMenuLevel(ImGuiID level, const ImRect& anchor);

} // namespace facet::internal
