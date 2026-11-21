#pragma once

// Navigation: the sidebar, tab bars, menu bars, menus and menu items, breadcrumbs, pagination,
// wizards, step indicators and the shortcut sheet.
//
// The declarations follow docs/api-conventions.md; widgets/common.h holds the types shared
// with other families, and docs/widgets/navigation.md has usage examples. The entry of each
// widget there ends with its status; a widget whose status is planned draws a placeholder.
// Shortcuts themselves are registered and routed in core/input.h; menu items
// and the shortcut sheet show them.
//
// When End is called (rule 8): a Begin that returns void always lays out its content, and its
// End is always called. A Begin that returns [[nodiscard]] bool may skip its content; its End
// is called only after true. Every pair has an XScope class. Null pointers, null labels and
// out-of-range values follow rule 14.

#include <span>

#include <imgui.h>

#include "facet/core/input.h"
#include "facet/widgets/common.h"
#include "facet/widgets/lists.h"

namespace facet
{

// --- Sidebar -------------------------------------------------------------------------------------

// How a sidebar shows its items.
enum class SidebarMode
{
    Auto,      // Expanded in an Expanded width, Rail in a Regular or Compact one
    Expanded,  // icons and labels
    Rail,      // icons only; labels become tooltips
    Hidden,    // not drawn; only on the caller's request, never chosen by Auto
};

struct SidebarOptions
{
    SidebarMode mode = SidebarMode::Auto;  // Auto follows the width class of the available width
    float width = 0.0f;                // units; width while expanded; 0 for the token default
    float height = 0.0f;               // units; 0 fills the available height
};

// A vertical navigation column. Submit SidebarItem and BeginSidebarSection between
// BeginSidebar and EndSidebar. The width animates between the modes, and the selection pill
// slides to the selected item. Up and Down move between the items. With Auto the width class
// (core/layout.h) picks the mode, and the narrowest form is the icon rail, so navigation stays
// reachable at every width. Returns false only while the caller's mode is Hidden; call
// EndSidebar only after true.
[[nodiscard]] bool BeginSidebar(const char* id, const SidebarOptions& options = {});
void EndSidebar();

struct SidebarItemOptions
{
    bool selected = false;             // the item the selection pill marks; the caller sets it from its own state
    int badge = 0;                     // count shown at the end; 0 for none
    Tone badgeTone = Tone::Neutral;    // color of the badge; Neutral is the gray fill
};

// A navigation entry with an icon and the visible label part (a tooltip in rail mode). Returns
// true when it is activated (click, Enter, Space); the caller then selects it.
bool SidebarItem(const char* label, Icon icon, const SidebarItemOptions& options = {});

struct SidebarSectionOptions
{
    bool collapsible = false;          // the header toggles the section's items
    bool defaultOpen = true;           // open state the first time it appears, when collapsible
};

// A group header inside a sidebar; a hairline in rail mode. Returns true while the section's
// items are shown (always unless collapsible and closed); call EndSidebarSection only after
// true. The open state is kept under the id.
[[nodiscard]] bool BeginSidebarSection(const char* label, const SidebarSectionOptions& options = {});
void EndSidebarSection();

class [[nodiscard]] SidebarScope
{
public:
    explicit SidebarScope(const char* id, const SidebarOptions& options = {}) : open_(BeginSidebar(id, options)) {}
    ~SidebarScope() { if (open_) EndSidebar(); }
    SidebarScope(const SidebarScope&) = delete;
    SidebarScope& operator=(const SidebarScope&) = delete;
    explicit operator bool() const { return open_; }
private:
    bool open_;
};

class [[nodiscard]] SidebarSectionScope
{
public:
    explicit SidebarSectionScope(const char* label, const SidebarSectionOptions& options = {})
        : open_(BeginSidebarSection(label, options)) {}
    ~SidebarSectionScope() { if (open_) EndSidebarSection(); }
    SidebarSectionScope(const SidebarSectionScope&) = delete;
    SidebarSectionScope& operator=(const SidebarSectionScope&) = delete;
    explicit operator bool() const { return open_; }
private:
    bool open_;
};

// --- Tab bar -------------------------------------------------------------------------------------

enum class TabStyle
{
    Pill,       // the selection pill behind the selected tab
    Underline,  // an accent line under the selected tab
};

struct TabBarOptions
{
    TabStyle style = TabStyle::Pill;   // how the selected tab is marked
    bool closable = false;             // a close button per tab; read TabBarClosedTab
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;                // units; 0 fills; tabs that do not fit move into a "more" menu
    bool reorderable = false;          // tabs reorder by drag and keyboard pick-up; read TabBarMovedTab (since 0.1)
};

// The tab strip; the caller draws the content of the tab *index below it. Tabs show the item
// icon, label and detail (as a badge). Left and Right move between tabs; Ctrl+Page Down and
// Ctrl+Page Up switch to the next and previous tab while focus is inside the window (Ctrl+Tab
// stays with Dear ImGui's window switcher, which always claims it). Writes *index and returns
// true when another tab is chosen. A closed tab is reported by TabBarClosedTab; the caller removes
// it. With `reorderable`, a tab is dragged to a new place, or picked up with Ctrl+Space (Space
// still selects), moved with Left and Right, dropped with Space or Enter and put back with
// Escape; TabBarMovedTab reports the move, the caller applies it to its tabs, and TabBar has
// already written *index to the place the selected tab lands on.
// Default width: the available width. Tab widths of the last frame are kept under the id.
bool TabBar(const char* id, int* index, std::span<const ChoiceItem> tabs, const TabBarOptions& options = {});
bool TabBar(const char* id, int* index, std::span<const char* const> tabs, const TabBarOptions& options = {});
// The tab whose close button was activated in the last TabBar call of this frame, or -1.
int TabBarClosedTab();
// The move of a tab dropped in the last TabBar call of this frame (ApplyReorderMove, lists.h,
// applies it to the caller's tabs); IsValid() is false when no tab moved.
ReorderMove TabBarMovedTab();

// --- Menus ---------------------------------------------------------------------------------------

struct MenuBarOptions
{
    float width = 0.0f;                // units; 0 fills the available width
};

// A row of menu titles. Submit BeginMenu between BeginMenuBar and EndMenuBar. Once a menu is
// open, hovering another title switches to it; Alt focuses the bar, Left and Right move.
// Menus that do not fit move into a "more" menu. Returns false while the bar is out of sight;
// call EndMenuBar only after true. Unlike ImGui::BeginMenuBar it needs no window flag: it is an
// item at the cursor.
[[nodiscard]] bool BeginMenuBar(const char* id, const MenuBarOptions& options = {});
void EndMenuBar();

struct MenuOptions
{
    Icon icon;                         // before the title (in a parent menu) or instead of it (in a bar)
};

// A menu title in a menu bar, or a submenu row with a chevron inside another menu. Opens on
// click, Enter, Right (submenus) or hover in an open bar; a submenu opens to the side with room
// and stays open while the pointer moves toward it. Returns true while the menu is open: submit
// its items, then call EndMenu. Returns false while closed; call EndMenu only after true.
[[nodiscard]] bool BeginMenu(const char* label, const MenuOptions& options = {});
void EndMenu();

struct MenuItemOptions
{
    Icon icon;
    ImGuiKeyChord shortcut = 0;        // shown at the end; routing is the caller's
    ShortcutId command;                // a registered shortcut: its chord is shown instead of `shortcut`
    bool destructive = false;          // destructive tone
};

// A menu row. Returns true when it is activated (click, Enter); the menu then closes. With
// `command` set, activation also calls TriggerShortcut(command), so the shortcut's handler runs
// the action in the next frame. With a command, run the action in the ShortcutPressed handler
// only; the result reports the activation, for example to close other UI.
bool MenuItem(const char* label, const MenuItemOptions& options = {});
// A row with a check mark; activation flips *checked and returns true. A name of its own, not a
// MenuItem overload: MenuItem("Save", {}) would otherwise bind to a null bool* silently.
bool MenuCheckItem(const char* label, bool* checked, const MenuItemOptions& options = {});
// A row with a radio mark, shown while *selection == value; activation writes value and
// returns true.
bool MenuRadioItem(const char* label, int* selection, int value, const MenuItemOptions& options = {});

class [[nodiscard]] MenuBarScope
{
public:
    explicit MenuBarScope(const char* id, const MenuBarOptions& options = {}) : open_(BeginMenuBar(id, options)) {}
    ~MenuBarScope() { if (open_) EndMenuBar(); }
    MenuBarScope(const MenuBarScope&) = delete;
    MenuBarScope& operator=(const MenuBarScope&) = delete;
    explicit operator bool() const { return open_; }
private:
    bool open_;
};

class [[nodiscard]] MenuScope
{
public:
    explicit MenuScope(const char* label, const MenuOptions& options = {}) : open_(BeginMenu(label, options)) {}
    ~MenuScope() { if (open_) EndMenu(); }
    MenuScope(const MenuScope&) = delete;
    MenuScope& operator=(const MenuScope&) = delete;
    explicit operator bool() const { return open_; }
private:
    bool open_;
};

// --- Breadcrumbs and pagination ------------------------------------------------------------------

struct BreadcrumbsOptions
{
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;                // units; 0 fills; middle segments that do not fit collapse into a menu
};

// A path trail of `path` (the last entry is the current place and is not a link). Returns the
// index of the segment activated this frame (click, Enter), or -1; the caller navigates there.
int Breadcrumbs(const char* id, std::span<const ChoiceItem> path, const BreadcrumbsOptions& options = {});
int Breadcrumbs(const char* id, std::span<const char* const> path, const BreadcrumbsOptions& options = {});

struct PaginationOptions
{
    bool compact = false;              // previous, Strings::pageOf ({page} from 1, {count}), and next only
    ControlSize size = ControlSize::Inherit;
};

// Previous and next buttons around page numbers with gaps ("1 ... 4 5 6 ... 20"); a gap opens
// a field to type a page. *page is 0-based and clamped to pageCount in the drawing. Writes
// *page and returns true when another page is chosen. Left and Right move the focus.
bool Pagination(const char* id, int* page, int pageCount, const PaginationOptions& options = {});

// --- Steps ---------------------------------------------------------------------------------------

struct StepIndicatorOptions
{
    Orientation orientation = Orientation::Horizontal;  // horizontal turns vertical in a Compact width
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;                // units; 0 fills the available width
};

// Discrete steps with connectors: steps before `current` show done, `current` is marked, the
// rest are open. Not interactive.
void StepIndicator(const char* id, int current, std::span<const ChoiceItem> steps,
                   const StepIndicatorOptions& options = {});
void StepIndicator(const char* id, int current, std::span<const char* const> steps,
                   const StepIndicatorOptions& options = {});
// The same, and a done step can be activated: writes *current and returns true.
bool StepIndicator(const char* id, int* current, std::span<const ChoiceItem> steps,
                   const StepIndicatorOptions& options = {});
bool StepIndicator(const char* id, int* current, std::span<const char* const> steps,
                   const StepIndicatorOptions& options = {});

struct WizardOptions
{
    bool stepIndicator = true;         // the steps above the content
    float width = 0.0f;                // units; 0 fills the available width
};

// A multi-step flow: the step indicator, the content of step *step, which the caller submits
// and which slides when the step changes, and a footer from WizardFooter. `step` is kept by
// the caller and written by the footer and by the step indicator. EndWizard is always called.
void BeginWizard(const char* id, int* step, std::span<const ChoiceItem> steps, const WizardOptions& options = {});
void BeginWizard(const char* id, int* step, std::span<const char* const> steps, const WizardOptions& options = {});
void EndWizard();

// What the wizard footer did this frame.
enum class WizardAction
{
    None,
    Back,    // *step was decreased
    Next,    // *step was increased
    Finish,  // activated on the last step; *step is unchanged
    Cancel,
};

struct WizardFooterOptions
{
    bool canAdvance = true;            // Next and Finish are enabled
    bool cancelButton = false;         // adds a Cancel button, whose activation returns Cancel
    const char* finishLabel = nullptr; // null uses Strings::finish
};

// Back, Next (Finish on the last step) and optionally Cancel for the wizard of the enclosing
// BeginWizard. Returns the action of this frame. Only between BeginWizard and EndWizard.
WizardAction WizardFooter(const WizardFooterOptions& options = {});

class [[nodiscard]] WizardScope
{
public:
    WizardScope(const char* id, int* step, std::span<const ChoiceItem> steps, const WizardOptions& options = {})
    {
        BeginWizard(id, step, steps, options);
    }
    WizardScope(const char* id, int* step, std::span<const char* const> steps, const WizardOptions& options = {})
    {
        BeginWizard(id, step, steps, options);
    }
    ~WizardScope() { EndWizard(); }
    WizardScope(const WizardScope&) = delete;
    WizardScope& operator=(const WizardScope&) = delete;
};

// --- Shortcut sheet ------------------------------------------------------------------------------

struct ShortcutSheetOptions
{
    const char* group = nullptr;       // only shortcuts of this group; null for all
    bool searchable = true;            // a filter field above the list
};

// A modal sheet listing the registered shortcuts (core/input.h) by group, with their chords as
// key caps. Shown while *open is true; Escape, the close button or a click on the scrim write
// *open = false. A null `open` shows nothing. The visible label part is the title. Disabled
// shortcuts are listed dimmed. With no registered shortcuts in the group the sheet shows
// Strings::empty, and with none matching the filter Strings::noResults; the registry is local,
// so there is no loading or error state.
void ShortcutSheet(const char* label, bool* open, const ShortcutSheetOptions& options = {});

} // namespace facet
