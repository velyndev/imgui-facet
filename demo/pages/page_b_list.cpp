// Demo page: List core. Prototype lists on the list view and tree of the list core
// (src/internal/list_view.h): a virtual list of one million generated rows, a multi-selection
// with disabled rows, a sorted and filtered list that keeps its selection by id, a single choice
// on the selection pill with typeahead over the names of the built-in icons, a tree of this
// gallery's own pages with a lazily loaded node, and the loading, error and empty states.
// Placeholder entries are labelled as placeholders and generated rows as generated.

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <iterator>
#include <string>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "gallery.h"
#include "internal/focus.h"
#include "internal/list_view.h"
#include "internal/scroll.h"
#include "internal/selection_pill.h"
#include "internal/text_util.h"
#include "pages/pages.h"

namespace
{

namespace fi = facet::internal;
using facet::demo::ForcedItemState;
using facet::demo::Note;
using facet::demo::SectionHeader;

enum CaptureState
{
    kDefault,
    kHovered,
    kFocused,
    kLower,
};

constexpr int kVirtualRows = 1000000;
constexpr int kMultiRows = 12;
constexpr int kSortedRows = 30;
constexpr int kLazyChildren = 3;

struct ListPageState
{
    ImGuiSelectionBasicStorage virtualSelection;
    ImGuiSelectionBasicStorage multiSelection;
    ImGuiSelectionBasicStorage sortedSelection;
    ImGuiSelectionBasicStorage treeSelection;
    std::vector<ImGuiID> sortedView;  // ids in display order
    int sort = 0;                     // 0 ascending, 1 descending
    int filter = 0;                   // 0 all, 1 odd ids, 2 even ids
    int choice = -1;                  // index into the icon registry
    int virtualFocus = -1;
    int virtualSubmitted = 0;
    double virtualMicros = 0.0;
    int virtualActivated = -1;
    int multiActivated = -1;
    bool lazyOpen = false;
    float lazyWait = 0.0f;            // frame time the simulated load has waited
    bool lazyLoaded = false;
    int treeActivated = 0;
    bool initialized = false;
    int preparedState = -1;  // the capture state the selection was last prepared for
    bool revealTop = false;  // every list scrolls back to its first row in the next frame
};

ListPageState page;

void Reset()
{
    page.virtualSelection.Clear();
    page.multiSelection.Clear();
    page.sortedSelection.Clear();
    page.treeSelection.Clear();
    page.sort = 0;
    page.filter = 0;
    page.choice = -1;
    page.virtualActivated = -1;
    page.multiActivated = -1;
    page.lazyOpen = false;
    page.lazyWait = 0.0f;
    page.lazyLoaded = false;
    page.treeActivated = 0;
    page.revealTop = true;
}

// ---- Entries --------------------------------------------------------------------------------------

void VirtualEntry(void*, int index, fi::ListEntryInfo* out)
{
    static char label[32];
    std::snprintf(label, sizeof(label), "Generated row %07d", index + 1);
    out->label = label;
}

bool MultiDisabled(int index)
{
    return index == 2 || index == 10;
}

void MultiEntry(void*, int index, fi::ListEntryInfo* out)
{
    static char label[48];
    std::snprintf(label, sizeof(label), MultiDisabled(index) ? "Placeholder entry %d (disabled)" : "Placeholder entry %d",
                  index + 1);
    out->label = label;
    out->disabled = MultiDisabled(index);
}

void SortedEntry(void*, int index, fi::ListEntryInfo* out)
{
    static char label[32];
    const ImGuiID id = page.sortedView[static_cast<size_t>(index)];
    std::snprintf(label, sizeof(label), "Entry %02u", id);
    out->label = label;
    out->id = id;
}

void IconEntry(void*, int index, fi::ListEntryInfo* out)
{
    out->label = facet::IconNameAt(index);
}

void RebuildSortedView()
{
    page.sortedView.clear();
    for (ImGuiID id = 1; id <= kSortedRows; ++id)
    {
        if ((page.filter == 1 && id % 2 == 0) || (page.filter == 2 && id % 2 == 1))
            continue;
        page.sortedView.push_back(id);
    }
    if (page.sort == 1)
        std::reverse(page.sortedView.begin(), page.sortedView.end());
}

// ---- Drawing ------------------------------------------------------------------------------------

// The label of a prototype row: semibold when selected, labelTertiary when disabled, cut with an
// ellipsis at the end of the row, after an optional icon.
void RowLabel(const fi::ListRowState& row, const char* text, facet::Icon icon = facet::Icon())
{
    const facet::Theme& t = facet::T();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    if (!row.item.visible)
        return;
    fi::PushPillLabelStyle(facet::TextStyle::Body, row.selected);
    float x = row.content.Min.x;
    const ImU32 color = facet::ToU32(row.disabled ? t.colors.labelTertiary : t.colors.label);
    if (icon.IsValid())
    {
        const float size = facet::Px(t.spacing.lg);
        facet::DrawIcon(drawList, icon, ImVec2(x, std::floor(row.rect.GetCenter().y - size * 0.5f)), size,
                        facet::ToU32(row.disabled ? t.colors.labelTertiary : t.colors.labelSecondary));
        x += size + facet::Px(t.spacing.sm);
    }
    char shown[128];
    fi::TruncateText(ImGui::GetFont(), ImGui::GetFontSize(), text, nullptr, std::max(0.0f, row.content.Max.x - x),
                     fi::TruncateMode::End, shown, sizeof(shown));
    drawList->AddText(ImVec2(x, facet::CenteredTextY(row.rect.Min.y, row.rect.GetHeight())), color, shown);
    facet::PopTextStyle();
}

// A card of bgSecondary at the cursor; returns the rect of the list inside it.
ImRect Card(float width, float height)
{
    const facet::Theme& t = facet::T();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    facet::FillRect(drawList, pos, ImVec2(pos.x + width, pos.y + height), facet::ToU32(t.colors.bgSecondary),
                    facet::Px(t.radii.card));
    const float inset = facet::Px(t.spacing.xs);
    return ImRect(pos.x + inset, pos.y + inset, pos.x + width - inset, pos.y + height - inset);
}

void EndCard(const ImRect& inner)
{
    const float inset = facet::Px(facet::T().spacing.xs);
    ImGui::SetCursorScreenPos(ImVec2(inner.Min.x - inset, inner.Max.y + inset));
    ImGui::Dummy(ImVec2(inner.GetWidth() + inset * 2.0f, 0.0f));
}

// The forced looks of the capture states land on row `row` of the next list.
void ForceRow(facet::demo::PageCtx& ctx, int row)
{
    if (ctx.capture && ctx.captureState == kHovered)
        ctx.ForceNextItemState(ForcedItemState::Hovered, row);
}

// ---- Sections -----------------------------------------------------------------------------------

void VirtualList(facet::demo::PageCtx& ctx)
{
    const facet::Theme& t = facet::T();
    const ImRect inner = Card(ImGui::GetContentRegionAvail().x, facet::Px(t.metrics.panelHeight * 1.25f));
    ImGui::SetCursorScreenPos(inner.Min);
    fi::ListViewDesc desc;
    desc.rowCount = kVirtualRows;
    desc.areaSize = inner.GetSize();
    desc.selection = &page.virtualSelection;
    desc.boxSelect = true;
    desc.entry = &VirtualEntry;
    desc.fadeSurface = t.colors.bgSecondary;
    desc.revealRow = page.revealTop ? 0 : -1;
    const auto start = std::chrono::steady_clock::now();
    int submitted = 0;
    if (fi::BeginListView(ImGui::GetID("virtual"), desc))
    {
        facet::RowRange rows;
        char probe[48];
        while (facet::NextVisibleRows(&rows))
        {
            for (int i = rows.first; i < rows.last; ++i)
            {
                const fi::ListRowState row = fi::ListViewRow(i);
                std::snprintf(probe, sizeof(probe), "list.virtual.row%d", i);
                ctx.Probe(probe);
                fi::ListEntryInfo info;
                VirtualEntry(nullptr, i, &info);
                RowLabel(row, info.label);
                if (row.focused)
                    page.virtualFocus = i;
                ++submitted;
            }
        }
        const fi::ListViewResult result = fi::EndListView();
        if (result.activated >= 0)
            page.virtualActivated = result.activated;
    }
    ctx.Probe("list.virtual");
    page.virtualMicros =
        std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count();
    page.virtualSubmitted = submitted;
    EndCard(inner);
}

void MultiList(facet::demo::PageCtx& ctx)
{
    const facet::Theme& t = facet::T();
    const float width = std::min(ImGui::GetContentRegionAvail().x, facet::Px(t.metrics.cardWidth * 1.5f));
    // Tall enough for every entry, so the probes of all rows exist.
    const float padding = (fi::FocusRingOutset() + facet::Px(t.spacing.xs)) * 2.0f;
    const ImRect inner = Card(width, fi::ListRowHeight(facet::ControlSize::Regular) * kMultiRows + padding);
    ImGui::SetCursorScreenPos(inner.Min);
    fi::ListViewDesc desc;
    desc.rowCount = kMultiRows;
    desc.areaSize = inner.GetSize();
    desc.selection = &page.multiSelection;
    desc.boxSelect = true;
    desc.entry = &MultiEntry;
    desc.fadeSurface = t.colors.bgSecondary;
    ForceRow(ctx, 3);
    if (fi::BeginListView(ImGui::GetID("multi"), desc))
    {
        facet::RowRange rows;
        char probe[48];
        while (facet::NextVisibleRows(&rows))
        {
            for (int i = rows.first; i < rows.last; ++i)
            {
                if (ctx.capture && ctx.captureState == kFocused && i == 5)
                {
                    ImGui::SetKeyboardFocusHere();
                    fi::SetKeyboardModality(true);
                }
                const fi::ListRowState row = fi::ListViewRow(i);
                std::snprintf(probe, sizeof(probe), "list.multi.row%d", i);
                ctx.Probe(probe);
                fi::ListEntryInfo info;
                MultiEntry(nullptr, i, &info);
                RowLabel(row, info.label);
            }
        }
        const fi::ListViewResult result = fi::EndListView();
        if (result.activated >= 0)
            page.multiActivated = result.activated;
    }
    ctx.Probe("list.multi");
    EndCard(inner);
}

void SortedList(facet::demo::PageCtx& ctx)
{
    const facet::Theme& t = facet::T();
    static constexpr const char* kSorts[] = {"Ascending", "Descending"};
    static constexpr const char* kFilters[] = {"All", "Odd ids", "Even ids"};
    const int sort = page.sort;
    const int filter = page.filter;
    facet::demo::ProtoSegmented(ctx, "sort", &page.sort, kSorts, "list.sort");
    facet::demo::ProtoSegmented(ctx, "filter", &page.filter, kFilters, "list.filter");
    if (sort != page.sort || filter != page.filter || page.sortedView.empty())
        RebuildSortedView();
    const float width = std::min(ImGui::GetContentRegionAvail().x, facet::Px(t.metrics.cardWidth * 1.5f));
    const ImRect inner = Card(width, fi::ListRowHeight(facet::ControlSize::Small) * 8.0f);
    ImGui::SetCursorScreenPos(inner.Min);
    // The storage keys entries by id, so the selection survives sorting and filtering.
    page.sortedSelection.UserData = &page.sortedView;
    page.sortedSelection.AdapterIndexToStorageId = [](ImGuiSelectionBasicStorage* storage, int index) {
        return (*static_cast<std::vector<ImGuiID>*>(storage->UserData))[static_cast<size_t>(index)];
    };
    fi::ListViewDesc desc;
    desc.rowCount = static_cast<int>(page.sortedView.size());
    desc.size = facet::ControlSize::Small;
    desc.areaSize = inner.GetSize();
    desc.selection = &page.sortedSelection;
    desc.entry = &SortedEntry;
    desc.fadeSurface = t.colors.bgSecondary;
    desc.revealRow = page.revealTop ? 0 : -1;
    if (fi::BeginListView(ImGui::GetID("sorted"), desc))
    {
        facet::RowRange rows;
        char probe[48];
        while (facet::NextVisibleRows(&rows))
        {
            for (int i = rows.first; i < rows.last; ++i)
            {
                const fi::ListRowState row = fi::ListViewRow(i);
                std::snprintf(probe, sizeof(probe), "list.sorted.id%u", page.sortedView[static_cast<size_t>(i)]);
                ctx.Probe(probe);
                fi::ListEntryInfo info;
                SortedEntry(nullptr, i, &info);
                RowLabel(row, info.label);
            }
        }
        fi::EndListView();
    }
    ctx.Probe("list.sorted");
    EndCard(inner);
}

void IconChoice(facet::demo::PageCtx& ctx)
{
    const facet::Theme& t = facet::T();
    const float width = std::min(ImGui::GetContentRegionAvail().x, facet::Px(t.metrics.cardWidth * 1.5f));
    const ImRect inner = Card(width, fi::ListRowHeight(facet::ControlSize::Regular) * 6.0f);
    ImGui::SetCursorScreenPos(inner.Min);
    fi::ListViewDesc desc;
    desc.rowCount = facet::IconCount();
    desc.areaSize = inner.GetSize();
    desc.choice = &page.choice;
    desc.pill = true;
    desc.entry = &IconEntry;
    desc.fadeSurface = t.colors.bgSecondary;
    desc.revealRow = page.revealTop ? 0 : -1;
    if (fi::BeginListView(ImGui::GetID("icons"), desc))
    {
        facet::RowRange rows;
        char probe[48];
        while (facet::NextVisibleRows(&rows))
        {
            for (int i = rows.first; i < rows.last; ++i)
            {
                const fi::ListRowState row = fi::ListViewRow(i);
                std::snprintf(probe, sizeof(probe), "list.icons.row%d", i);
                ctx.Probe(probe);
                const char* name = facet::IconNameAt(i);
                RowLabel(row, name, facet::Icon(name));
            }
        }
        fi::EndListView();
    }
    ctx.Probe("list.icons");
    EndCard(inner);
}

// One node of the tree, with a probe "list.tree.<label>".
fi::ListTreeNodeState TreeNode(facet::demo::PageCtx& ctx, const char* label, const fi::ListTreeNodeOptions& options = {})
{
    const fi::ListTreeNodeState node = fi::ListTreeNode(label, options);
    char probe[96];
    std::snprintf(probe, sizeof(probe), "list.tree.%s", label);
    ctx.Probe(probe);
    RowLabel(node.row, label);
    return node;
}

void Tree(facet::demo::PageCtx& ctx)
{
    const facet::Theme& t = facet::T();
    const float width = std::min(ImGui::GetContentRegionAvail().x, facet::Px(t.metrics.cardWidth * 1.5f));
    const ImRect inner = Card(width, facet::Px(t.metrics.panelHeight * 1.25f));
    ImGui::SetCursorScreenPos(inner.Min);
    fi::ListTreeDesc desc;
    desc.selection = &page.treeSelection;
    desc.areaSize = inner.GetSize();
    desc.fadeSurface = t.colors.bgSecondary;
    if (!fi::BeginListTree(ImGui::GetID("tree"), desc))
    {
        ctx.Probe("list.tree");
        EndCard(inner);
        return;
    }
    // Three levels: the groups of this gallery, their pages, and each page's capture states.
    const std::span<const facet::demo::PageInfo> pages = facet::demo::Pages();
    for (size_t i = 0; i < pages.size(); ++i)
    {
        if (i > 0 && pages[i].group == pages[i - 1].group)
            continue;
        const std::string& group = pages[i].group;
        if (!TreeNode(ctx, group.c_str(), {.defaultOpen = group == "Blocks"}).open)
            continue;
        for (size_t j = i; j < pages.size() && pages[j].group == group; ++j)
        {
            const facet::demo::PageInfo& info = pages[j];
            if (TreeNode(ctx, info.title.c_str()).open)
            {
                for (const std::string& state : info.states)
                    TreeNode(ctx, state.c_str(), {.leaf = true});
                fi::ListTreePop();
            }
        }
        fi::ListTreePop();
    }
    // Children that arrive a moment after the node opens, as from a slow source.
    const char* lazyLabel = "Loaded on open (simulated delay)";
    const fi::ListTreeNodeState lazy = TreeNode(ctx, lazyLabel, {.loading = page.lazyOpen && !page.lazyLoaded});
    page.lazyOpen = lazy.open;
    if (lazy.open)
    {
        if (!page.lazyLoaded)
        {
            page.lazyWait += facet::DeltaTime();
            page.lazyLoaded = page.lazyWait >= facet::T().motion.durationSlow * 3.0f;
        }
        if (page.lazyLoaded)
        {
            char label[48];
            for (int i = 1; i <= kLazyChildren; ++i)
            {
                std::snprintf(label, sizeof(label), "Generated child %d", i);
                TreeNode(ctx, label, {.leaf = true});
            }
        }
        fi::ListTreePop();
    }
    else
    {
        page.lazyWait = 0.0f;
        page.lazyLoaded = false;
    }
    const fi::ListTreeResult result = fi::EndListTree();
    if (result.activated != 0)
        page.treeActivated = static_cast<int>(page.treeSelection.Size);
    ctx.Probe("list.tree");
    EndCard(inner);
}

void States(facet::demo::PageCtx&)
{
    const facet::Theme& t = facet::T();
    struct Look
    {
        const char* title;
        facet::DataState state;
        int rows;
    };
    const Look looks[] = {
        {"Loading", facet::DataState::Loading, 5},
        {"Error", facet::DataState::Error, 5},
        {"Empty", facet::DataState::Ready, 0},
    };
    const float gap = facet::Px(t.spacing.md);
    const float available = ImGui::GetContentRegionAvail().x;
    const bool stack = available < facet::Px(t.layout.regularFrom * 1.5f);
    const float width = stack ? available : (available - gap * 2.0f) / 3.0f;
    const float height = fi::ListRowHeight(facet::ControlSize::Regular) * 3.0f;
    for (int i = 0; i < 3; ++i)
    {
        if (i > 0 && !stack)
            ImGui::SameLine(0.0f, gap);
        ImGui::BeginGroup();
        ImGui::PushID(i);
        facet::Text(facet::TextStyle::Footnote, "%s", looks[i].title);
        const ImRect inner = Card(width, height);
        ImGui::SetCursorScreenPos(inner.Min);
        fi::ListViewDesc desc;
        desc.rowCount = looks[i].rows;
        desc.areaSize = inner.GetSize();
        desc.state = looks[i].state;
        desc.entry = &MultiEntry;
        desc.fadeSurface = t.colors.bgSecondary;
        if (fi::BeginListView(ImGui::GetID("state"), desc))
            fi::EndListView();
        EndCard(inner);
        ImGui::PopID();
        ImGui::EndGroup();
    }
}

int SelectedIndices(ImGuiSelectionBasicStorage& selection, int count, char* out, size_t size)
{
    int written = 0;
    out[0] = '\0';
    size_t used = 0;
    for (int i = 0; i < count && used + 8 < size; ++i)
    {
        if (!selection.Contains(selection.GetStorageIdFromIndex(i)))
            continue;
        used += static_cast<size_t>(std::snprintf(out + used, size - used, written == 0 ? "%d" : ", %d", i + 1));
        ++written;
    }
    return written;
}

int SelectedIds(char* out, size_t size)
{
    int written = 0;
    out[0] = '\0';
    size_t used = 0;
    for (ImGuiID id = 1; id <= kSortedRows && used + 8 < size; ++id)
    {
        if (!page.sortedSelection.Contains(id))
            continue;
        used += static_cast<size_t>(std::snprintf(out + used, size - used, written == 0 ? "%u" : ", %u", id));
        ++written;
    }
    return written;
}

} // namespace

FACET_DEMO_PAGE("list-core", "List core", "Blocks", 130, "default", "row hovered", "keyboard focus",
                "tree and states")
{
    // Every captured look starts from one known selection.
    if (!page.initialized || (ctx.capture && ctx.captureState != page.preparedState))
    {
        Reset();
        page.multiSelection.SetItemSelected(1, true);
        page.multiSelection.SetItemSelected(6, true);
        page.virtualSelection.SetItemSelected(2, true);
        page.choice = 4;
        page.initialized = true;
        page.preparedState = ctx.capture ? ctx.captureState : -1;
    }
    facet::demo::PageHeader("List core",
                            "Rows of one height per control size in a scroll area, virtualized, with Dear ImGui's "
                            "multi-select behind them: click, Ctrl and Shift, Shift with the arrows, Ctrl+A and box "
                            "selection; arrows, Page Up and Down, Home and End, and typeahead.");
    Note("Prototype lists built on the list core, the scroll core, the item behaviour, the focus ring and the selection "
         "pill blocks. Generated rows and placeholder entries are labelled as such.");

    SectionHeader("Virtual list with one million generated rows");
    VirtualList(ctx);
    if (ctx.capture || ctx.probes != nullptr)
        Note("Rows submitted this frame: %d of %d. The build time is shown in interactive runs only.",
             page.virtualSubmitted, kVirtualRows);
    else
        Note("Rows submitted this frame: %d of %d, built in %.0f us (measured this frame).", page.virtualSubmitted,
             kVirtualRows, page.virtualMicros);
    Note("Selected rows: %d. Ctrl+A selects all of them; a drag from a row selects the rows it covers.",
         page.virtualSelection.Size);

    SectionHeader("Multiple selection");
    MultiList(ctx);
    if (ctx.capture && ctx.captureState != kDefault)
        ctx.RevealInCapture(0.5f);
    char text[160];
    if (SelectedIndices(page.multiSelection, kMultiRows, text, sizeof(text)) > 0)
        Note("Selected entries: %s. Disabled entries are skipped by the arrows, ranges and Ctrl+A.", text);
    else
        Note("Nothing selected. Disabled entries are skipped by the arrows, ranges and Ctrl+A.");

    SectionHeader("Sorted and filtered, selection by id");
    SortedList(ctx);
    if (SelectedIds(text, sizeof(text)) > 0)
        Note("Selected ids: %s. The selection is kept by id while the order and the filter change, also for entries "
             "the filter hides.",
             text);
    else
        Note("Nothing selected. The selection is kept by id while the order and the filter change.");

    SectionHeader("Single choice on the selection pill");
    IconChoice(ctx);
    Note("Chosen: %s. The names of the built-in icons; typing jumps to the next name that starts with the typed text.",
         page.choice >= 0 ? facet::IconNameAt(page.choice) : "none");

    SectionHeader("Tree");
    Tree(ctx);
    if (ctx.capture && ctx.captureState == kLower)
        ctx.RevealInCapture(0.45f);
    Note("The groups, pages and capture states of this gallery. Right opens and enters, Left closes and goes to the "
         "parent. Selected nodes: %d.",
         page.treeSelection.Size);

    SectionHeader("States");
    States(ctx);
    page.revealTop = false;
}

FACET_DEMO_SELFTEST("list-core")
{
    using T = facet::demo::Target;
    test.Do([] { Reset(); });
    test.Wait(2);

    // Multiple selection, by mouse. The first entries, which are on screen together at every size.
    test.Reveal("list.multi");
    test.Wait(2);
    test.Click("list.multi.row0");
    test.Expect("a click selects one entry",
                [] { return page.multiSelection.Size == 1 && page.multiSelection.Contains(0); });
    test.KeyDown(ImGuiMod_Ctrl);
    test.Click("list.multi.row1");
    test.KeyUp(ImGuiMod_Ctrl);
    test.Expect("Ctrl+click adds one", [] { return page.multiSelection.Size == 2 && page.multiSelection.Contains(1); });
    test.KeyDown(ImGuiMod_Shift);
    test.Click("list.multi.row3");
    test.KeyUp(ImGuiMod_Shift);
    test.Expect("Shift+click selects the range without the disabled entry", [] {
        return page.multiSelection.Size == 2 && page.multiSelection.Contains(1) && !page.multiSelection.Contains(2) &&
               page.multiSelection.Contains(3);
    });
    // And by keyboard.
    test.KeyPress(ImGuiKey_UpArrow | ImGuiMod_Shift);
    test.Expect("Shift+Up shrinks the range past the disabled entry",
                [] { return page.multiSelection.Size == 1 && page.multiSelection.Contains(1); });
    test.KeyPress(ImGuiKey_A | ImGuiMod_Ctrl);
    test.ExpectNear("Ctrl+A selects every enabled entry", [] { return page.multiSelection.Size; }, kMultiRows - 2, 0.0);
    test.KeyPress(ImGuiKey_Escape);
    test.ExpectNear("Escape clears", [] { return page.multiSelection.Size; }, 0.0, 0.0);
    test.Click("list.multi.row1");
    test.KeyPress(ImGuiKey_DownArrow);
    test.Expect("Down skips the disabled entry", [] { return page.multiSelection.Contains(3); });
    test.DoubleClick("list.multi.row0");
    test.ExpectNear("a double click activates", [] { return page.multiActivated; }, 0.0, 0.0);

    // The virtual list: the ends, select all, and the wheel.
    test.Reveal("list.virtual");
    test.Wait(2);
    test.Click("list.virtual.row0");
    test.KeyPress(ImGuiKey_End);
    test.Wait(3);
    test.ExpectNear("End moves to the last of a million rows", [] { return page.virtualFocus; }, kVirtualRows - 1, 0.0);
    test.ExpectVisible("list.virtual.row999999");
    test.ExpectNotVisible("list.virtual.row0");
    test.Expect("only the visible rows are submitted", [] { return page.virtualSubmitted < 40; });
    test.KeyPress(ImGuiKey_A | ImGuiMod_Ctrl);
    test.ExpectNear("Ctrl+A selects all of them", [] { return page.virtualSelection.Size; }, kVirtualRows, 0.0);
    test.KeyPress(ImGuiKey_Home);
    test.Wait(3);
    test.ExpectNear("Home moves back to the first", [] { return page.virtualFocus; }, 0.0, 0.0);
    test.Expect("and selects it alone", [] { return page.virtualSelection.Size == 1; });
    test.Wheel(T::Item("list.virtual"), -3.0f);
    test.WaitSeconds(0.6f);
    test.ExpectNotVisible("list.virtual.row0");

    // Selection by id through sorting and filtering.
    test.Reveal("list.sorted");
    test.Wait(2);
    test.Click("list.sorted.id5");
    test.Click("list.sort.1");
    test.Expect("sorting keeps the selected id", [] {
        return page.sortedSelection.Size == 1 && page.sortedSelection.Contains(5) && page.sortedView.front() == 30;
    });
    test.Reveal("list.sorted");
    test.KeyDown(ImGuiMod_Ctrl);
    test.Click("list.sorted.id28");
    test.KeyUp(ImGuiMod_Ctrl);
    test.Expect("Ctrl+click adds another id", [] {
        return page.sortedSelection.Size == 2 && page.sortedSelection.Contains(28) && page.sortedSelection.Contains(5);
    });
    test.Click("list.filter.1");
    test.Expect("a filter that hides an entry keeps it selected",
                [] { return page.sortedSelection.Contains(28) && page.sortedSelection.Contains(5); });
    test.Click("list.filter.0");
    test.Click("list.sort.0");

    // Typeahead over the icon names.
    test.Reveal("list.icons");
    test.Wait(2);
    test.Click("list.icons.row0");
    test.ExpectNear("a click chooses", [] { return page.choice; }, 0.0, 0.0);
    const auto chosenStartsWith = [](const char* prefix) {
        const char* name = page.choice >= 0 ? facet::IconNameAt(page.choice) : nullptr;
        return name != nullptr && std::string(name).rfind(prefix, 0) == 0;
    };
    test.Type("se");
    test.Wait(2);
    test.Expect("typing jumps to a name that starts with the text", [=] { return chosenStartsWith("se"); });
    test.Type("t");
    test.Wait(2);
    test.Expect("more typing narrows it", [=] { return chosenStartsWith("set"); });
    test.WaitSeconds(facet::T().list.typeaheadTimeout + 0.1f);
    test.Type("z");
    test.Wait(2);
    test.Expect("after a pause the text starts again", [=] { return chosenStartsWith("z"); });
    static int before = 0;
    test.Do([] { before = page.choice; });
    test.KeyPress(ImGuiKey_UpArrow);
    test.Expect("Up moves the choice to the row above", [] { return page.choice == before - 1; });

    // The tree: keys open, enter, leave and close.
    test.Reveal("list.tree");
    test.Wait(2);
    test.Click("list.tree.Media");
    test.KeyPress(ImGuiKey_RightArrow);
    test.Wait(2);
    test.ExpectVisible("list.tree.Icons");
    test.KeyPress(ImGuiKey_RightArrow);
    test.Wait(2);
    test.ExpectFocused("list.tree.Icons");
    test.KeyPress(ImGuiKey_LeftArrow);
    test.Wait(2);
    test.ExpectFocused("list.tree.Media");
    test.KeyPress(ImGuiKey_LeftArrow);
    test.Wait(2);
    test.ExpectNotVisible("list.tree.Icons");
    // A node whose children load after it opens.
    test.DoubleClick("list.tree.Loaded on open (simulated delay)");
    test.Expect("the node opens and loads", [] { return page.lazyOpen && !page.lazyLoaded; });
    test.WaitSeconds(1.2f);
    test.Expect("the children arrive", [] { return page.lazyLoaded; });
    test.Reveal("list.tree.Generated child 1");
    test.ExpectVisible("list.tree.Generated child 1");
    test.DoubleClick("list.tree.Loaded on open (simulated delay)");
    test.Expect("a double click closes it again", [] { return !page.lazyOpen; });
    test.Do([] { Reset(); });
}
