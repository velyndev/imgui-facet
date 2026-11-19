// List and selection core (list_core.h): row ranges over ImGuiListClipper and the selection of
// lists, grids and tables over ImGui::BeginMultiSelect and ImGuiSelectionBasicStorage. The views
// and trees built on them are in list_view.cpp.
//
// Row ranges: one clipper per open BeginListRows in a fixed stack, so nothing is allocated per
// frame (Dear ImGui keeps the clipper's range buffers between frames). The clipper hands out the
// visible lines plus, by itself, the line of the keyboard focus (when it was submitted in this
// window), the lines a navigation request scores and the box selection; BeginListRows adds
// keepAlive and the range start of an open selection in the same window, so Shift ranges and the
// focus survive scrolling. Ranges come sorted and fused, so no entry is handed out twice.

#include "internal/list_core.h"

#include <algorithm>
#include <cstdint>

#include "core/core_internal.h"
#include "internal/list_internal.h"

namespace facet::internal
{
namespace
{

constexpr int kListSelectionDepth = 4;

struct RowsScope
{
    ImGuiListClipper clipper;
    ImGuiID window = 0;
    int count = 0;
    int columns = 1;
    bool stepping = false;  // the clipper is between Begin and its end
};

struct SelectionScope
{
    ListSelection* storage = nullptr;
    ImGuiMultiSelectIO* io = nullptr;  // null: an inert scope (no storage)
    ImGuiID window = 0;
    ListSelectionOptions options;
    int itemCount = 0;
    ListEntryFn entry = nullptr;
    void* user = nullptr;
    bool fingerprinted = false;
    std::uint64_t before = 0;
};

struct ListCore
{
    RowsScope rows[kListRowsDepth];
    int rowsDepth = 0;
    int rowsOverflow = 0;
    SelectionScope selections[kListSelectionDepth];
    int selectionDepth = 0;
    int selectionOverflow = 0;
    ImVector<ImGuiSelectionRequest> scratch;  // filtered requests, kept between frames
};

ListCore& Core()
{
    static ListCore core;
    return core;
}

} // namespace

std::uint64_t ListSelectionFingerprint(const ListSelection& selection)
{
    std::uint64_t sum = 0;
    for (const ImGuiStoragePair& pair : selection._Storage.Data)
    {
        if (pair.val_i == 0)
            continue;
        std::uint64_t x = pair.key * 0x9E3779B97F4A7C15ull;
        x ^= x >> 29;
        sum += x * 0xBF58476D1CE4E5B9ull;
    }
    return sum ^ (static_cast<std::uint64_t>(selection.Size) << 40);
}

namespace
{

// Adds the enabled runs of [first, last] as SetRange requests of `selected`.
void PushEnabledRuns(ImVector<ImGuiSelectionRequest>& out, const ImGuiSelectionRequest& request, int first, int last,
                     bool selected, ListEntryFn entry, void* user, bool wantEnabled)
{
    int runStart = -1;
    for (int i = first; i <= last + 1; ++i)
    {
        bool take = false;
        if (i <= last)
        {
            ListEntryInfo info;
            entry(user, i, &info);
            take = info.disabled != wantEnabled;
        }
        if (take && runStart < 0)
            runStart = i;
        if (!take && runStart >= 0)
        {
            ImGuiSelectionRequest run = request;
            run.Type = ImGuiSelectionRequestType_SetRange;
            run.Selected = selected;
            run.RangeFirstItem = runStart;
            run.RangeLastItem = i - 1;
            out.push_back(run);
            runStart = -1;
        }
    }
}

// Rewrites the requests so that disabled entries are never selected by a range or select-all.
void FilterRequests(ImGuiMultiSelectIO* io, int count, ListEntryFn entry, void* user)
{
    ImVector<ImGuiSelectionRequest>& out = Core().scratch;
    out.resize(0);
    for (const ImGuiSelectionRequest& request : io->Requests)
    {
        if (request.Type == ImGuiSelectionRequestType_SetAll && request.Selected && count > 0)
        {
            out.push_back(request);
            PushEnabledRuns(out, request, 0, count - 1, false, entry, user, false);
        }
        else if (request.Type == ImGuiSelectionRequestType_SetRange && request.Selected)
        {
            const ImS64 low = std::min(request.RangeFirstItem, request.RangeLastItem);
            const ImS64 high = std::max(request.RangeFirstItem, request.RangeLastItem);
            const int first = static_cast<int>(std::max<ImS64>(0, low));
            const int last = static_cast<int>(std::min<ImS64>(count - 1, high));
            if (first <= last)
                PushEnabledRuns(out, request, first, last, true, entry, user, true);
        }
        else
        {
            out.push_back(request);
        }
    }
    io->Requests.swap(out);
}

void ApplyRequests(SelectionScope& scope, ImGuiMultiSelectIO* io)
{
    if (io == nullptr || scope.storage == nullptr || scope.options.readOnly || io->Requests.Size == 0)
        return;
    if (!scope.fingerprinted)
    {
        scope.before = ListSelectionFingerprint(*scope.storage);
        scope.fingerprinted = true;
    }
    ApplyListRequests(scope.storage, io, scope.itemCount, scope.entry, scope.user);
}

void OpenSelection(ListSelection* selection, int itemCount, const ListSelectionOptions& options, ListEntryFn entry,
                   void* user)
{
    ListCore& core = Core();
    if (core.selectionDepth >= kListSelectionDepth)
    {
        if (core.selectionOverflow == 0)
            ReportUsageError("BeginListSelection nested deeper than %d; the selection is ignored", kListSelectionDepth);
        ++core.selectionOverflow;
        return;
    }
    SelectionScope& scope = core.selections[core.selectionDepth++];
    scope = SelectionScope();
    ImGuiContext* context = ImGui::GetCurrentContext();
    if (selection == nullptr || context == nullptr || context->CurrentWindow == nullptr)
        return;
    scope.storage = selection;
    scope.window = context->CurrentWindow->ID;
    scope.options = options;
    scope.itemCount = std::max(0, itemCount);
    scope.entry = entry;
    scope.user = user;
    ImGuiMultiSelectFlags flags = ImGuiMultiSelectFlags_ClearOnEscape | ImGuiMultiSelectFlags_ClearOnClickVoid;
    if (options.single)
        flags |= ImGuiMultiSelectFlags_SingleSelect;
    else if (options.boxSelect)
        flags |= options.grid ? ImGuiMultiSelectFlags_BoxSelect2d : ImGuiMultiSelectFlags_BoxSelect1d;
    if (options.grid)
        flags |= ImGuiMultiSelectFlags_NavWrapX;
    scope.io = ImGui::BeginMultiSelect(flags, selection->Size, scope.itemCount);
    // With a clipper, requests from Begin (select all, clear) are applied before the rows.
    ApplyRequests(scope, scope.io);
}

} // namespace

void BeginListRows(const ListRowsDesc& desc)
{
    ListCore& core = Core();
    ImGuiContext* context = ImGui::GetCurrentContext();
    if (core.rowsDepth >= kListRowsDepth || context == nullptr || context->CurrentWindow == nullptr)
    {
        if (core.rowsDepth >= kListRowsDepth && core.rowsOverflow == 0)
            ReportUsageError("BeginListRows nested deeper than %d; the rows are not handed out", kListRowsDepth);
        ++core.rowsOverflow;
        return;
    }
    RowsScope& scope = core.rows[core.rowsDepth++];
    ImGuiWindow* window = context->CurrentWindow;
    scope.window = window->ID;
    scope.count = std::max(0, desc.rowCount);
    scope.columns = std::max(1, desc.columns);
    scope.stepping = false;
    if (!(desc.rowHeight > 0.0f))
    {
        ReportUsageError("BeginListRows needs a row height above 0; the rows are not handed out");
        return;
    }
    const int lines = (scope.count + scope.columns - 1) / scope.columns;
    scope.clipper.Begin(lines, desc.rowHeight);
    scope.stepping = true;
    if (desc.keepAlive >= 0 && desc.keepAlive < scope.count)
        scope.clipper.IncludeItemByIndex(desc.keepAlive / scope.columns);
    // The range start of a selection opened in this window (index keyed), so Shift ranges that
    // start out of view still work.
    if (core.selectionDepth > 0)
    {
        const SelectionScope& selection = core.selections[core.selectionDepth - 1];
        if (selection.io != nullptr && selection.window == window->ID)
        {
            const ImS64 source = selection.io->RangeSrcItem;
            if (source >= 0 && source < scope.count)
                scope.clipper.IncludeItemByIndex(static_cast<int>(source) / scope.columns);
        }
    }
}

bool NextListRows(RowRange* rows)
{
    if (rows != nullptr)
        *rows = {};
    ListCore& core = Core();
    if (core.rowsOverflow > 0 || core.rowsDepth == 0)
        return false;
    RowsScope& scope = core.rows[core.rowsDepth - 1];
    if (!scope.stepping)
        return false;
    const ImGuiContext* context = ImGui::GetCurrentContext();
    if (context == nullptr || context->CurrentWindow == nullptr || context->CurrentWindow->ID != scope.window)
        return false;
    while (scope.clipper.Step())
    {
        const int first = scope.clipper.DisplayStart * scope.columns;
        const int last = std::min(scope.clipper.DisplayEnd * scope.columns, scope.count);
        if (first >= last)
            continue;
        if (rows != nullptr)
            *rows = {first, last};
        return true;
    }
    scope.stepping = false;  // Step() ended the clipper and laid out the rest
    return false;
}

void EndListRows()
{
    ListCore& core = Core();
    if (core.rowsOverflow > 0)
    {
        --core.rowsOverflow;
        return;
    }
    if (core.rowsDepth == 0)
    {
        ReportUsageError("EndListRows without BeginListRows");
        return;
    }
    RowsScope& scope = core.rows[--core.rowsDepth];
    if (!scope.stepping)
        return;
    scope.stepping = false;
    const ImGuiContext* context = ImGui::GetCurrentContext();
    if (context == nullptr || context->CurrentWindow == nullptr || context->CurrentWindow->ID != scope.window)
    {
        // Wrong window: close the clipper without moving any cursor.
        scope.clipper.ItemsCount = -1;
        scope.clipper.End();
        return;
    }
    // A loop that stopped early (or never ran) still lays out the height of every entry.
    if (scope.clipper.DisplayStart < 0 && scope.clipper.ItemsCount > 0)
        scope.clipper.SeekCursorForItem(scope.clipper.ItemsCount);
    scope.clipper.End();
}

void ApplyListRequests(ListSelection* selection, ImGuiMultiSelectIO* io, int itemCount, ListEntryFn entry, void* user)
{
    if (selection == nullptr || io == nullptr || io->Requests.Size == 0)
        return;
    if (entry == nullptr)
    {
        selection->ApplyRequests(io);
        return;
    }
    FilterRequests(io, itemCount, entry, user);
    selection->ApplyRequests(io);
    io->Requests.swap(Core().scratch);  // give the caller its own buffer back
}

void BeginListSelection(ListSelection* selection, int itemCount, const ListSelectionOptions& options)
{
    OpenSelection(selection, itemCount, options, nullptr, nullptr);
}

void BeginListSelectionFiltered(ListSelection* selection, int itemCount, const ListSelectionOptions& options,
                                ListEntryFn entry, void* user)
{
    OpenSelection(selection, itemCount, options, entry, user);
}

bool EndListSelection()
{
    ListCore& core = Core();
    if (core.selectionOverflow > 0)
    {
        --core.selectionOverflow;
        return false;
    }
    if (core.selectionDepth == 0)
    {
        ReportUsageError("EndListSelection without BeginListSelection");
        return false;
    }
    SelectionScope& scope = core.selections[--core.selectionDepth];
    if (scope.io == nullptr)
        return false;
    const ImGuiContext* context = ImGui::GetCurrentContext();
    if (context == nullptr || context->CurrentWindow == nullptr || context->CurrentWindow->ID != scope.window)
    {
        ReportUsageError("EndListSelection called in another window than its BeginListSelection");
        return false;
    }
    ApplyRequests(scope, ImGui::EndMultiSelect());
    return scope.fingerprinted && ListSelectionFingerprint(*scope.storage) != scope.before;
}

bool IsListEntrySelected(ListSelection* selection, int index)
{
    if (selection == nullptr || index < 0)
        return false;
    return selection->Contains(selection->GetStorageIdFromIndex(index));
}

void ListRowsEndFrame()
{
    ListCore& core = Core();
    const int rows = core.rowsDepth + core.rowsOverflow;
    if (rows > 0)
        ReportUsageError("BeginListRows without EndListRows: %d range(s) still open at EndFrame", rows);
    for (int i = 0; i < core.rowsDepth; ++i)
    {
        RowsScope& scope = core.rows[i];
        if (scope.stepping)
        {
            scope.clipper.ItemsCount = -1;  // no cursor to move outside a window
            scope.clipper.End();
            scope.stepping = false;
        }
    }
    core.rowsDepth = 0;
    core.rowsOverflow = 0;
    const int selections = core.selectionDepth + core.selectionOverflow;
    if (selections > 0)
        ReportUsageError("BeginListSelection without EndListSelection: %d selection(s) still open at EndFrame",
                         selections);
    core.selectionDepth = 0;
    core.selectionOverflow = 0;
}

void ListRowsShutdown()
{
    ListCore& core = Core();
    for (RowsScope& scope : core.rows)
    {
        // The context the clippers belonged to may be gone: forget their temporary data.
        scope.clipper.TempData = nullptr;
        scope.clipper.ItemsCount = -1;
        scope.stepping = false;
    }
    core.rowsDepth = 0;
    core.rowsOverflow = 0;
    core.selectionDepth = 0;
    core.selectionOverflow = 0;
}

} // namespace facet::internal
