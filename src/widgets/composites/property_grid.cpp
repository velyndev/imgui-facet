// The property grid, its categories and rows (facet/widgets/composites.h). An inspector built from
// finished widgets: each row is a settings row of the layout family (BeginSettingsRow), so label,
// modified mark, narrow stacking and the label tooltip behave as in every settings list; each
// category header is a Row disclosure of the containers family (BeginDisclosure), with its open
// state kept under the label; a filter with no hit shows the compact empty state of the display
// family. What the grid adds is the shared label column and its edge, which the user drags, and
// the filter across rows and categories.
//
// The label column: every row of a grid gets the same column, so their controls start at one x
// whatever the labels and however deep the category nesting indents them (a row inside a nested
// category gets a column narrowed by its indent). The column is the one the user dragged, kept
// under the grid id and saved in the ini file; before any drag, options.labelWidth or the
// labelColumn token. A drag never makes it narrower than composites.propertyLabelMinWidth nor so
// wide that a row would stack. The edge is a divider item between the label and the control
// column, over the whole grid: the resize cursor on hover, drag to move, arrows step it while it
// has the keyboard focus (Shift finer, Ctrl coarser), Home and End go to the narrowest and the
// widest column, Escape during a drag puts it back, a double click returns to the default. It is
// the last Tab stop of the grid. It is drawn only while hovered, dragged or focused, so the rows are
// not cut in two at rest. A grid whose rows stack (narrow) has no column and no edge.
//
// The filter: a row shows when the filter matches its visible label or its keywords (fuzzy, or by
// PassesFilter rules with filterSettings). Under a filter every category returns true, so its rows
// can be matched; its header shows (open, not toggled by input) while one of its rows matched in the
// last frame it was shown. With no row matching anywhere, EndPropertyGrid shows Strings::noResults.

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string_view>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/layout.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "facet/widgets/composites.h"
#include "facet/widgets/containers.h"
#include "facet/widgets/display.h"
#include "facet/widgets/layout.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/fuzzy.h"
#include "internal/item.h"
#include "internal/popup_host.h"
#include "internal/scope_stack.h"
#include "internal/state_storage.h"
#include "widgets/composites/editors_frame.h"
#include "widgets/layout/layout_internal.h"

namespace facet
{
namespace
{

constexpr int kPropertyGridDepth = 8;
constexpr int kPropertyCategoryDepth = 16;
constexpr int kPropertyRowDepth = 8;

// The column the user dragged, in design units with the gap; 0 before any drag.
struct PropertyGridColumnState
{
    static constexpr const char* kStateName = "PropertyGrid";
    float column = 0.0f;
    void WriteIni(ImGuiTextBuffer& out) const { out.appendf("column=%.3f\n", column); }
    void ReadIni(const char* key, const char* value)
    {
        if (std::strcmp(key, "column") == 0)
            column = std::max(0.0f, static_cast<float>(std::atof(value)));
    }
};

// What the grid knows across frames that is not saved.
struct PropertyGridFrameState
{
    float dragStart = 0.0f;  // the column when a drag began, for Escape
    float effective = 0.0f;  // the column used in the last frame, units
};

struct PropertyCategoryOpenState
{
    static constexpr const char* kStateName = "PropertyCategory";
    bool known = false;
    bool open = true;
    void WriteIni(ImGuiTextBuffer& out) const { out.appendf("open=%d\n", open ? 1 : 0); }
    void ReadIni(const char* key, const char* value)
    {
        if (std::strcmp(key, "open") == 0)
        {
            open = std::atoi(value) != 0;
            known = true;
        }
    }
};

// Rows of a category that matched the filter in the last frame it was shown.
struct PropertyCategoryHits
{
    int frame = -1;
    int hits = 0;      // this frame so far
    int lastHits = 0;  // the frame before
};

struct PropertyGridEntry
{
    bool active = false;
    ImGuiWindow* window = nullptr;
    ImGuiID id = 0;
    float startX = 0.0f;       // the grid's left edge (pixels)
    float top = 0.0f;
    float available = 0.0f;    // pixels from startX to the right edge at Begin
    float column = 0.0f;       // units, the label column with its gap
    float minColumn = 0.0f;    // units
    float maxColumn = 0.0f;    // units
    bool stacked = false;      // rows put their control under the label
    const char* filter = nullptr;
    const FilterSettings* filterSettings = nullptr;
    bool filtering = false;
    int rows = 0;              // rows shown this frame
    float rowsTop = 0.0f;      // pixels: the top of the first row and the bottom of the last one,
    float rowsBottom = 0.0f;   // the span of the column edge
    internal::ForcedItemRequest forced;
};

struct PropertyCategoryEntry
{
    PropertyCategoryHits* hits = nullptr;
    int grid = -1;              // index into the grid stack
    bool disclosure = false;    // BeginDisclosure returned true: EndDisclosure follows
};

struct PropertyRowEntry
{
    ImGuiWindow* window = nullptr;
    ImGuiID id = 0;
    const char* tooltip = nullptr;
    const char* label = nullptr;
    const char* labelEnd = nullptr;
};

struct PropertyGridStacks
{
    PropertyGridEntry grids[kPropertyGridDepth];
    int gridDepth = 0;
    int gridOverflow = 0;
    PropertyCategoryEntry categories[kPropertyCategoryDepth];
    int categoryDepth = 0;
    int categoryOverflow = 0;
    PropertyRowEntry rows[kPropertyRowDepth];
    int rowDepth = 0;
    int rowOverflow = 0;
    internal::PropertyGridDividerRecord divider;
    int lastRowCount = 0;
};

PropertyGridStacks& Stacks()
{
    static PropertyGridStacks stacks;
    return stacks;
}

PropertyGridEntry* CurrentGrid()
{
    PropertyGridStacks& s = Stacks();
    if (s.gridDepth == 0 || s.gridOverflow > 0)
        return nullptr;
    PropertyGridEntry& entry = s.grids[s.gridDepth - 1];
    return entry.active ? &entry : nullptr;
}

// Units of a pixel length at the current scale.
float PropertyUnits(float pixels)
{
    const float scale = Scale();
    return scale > 0.0f ? pixels / scale : pixels;
}

bool PropertyMatches(const PropertyGridEntry& grid, const char* label, const char* keywords)
{
    const std::string_view query(grid.filter);
    const char* labelEnd = ImGui::FindRenderedTextEnd(label);
    const std::string_view visible(label, static_cast<std::size_t>(labelEnd - label));
    if (grid.filterSettings != nullptr)
    {
        if (PassesFilter(query, *grid.filterSettings, visible))
            return true;
        return keywords != nullptr && keywords[0] != '\0' && PassesFilter(query, *grid.filterSettings, keywords);
    }
    if (internal::FuzzyScore(query, visible).matched)
        return true;
    return keywords != nullptr && keywords[0] != '\0' && internal::FuzzyScore(query, keywords).matched;
}

} // namespace

void BeginPropertyGrid(const char* id, const PropertyGridOptions& options)
{
    internal::PushScope("BeginPropertyGrid");
    PropertyGridStacks& s = Stacks();
    if (s.gridDepth >= kPropertyGridDepth)
    {
        ++s.gridOverflow;
        return;
    }
    PropertyGridEntry& grid = s.grids[s.gridDepth++];
    grid = PropertyGridEntry{};
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return;
    if (id == nullptr)
        id = "";

    const Theme& t = T();
    grid.window = window;
    grid.id = window->GetID(id);
    grid.forced = internal::TakeForcedItemState();
    grid.startX = window->DC.CursorPos.x;
    grid.top = window->DC.CursorPos.y;
    grid.available = std::max(0.0f, ImGui::GetContentRegionAvail().x);

    // The column: the user's drag, else the option, else the token; clamped so a row keeps its
    // control beside the label.
    const PropertyGridColumnState& saved = internal::GetPersistentState<PropertyGridColumnState>(grid.id);
    const float defaultColumn = options.labelWidth > 0.0f ? options.labelWidth : t.layout.labelColumn;
    grid.minColumn = std::max(1.0f, t.composites.propertyLabelMinWidth);
    grid.maxColumn = std::max(grid.minColumn, PropertyUnits(grid.available) - t.layout.minRowControl);
    // A dragged column is kept inside the limits of this width; the default is used as it is (a row
    // too narrow for it stacks, as every settings row does).
    grid.column = saved.column > 0.0f ? std::clamp(saved.column, grid.minColumn, grid.maxColumn) : defaultColumn;
    grid.stacked = LayoutLabeledRow(grid.available, grid.column).stacked;
    internal::GetState<PropertyGridFrameState>(grid.id).effective = grid.column;

    grid.filter = options.filter;
    grid.filterSettings = options.filterSettings;
    grid.filtering = options.filter != nullptr && std::string_view(options.filter).find_first_not_of(' ') != std::string_view::npos;
    grid.active = true;

    ImGui::BeginGroup();
    ImGui::PushOverrideID(grid.id);
}

void EndPropertyGrid()
{
    if (!internal::PopScope("BeginPropertyGrid"))
        return;
    PropertyGridStacks& s = Stacks();
    if (s.gridOverflow > 0)
    {
        --s.gridOverflow;
        return;
    }
    if (s.gridDepth == 0)
        return;
    const PropertyGridEntry grid = s.grids[--s.gridDepth];
    if (!grid.active || GImGui->CurrentWindow != grid.window)
        return;
    ImGuiWindow* window = grid.window;
    const Theme& t = T();

    // A filter that matched no row says so.
    if (grid.filtering && grid.rows == 0)
        EmptyState(GetStrings().noResults, {.compact = true});

    // The column edge, over every row of the grid, between the label and the control column.
    internal::PropertyGridDividerRecord record;
    const float bottom = grid.rowsBottom;
    if (!grid.stacked && grid.rows > 0 && bottom > grid.rowsTop)
    {
        const ImGuiID dividerId = window->GetID("##column");
        const float band = Px(t.layout.dividerBand);
        const float edge = std::round(grid.startX + Px(grid.column) - Px(t.spacing.sm) * 0.5f);
        const ImRect rect(edge - band * 0.5f, grid.rowsTop, edge + band * 0.5f, bottom);
        internal::ItemOptions itemOptions;
        itemOptions.forced = &grid.forced;
        itemOptions.part = 0;
        PropertyGridFrameState& frame = internal::GetState<PropertyGridFrameState>(grid.id);
        PropertyGridColumnState& saved = internal::GetPersistentState<PropertyGridColumnState>(grid.id);
        const float split = Px(grid.column);
        const internal::LayoutDividerInput input =
            internal::LayoutDivider(dividerId, rect, ImGuiAxis_X, split, grid.startX, itemOptions);
        float column = grid.column;
        if (input.started)
            frame.dragStart = grid.column;
        if (input.dragging)
            column = PropertyUnits(input.dragTarget);
        else if (input.canceled)
            column = frame.dragStart;
        else if (input.steps != 0)
            column += PropertyUnits(static_cast<float>(input.steps) * input.stepPixels);
        else if (input.home)
            column = grid.minColumn;
        else if (input.end)
            column = grid.maxColumn;
        if (input.reset)
        {
            saved.column = 0.0f;
            internal::MarkStateDirty();
        }
        else if (column != grid.column)
        {
            saved.column = std::clamp(column, grid.minColumn, grid.maxColumn);
            internal::MarkStateDirty();
        }
        const internal::ItemState& item = input.item;
        if (item.hovered || item.active || item.focusVisible || item.forced != internal::ForcedItemState::None)
            internal::DrawLayoutDivider(item, rect, ImGuiAxis_X);
        record.id = dividerId;
        record.band = rect;
    }
    s.divider = record;
    s.lastRowCount = grid.rows;

    ImGui::PopID();
    ImGui::EndGroup();
}

bool BeginPropertyCategory(const char* label, const PropertyCategoryOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    PropertyGridStacks& s = Stacks();
    ImGuiContext& g = *GImGui;
    const PropertyGridEntry* grid = CurrentGrid();
    const ImGuiID id = window->GetID(label);

    PropertyCategoryHits& hits = internal::GetState<PropertyCategoryHits>(ImHashStr("##hits", 0, id));
    if (hits.frame != g.FrameCount)
    {
        hits.lastHits = hits.hits;
        hits.hits = 0;
        hits.frame = g.FrameCount;
    }

    PropertyCategoryEntry entry;
    entry.hits = &hits;
    entry.grid = grid != nullptr ? s.gridDepth - 1 : -1;
    const bool filtering = grid != nullptr && grid->filtering;
    if (filtering)
    {
        // The header shows, open, while a row inside matched; the content is always submitted so
        // the rows can be matched.
        if (hits.lastHits > 0)
        {
            bool open = true;
            ImGui::PushItemFlag(ImGuiItemFlags_ReadOnly, true);
            entry.disclosure = BeginDisclosure(label, &open);
            ImGui::PopItemFlag();
        }
    }
    else
    {
        PropertyCategoryOpenState& state = internal::GetPersistentState<PropertyCategoryOpenState>(id);
        if (!state.known)
        {
            state.known = true;
            state.open = options.defaultOpen;
        }
        // Left closes and Right opens the focused header, as in a tree; Enter, Space and a click
        // toggle it through the disclosure.
        if (g.NavId == id && g.NavWindow == window)
        {
            ImGui::SetKeyOwner(ImGuiKey_LeftArrow, id);
            ImGui::SetKeyOwner(ImGuiKey_RightArrow, id);
            const bool readOnly = (g.CurrentItemFlags & static_cast<ImGuiItemFlags>(ImGuiItemFlags_ReadOnly)) != 0 ||
                                  (g.CurrentItemFlags & static_cast<ImGuiItemFlags>(ImGuiItemFlags_Disabled)) != 0;
            if (!readOnly && ImGui::IsKeyPressed(ImGuiKey_LeftArrow, ImGuiInputFlags_None, id) && state.open)
            {
                state.open = false;
                internal::MarkStateDirty();
            }
            if (!readOnly && ImGui::IsKeyPressed(ImGuiKey_RightArrow, ImGuiInputFlags_None, id) && !state.open)
            {
                state.open = true;
                internal::MarkStateDirty();
            }
        }
        const bool before = state.open;
        entry.disclosure = BeginDisclosure(label, &state.open);
        if (state.open != before)
            internal::MarkStateDirty();
        if (!entry.disclosure)
            return false;
    }

    internal::PushScope("BeginPropertyCategory");
    if (s.categoryDepth >= kPropertyCategoryDepth)
        ++s.categoryOverflow;
    else
        s.categories[s.categoryDepth++] = entry;
    return true;
}

void EndPropertyCategory()
{
    if (!internal::PopScope("BeginPropertyCategory"))
        return;
    PropertyGridStacks& s = Stacks();
    if (s.categoryOverflow > 0)
    {
        --s.categoryOverflow;
        return;
    }
    if (s.categoryDepth == 0)
        return;
    const PropertyCategoryEntry entry = s.categories[--s.categoryDepth];
    if (entry.disclosure)
        EndDisclosure();
}

bool BeginPropertyRow(const char* label, const PropertyRowOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    PropertyGridStacks& s = Stacks();
    PropertyGridEntry* grid = CurrentGrid();
    if (grid != nullptr && grid->filtering && !PropertyMatches(*grid, label, options.keywords))
        return false;

    // Count the hit for the grid and for every category around the row.
    if (grid != nullptr)
    {
        if (grid->rows++ == 0)
            grid->rowsTop = window->DC.CursorPos.y;
    }
    for (int i = 0; i < s.categoryDepth; ++i)
        if (s.categories[i].hits != nullptr && s.categories[i].grid == s.gridDepth - 1)
            ++s.categories[i].hits->hits;

    // The shared column, narrowed by the indent of nested categories so every control starts at
    // the same x.
    const Theme& t = T();
    float column = grid != nullptr ? grid->column : t.layout.labelColumn;
    if (grid != nullptr)
    {
        const float indent = PropertyUnits(std::max(0.0f, window->DC.CursorPos.x - grid->startX));
        column = std::max(t.composites.propertyLabelMinWidth * 0.5f, column - indent);
    }
    SettingsRowOptions row;
    row.modified = options.modified;
    row.labelWidth = column;
    BeginSettingsRow(label, row);

    internal::PushScope("BeginPropertyRow");
    if (s.rowDepth >= kPropertyRowDepth)
    {
        ++s.rowOverflow;
        return true;
    }
    PropertyRowEntry& entry = s.rows[s.rowDepth++];
    entry.window = window;
    entry.id = window->GetID(label);  // the row's label is on the id stack: the id of the row's tooltip
    entry.tooltip = options.tooltip;
    entry.label = label;
    entry.labelEnd = ImGui::FindRenderedTextEnd(label);
    return true;
}

void EndPropertyRow()
{
    if (!internal::PopScope("BeginPropertyRow"))
        return;
    PropertyGridStacks& s = Stacks();
    PropertyRowEntry entry;
    if (s.rowOverflow > 0)
        --s.rowOverflow;
    else if (s.rowDepth > 0)
        entry = s.rows[--s.rowDepth];
    EndSettingsRow();
    if (PropertyGridEntry* grid = CurrentGrid(); grid != nullptr && GImGui->CurrentWindow == grid->window)
        grid->rowsBottom = std::max(grid->rowsBottom, GImGui->LastItemData.Rect.Max.y);
    if (entry.window == nullptr || GImGui->CurrentWindow != entry.window || entry.tooltip == nullptr ||
        entry.tooltip[0] == '\0')
        return;

    // The tooltip of the label: on hover of the label, and while the row's control has the visible
    // keyboard focus. It names the property first, so a cut label is readable there too.
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = entry.window;
    const internal::RowLayout& layout = internal::LastSettingsRowLayout();
    const ImRect rowRect = g.LastItemData.Rect;
    const bool hovered = g.HoveredWindow == window && layout.label.Contains(g.IO.MousePos) &&
                         ImGui::IsMouseHoveringRect(layout.label.Min, layout.label.Max);
    bool focused = false;
    if (g.NavWindow == window && g.NavId != 0 && internal::KeyboardModality())
    {
        const ImRect nav = ImGui::WindowRectRelToAbs(window, window->NavRectRel[ImGuiNavLayer_Main]);
        focused = rowRect.Contains(nav.GetCenter());
    }
    if (internal::BeginPopupTooltip(entry.id, layout.label, hovered, focused))
    {
        // Both lines wrap at the host's wrap position (overlay.tooltipMaxWidth).
        PushTextStyle(TextStyle::Footnote);
        ImGui::PushStyleColor(ImGuiCol_Text, T().colors.label);
        ImGui::TextUnformatted(entry.label, entry.labelEnd);
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_Text, T().colors.labelSecondary);
        ImGui::TextUnformatted(entry.tooltip);
        ImGui::PopStyleColor();
        PopTextStyle();
        internal::EndPopupTooltip();
    }
}

namespace internal
{

float PropertyGridColumnUnits(ImGuiID gridId)
{
    return GetState<PropertyGridFrameState>(gridId).effective;
}

const PropertyGridDividerRecord& LastPropertyGridDivider()
{
    return Stacks().divider;
}

int LastPropertyGridRowCount()
{
    return Stacks().lastRowCount;
}

void ClearPropertyGridStacks()
{
    PropertyGridStacks& s = Stacks();
    s.gridDepth = 0;
    s.gridOverflow = 0;
    s.categoryDepth = 0;
    s.categoryOverflow = 0;
    s.rowDepth = 0;
    s.rowOverflow = 0;
}

} // namespace internal

} // namespace facet
