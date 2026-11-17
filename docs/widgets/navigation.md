# Navigation

The sidebar, tab bars, menu bars, menus and menu items, breadcrumbs, pagination, step indicators,
wizards and the shortcut sheet, declared in `facet/widgets/navigation.h`. Every declaration follows
[the API conventions](../api-conventions.md); the header comments are the full reference. See
[the widget index](../widgets.md) for the other families. Shortcuts are registered and routed by the
registry in `facet/core/input.h`; menu items, the shortcut sheet and the command palette show them.

A widget whose status is planned is declared and callable: it draws a dashed placeholder with its
name, returns the neutral result (false, -1, None) and never writes a value. Phase 2 implements each
widget and changes its status to ready.

## Widgets

### BeginSidebar, SidebarItem, BeginSidebarSection

Status: planned

```cpp
[[nodiscard]] bool BeginSidebar(const char* id, const SidebarOptions& options = {});
void EndSidebar();
bool SidebarItem(const char* label, Icon icon, const SidebarItemOptions& options = {});
[[nodiscard]] bool BeginSidebarSection(const char* label, const SidebarSectionOptions& options = {});
void EndSidebarSection();
class SidebarScope;
class SidebarSectionScope;
```

Vertical navigation with expanded, icon rail and hidden modes chosen by the width class, badges and
the sliding selection pill.

### TabBar

Status: planned

```cpp
bool TabBar(const char* id, int* index, std::span<const ChoiceItem> tabs, const TabBarOptions& options = {});
bool TabBar(const char* id, int* index, std::span<const char* const> tabs, const TabBarOptions& options = {});
int TabBarClosedTab();
```

A tab strip with the selection pill or an underline, closable tabs and an overflow menu; the caller
draws the content of the selected tab. Left and Right move between tabs, Ctrl+Page Down and
Ctrl+Page Up switch tabs from anywhere in the window. Ctrl+Tab is not used: Dear ImGui always claims
it for its window switcher.

### BeginMenuBar

Status: planned

```cpp
[[nodiscard]] bool BeginMenuBar(const char* id, const MenuBarOptions& options = {});
void EndMenuBar();
class MenuBarScope;
```

A row of menu titles with hover switching, Alt focus and an overflow menu.

### BeginMenu

Status: planned

```cpp
[[nodiscard]] bool BeginMenu(const char* label, const MenuOptions& options = {});
void EndMenu();
class MenuScope;
```

A menu in a menu bar or a submenu that opens to the side with room.

### MenuItem

Status: planned

```cpp
bool MenuItem(const char* label, const MenuItemOptions& options = {});
bool MenuItem(const char* label, bool* checked, const MenuItemOptions& options = {});
bool MenuItem(const char* label, int* selection, int value, const MenuItemOptions& options = {});
```

A menu row with icon, shortcut, check or radio mark, destructive tone, and an optional registered
shortcut that it triggers.

### Breadcrumbs

Status: planned

```cpp
int Breadcrumbs(const char* id, std::span<const ChoiceItem> path, const BreadcrumbsOptions& options = {});
int Breadcrumbs(const char* id, std::span<const char* const> path, const BreadcrumbsOptions& options = {});
```

A path trail that returns the activated segment; middle segments collapse into a menu when narrow.

### Pagination

Status: planned

```cpp
bool Pagination(const char* id, int* page, int pageCount, const PaginationOptions& options = {});
```

Previous and next buttons around page numbers with gaps that open a page field.

### StepIndicator

Status: planned

```cpp
void StepIndicator(const char* id, int current, std::span<const ChoiceItem> steps,
                   const StepIndicatorOptions& options = {});
void StepIndicator(const char* id, int current, std::span<const char* const> steps,
                   const StepIndicatorOptions& options = {});
bool StepIndicator(const char* id, int* current, std::span<const ChoiceItem> steps,
                   const StepIndicatorOptions& options = {});
bool StepIndicator(const char* id, int* current, std::span<const char* const> steps,
                   const StepIndicatorOptions& options = {});
```

Discrete steps with connectors; the pointer overload lets done steps be activated.

### BeginWizard, WizardFooter

Status: planned

```cpp
void BeginWizard(const char* id, int* step, std::span<const ChoiceItem> steps, const WizardOptions& options = {});
void BeginWizard(const char* id, int* step, std::span<const char* const> steps, const WizardOptions& options = {});
void EndWizard();
WizardAction WizardFooter(const WizardFooterOptions& options = {});
class WizardScope;
```

A multi-step flow with a step indicator, sliding step content and a back, next and finish footer.

### ShortcutSheet

Status: planned

```cpp
void ShortcutSheet(const char* label, bool* open, const ShortcutSheetOptions& options = {});
```

A modal sheet that lists the registered shortcuts by group with their key caps.

## Usage examples

A sidebar that drives the app's page. The app owns the selection and tells each item whether it is
the selected one; the selection pill follows it:

```cpp
static int page = 0;
if (facet::BeginSidebar("nav"))
{
    if (facet::BeginSidebarSection("Library"))
    {
        if (facet::SidebarItem("Files", facet::Icon("folder"), {.selected = page == 0}))
            page = 0;
        if (facet::SidebarItem("Downloads", facet::Icon("download"), {.selected = page == 1, .badge = pending}))
            page = 1;
        facet::EndSidebarSection();
    }
    facet::EndSidebar();
}
```

Tabs that can be closed. The tab bar reports the closed tab; the app removes it:

```cpp
if (facet::TabBar("documents", &current, tabs, {.closable = true}))
    ShowDocument(current);
if (const int closed = facet::TabBarClosedTab(); closed >= 0)
    CloseDocument(closed);
```

A menu bar whose items run registered shortcuts, so the menu, the key chord and the command
palette share one handler:

```cpp
static const facet::ShortcutId save = facet::RegisterShortcut("file.save", ImGuiMod_Ctrl | ImGuiKey_S,
                                                              {.label = "Save", .group = "File"});
if (facet::BeginMenuBar("main"))
{
    if (facet::BeginMenu("File"))
    {
        facet::MenuItem("Save", {.icon = facet::Icon("download"), .command = save});
        facet::MenuItem("Show hidden files", &showHidden);
        facet::MenuItem("Sort by name", &sortMode, 0);
        facet::MenuItem("Sort by date", &sortMode, 1);
        facet::EndMenu();
    }
    facet::EndMenuBar();
}
if (facet::ShortcutPressed(save))   // the chord, the menu item and the palette all end here
    Save();
```

Breadcrumbs return the activated segment:

```cpp
if (const int level = facet::Breadcrumbs("path", pathItems); level >= 0)
    NavigateUp(level);
```

A wizard. The footer moves `step` itself; the app only says whether the current step is complete
and handles the finish:

```cpp
static int step = 0;
facet::BeginWizard("setup", &step, steps);
DrawStep(step);
switch (facet::WizardFooter({.canAdvance = StepComplete(step), .cancelButton = true}))
{
case facet::WizardAction::Finish: ApplySetup(); break;
case facet::WizardAction::Cancel: CloseSetup(); break;
default: break;
}
facet::EndWizard();
```

## Later

No widget of this family is in the later tier.
