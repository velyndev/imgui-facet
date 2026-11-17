#pragma once

// Layout: stacks, grids, flow layout, split views, panels, list and detail, settings rows,
// dependent groups, separators and item grids.
//
// The declarations follow docs/api-conventions.md; widgets/common.h holds the types shared
// with other families, and docs/widgets/layout.md has usage examples. The family is planned:
// its widgets are declared here and draw a placeholder until Phase 2 implements them
// (docs/widgets.md).
//
// Responsive behaviour uses the width classes of core/layout.h (WidthClass, AvailableWidthClass,
// LayoutLabeledRow), so every layout switches at the same breakpoints: settings rows stack their
// control under the label in a narrow width, split views and list-detail layouts show one pane
// at a time in a Compact width, grids change their column count. Apps use the same queries for
// their own fallbacks; this header adds no second breakpoint system.
//
// When End is called (rule 8): a Begin that returns void always lays out its content, and its
// End is always called. A Begin that returns [[nodiscard]] bool may skip its content; its End
// is called only after true. Every pair has an XScope class. Null pointers, null labels and
// out-of-range values follow rule 14.

#include <span>

#include <imgui.h>

#include "facet/core/layout.h"
#include "facet/widgets/common.h"

namespace facet
{

// Gaps between the items of a layout container, from the spacing tokens.
enum class Gap
{
    Item,     // spacing.sm, the gap between neighbouring controls
    None,     // no gap
    Tight,    // spacing.xs, for parts of one control group
    Loose,    // spacing.lg, between groups
    Section,  // spacing.xxl, between sections of a page
};

// Where items sit on the cross axis of a stack, or along the line of a flow layout.
enum class LayoutAlign
{
    Start,
    Center,
    End,
    Stretch,  // items take the full cross size (in a vertical stack: the item width is the stack width)
};

// --- Stack ---------------------------------------------------------------------------------------

struct StackOptions
{
    Orientation orientation = Orientation::Vertical;  // Vertical: a column; Horizontal: a row
    Gap gap = Gap::Item;               // between neighbouring items
    LayoutAlign align = LayoutAlign::Start;   // on the cross axis
    float width = 0.0f;                // units; 0 fills (vertical) or fits the items (horizontal)
    float height = 0.0f;               // units; 0 fits the items
};

// Lays out the items submitted until EndStack in a row or a column with the gap between them.
// StackSpacer takes the free space, so items after it move to the end; centering and spacers
// use the item sizes of the last frame, kept under the id. EndStack is always called; the
// stack is the last item after it.
void BeginStack(const char* id, const StackOptions& options = {});
// Flexible space inside a stack; several spacers share the free space equally.
void StackSpacer();
void EndStack();

class [[nodiscard]] StackScope
{
public:
    explicit StackScope(const char* id, const StackOptions& options = {}) { BeginStack(id, options); }
    ~StackScope() { EndStack(); }
    StackScope(const StackScope&) = delete;
    StackScope& operator=(const StackScope&) = delete;
};

// --- Grid ----------------------------------------------------------------------------------------

struct GridOptions
{
    float minCellWidth = 0.0f;         // units; columns are added while each stays this wide; 0 for layout.minRowControl
    int maxColumns = 0;                // 0 for no limit
    Gap gap = Gap::Item;               // between columns and between rows
    float width = 0.0f;                // units; 0 fills the available width
};

// Equal columns that reflow with the width: as many as fit minCellWidth, at least one. Start
// each cell with GridNext; cells fill rows from the leading edge, and a row is as tall as its
// tallest cell. EndGrid is always called. Arrow keys move between cells through Dear ImGui's
// spatial navigation. Row heights of the last frame are kept under the id.
void BeginGrid(const char* id, const GridOptions& options = {});

struct GridCellOptions
{
    int columnSpan = 1;                // columns the cell covers; clamped to the column count
};

// Starts the next cell.
void GridNext(const GridCellOptions& options = {});
void EndGrid();

class [[nodiscard]] GridScope
{
public:
    explicit GridScope(const char* id, const GridOptions& options = {}) { BeginGrid(id, options); }
    ~GridScope() { EndGrid(); }
    GridScope(const GridScope&) = delete;
    GridScope& operator=(const GridScope&) = delete;
};

// --- Flow ----------------------------------------------------------------------------------------

struct FlowOptions
{
    Gap gap = Gap::Item;               // between items and between lines
    LayoutAlign align = LayoutAlign::Start;    // of each line; Stretch spreads the items
    int maxLines = 0;                  // 0 for no limit; further items are counted in a "+N" chip
    float width = 0.0f;                // units; 0 fills the available width
};

// Items of different widths that wrap onto new lines, such as chips and tags. Call FlowNext
// before each item: it places the cursor and returns false when the item does not fit within
// maxLines; skip that item then. EndFlow draws the "+N" chip for the skipped items and is always
// called. Item widths of the last frame are kept under the id.
void BeginFlow(const char* id, const FlowOptions& options = {});
[[nodiscard]] bool FlowNext();
void EndFlow();

class [[nodiscard]] FlowScope
{
public:
    explicit FlowScope(const char* id, const FlowOptions& options = {}) { BeginFlow(id, options); }
    ~FlowScope() { EndFlow(); }
    FlowScope(const FlowScope&) = delete;
    FlowScope& operator=(const FlowScope&) = delete;
};

// --- Split view ----------------------------------------------------------------------------------

struct SplitViewOptions
{
    Orientation orientation = Orientation::Horizontal;  // Horizontal: panes side by side
    float minFirst = 0.0f;             // units; smallest first pane; 0 for layout.minRowControl
    float minSecond = 0.0f;            // units; smallest second pane; 0 for layout.minRowControl
    bool collapsible = false;          // dragging past a minimum collapses that pane; Enter on the divider toggles
    float width = 0.0f;                // units; 0 fills the available width
    float height = 0.0f;               // units; 0 fills the available height
};

// Two panes with a divider between them. *ratio is the first pane's share of the room (0 to 1),
// written while the divider is dragged or moved with the arrow keys (it is focusable); a
// collapsed pane writes 0 or 1. The first pane's content follows BeginSplitView, SplitViewNext
// starts the second, and EndSplitView is always called. In a Compact width a horizontal split
// shows the panes one above the other. Each pane is a child window, so its content keeps ids
// under the pane.
void BeginSplitView(const char* id, float* ratio, const SplitViewOptions& options = {});
void SplitViewNext();
void EndSplitView();

class [[nodiscard]] SplitViewScope
{
public:
    SplitViewScope(const char* id, float* ratio, const SplitViewOptions& options = {})
    {
        BeginSplitView(id, ratio, options);
    }
    ~SplitViewScope() { EndSplitView(); }
    SplitViewScope(const SplitViewScope&) = delete;
    SplitViewScope& operator=(const SplitViewScope&) = delete;
};

// --- Panels --------------------------------------------------------------------------------------

struct PanelsOptions
{
    Orientation orientation = Orientation::Horizontal;  // Horizontal: panels side by side
    float width = 0.0f;                // units; 0 fills the available width
    float height = 0.0f;               // units; 0 fills the available height
};

// N resizable panels with headers. `sizes` holds one share per panel (they are normalized to a
// sum of 1); a divider drag moves room only between its two neighbours and writes their entries.
// Submit each panel with BeginPanel, in order. EndPanels is always called. In a Compact width
// horizontal panels stack.
void BeginPanels(const char* id, std::span<float> sizes, const PanelsOptions& options = {});
void EndPanels();

struct PanelOptions
{
    Icon icon;                         // before the title in the header
    bool collapsible = true;           // the header collapses the panel to a strip
};

// The next panel; the visible label part is its header title. Returns true while the panel is
// expanded: submit its content and call EndPanel. Returns false while collapsed (its strip is
// drawn); call EndPanel only after true. The collapsed state is kept under the id.
[[nodiscard]] bool BeginPanel(const char* label, const PanelOptions& options = {});
void EndPanel();

class [[nodiscard]] PanelsScope
{
public:
    PanelsScope(const char* id, std::span<float> sizes, const PanelsOptions& options = {})
    {
        BeginPanels(id, sizes, options);
    }
    ~PanelsScope() { EndPanels(); }
    PanelsScope(const PanelsScope&) = delete;
    PanelsScope& operator=(const PanelsScope&) = delete;
};

class [[nodiscard]] PanelScope
{
public:
    explicit PanelScope(const char* label, const PanelOptions& options = {}) : open_(BeginPanel(label, options)) {}
    ~PanelScope() { if (open_) EndPanel(); }
    PanelScope(const PanelScope&) = delete;
    PanelScope& operator=(const PanelScope&) = delete;
    explicit operator bool() const { return open_; }
private:
    bool open_;
};

// --- List and detail -----------------------------------------------------------------------------

struct ListDetailOptions
{
    float listWidth = 0.0f;            // units; width of the list pane beside the detail; 0 for layout.labelColumn + layout.minRowControl
    float height = 0.0f;               // units; 0 fills the available height
};

// A list pane and a detail pane. In a Regular or Expanded width both are shown side by side;
// in a Compact width only one is: the list while *detailOpen is false, the detail (with a back
// button that writes *detailOpen = false) while it is true. The caller writes *detailOpen =
// true when an entry is chosen. Submit the panes with BeginListPane and BeginDetailPane, in that
// order; EndListDetail is always called.
void BeginListDetail(const char* id, bool* detailOpen, const ListDetailOptions& options = {});
// Each returns false while its pane is hidden; call its End only after true.
[[nodiscard]] bool BeginListPane();
void EndListPane();
[[nodiscard]] bool BeginDetailPane();
void EndDetailPane();
void EndListDetail();

class [[nodiscard]] ListDetailScope
{
public:
    ListDetailScope(const char* id, bool* detailOpen, const ListDetailOptions& options = {})
    {
        BeginListDetail(id, detailOpen, options);
    }
    ~ListDetailScope() { EndListDetail(); }
    ListDetailScope(const ListDetailScope&) = delete;
    ListDetailScope& operator=(const ListDetailScope&) = delete;
};

class [[nodiscard]] ListPaneScope
{
public:
    ListPaneScope() : open_(BeginListPane()) {}
    ~ListPaneScope() { if (open_) EndListPane(); }
    ListPaneScope(const ListPaneScope&) = delete;
    ListPaneScope& operator=(const ListPaneScope&) = delete;
    explicit operator bool() const { return open_; }
private:
    bool open_;
};

class [[nodiscard]] DetailPaneScope
{
public:
    DetailPaneScope() : open_(BeginDetailPane()) {}
    ~DetailPaneScope() { if (open_) EndDetailPane(); }
    DetailPaneScope(const DetailPaneScope&) = delete;
    DetailPaneScope& operator=(const DetailPaneScope&) = delete;
    explicit operator bool() const { return open_; }
private:
    bool open_;
};

// --- Settings row --------------------------------------------------------------------------------

struct SettingsRowOptions
{
    const char* description = nullptr; // second line under the label in labelSecondary; null for none
    Icon icon;                         // before the label
    bool modified = false;             // marks a value that differs from its default
    bool resetButton = false;          // with modified: a reset button at the end; read IsSettingsRowResetPressed
    float labelWidth = 0.0f;           // units; label column with its gap; 0 for layout.labelColumn
};

// A label column and a control column within the available width (also in a table cell),
// laid out by LayoutLabeledRow: the control follows the label, or sits under it when the row is
// too narrow. Pushes the label on the id stack until EndSettingsRow, so the control can use
// "##value". EndSettingsRow is always called; the row is the last item after it.
void BeginSettingsRow(const char* label, const SettingsRowOptions& options = {});
void EndSettingsRow();

// Whether the reset button of the last EndSettingsRow of this frame was activated.
bool IsSettingsRowResetPressed();

class [[nodiscard]] SettingsRowScope
{
public:
    explicit SettingsRowScope(const char* label, const SettingsRowOptions& options = {})
    {
        BeginSettingsRow(label, options);
    }
    ~SettingsRowScope() { EndSettingsRow(); }
    SettingsRowScope(const SettingsRowScope&) = delete;
    SettingsRowScope& operator=(const SettingsRowScope&) = delete;
};

// --- Dependent group -----------------------------------------------------------------------------

// What a dependent group does while its parent setting is off.
enum class DependentMode
{
    Disable,  // the content stays visible and is disabled
    Hide,     // the content collapses with an animated height
};

struct DependentGroupOptions
{
    DependentMode mode = DependentMode::Disable;  // what happens while the parent is off
    bool indent = true;                // the content is indented by spacing.lg
};

// Settings that only apply while a parent setting is on. `enabled` is the parent's state, read
// during the call. Returns true while the content is shown (always in Disable mode, which then
// disables it like ImGui::BeginDisabled); call EndDependentGroup only after true. The collapse
// animation state is kept under the id.
[[nodiscard]] bool BeginDependentGroup(const char* id, bool enabled, const DependentGroupOptions& options = {});
void EndDependentGroup();

class [[nodiscard]] DependentGroupScope
{
public:
    DependentGroupScope(const char* id, bool enabled, const DependentGroupOptions& options = {})
        : open_(BeginDependentGroup(id, enabled, options)) {}
    ~DependentGroupScope() { if (open_) EndDependentGroup(); }
    DependentGroupScope(const DependentGroupScope&) = delete;
    DependentGroupScope& operator=(const DependentGroupScope&) = delete;
    explicit operator bool() const { return open_; }
private:
    bool open_;
};

// --- Separators ----------------------------------------------------------------------------------

struct SeparatorOptions
{
    Orientation orientation = Orientation::Horizontal;  // Vertical: between items on one line, the line's height
    bool opaque = false;               // separatorOpaque instead of the translucent separator color
};

// A hairline divider snapped to whole pixels, across the available width (horizontal) or the
// current line height (vertical). Not an interactive item.
void Separator(const SeparatorOptions& options = {});

struct SeparatorTextOptions
{
    bool centered = false;             // the text sits in the middle of the line instead of at its start
};

// A horizontal hairline with `text` (read during the call) in Footnote labelSecondary.
void SeparatorText(const char* text, const SeparatorTextOptions& options = {});

// --- Item grid -----------------------------------------------------------------------------------

struct ItemGridOptions
{
    float minTileWidth = 0.0f;         // units; tiles are at least this wide; 0 for layout.minRowControl
    float tileAspect = 1.0f;           // tile height / tile width
    Gap gap = Gap::Item;               // between tiles, in both directions
    bool boxSelect = true;             // a drag that starts between tiles selects the tiles it covers
    DataState state = DataState::Ready;
    const char* emptyText = nullptr;   // null uses Strings::empty
    const char* errorText = nullptr;   // null uses Strings::loadFailed
    float width = 0.0f;                // units; 0 fills the available width
    float height = 0.0f;               // units; 0 fills the available height
};

// A scrolling grid of itemCount equal tiles that reflows with the width and submits only the
// visible tiles, for asset libraries and inventories. With `selection` null the tiles are not
// selectable; otherwise click, Ctrl, Shift, box select and arrow keys in two dimensions write
// it through ImGui::BeginMultiSelect. Returns false while hidden, not Ready, or empty (the state
// is drawn instead); call EndItemGrid only after true. Inside, loop NextVisibleRows (its range
// holds tile indices) and start each tile with ItemGridTile. The scroll position is kept under
// the id.
[[nodiscard]] bool BeginItemGrid(const char* id, int itemCount, ImGuiSelectionBasicStorage* selection,
                                 const ItemGridOptions& options = {});
// Starts tile `index`: a tile item (selection, hover, activation) whose area the caller fills.
// Returns true when the tile was activated (double click, Enter).
bool ItemGridTile(int index);
void EndItemGrid();

class [[nodiscard]] ItemGridScope
{
public:
    ItemGridScope(const char* id, int itemCount, ImGuiSelectionBasicStorage* selection,
                  const ItemGridOptions& options = {})
        : open_(BeginItemGrid(id, itemCount, selection, options)) {}
    ~ItemGridScope() { if (open_) EndItemGrid(); }
    ItemGridScope(const ItemGridScope&) = delete;
    ItemGridScope& operator=(const ItemGridScope&) = delete;
    explicit operator bool() const { return open_; }
private:
    bool open_;
};

} // namespace facet
