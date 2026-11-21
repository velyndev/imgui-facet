// Demo page: Lists. One section per widget of facet/widgets/lists.h with its important variants
// and states, capture states for the open, focused and dragged looks, and a selftest that operates
// every ready widget by mouse and keyboard. A widget that is still planned draws its placeholder.
// Sample entries are neutral ("Option 1") with built-in icons; generated entries are numbered.

#include <algorithm>
#include <cstdio>
#include <string>

#include <imgui.h>

#include <facet/facet.h>

#include "gallery.h"
#include "internal/popup_host.h"
#include "pages/pages.h"
#include "widgets/lists/list_parts.h"

namespace
{

using facet::demo::ForcedItemState;
using facet::demo::LabeledRow;
using facet::demo::Note;
using facet::demo::Paragraph;
using facet::demo::SectionHeader;

// The capture states, in the order of the FACET_DEMO_PAGE line below.
enum CaptureState
{
    kDefault,
    kDropdownOpen,
    kDropdownSearch,
    kFocused,
    kMultiOpen,
    kComboOpen,
    kReorderDrag,
    kListFocused,
    kTree,
    kChecks,
    kTransfer,
};

const facet::ChoiceItem kOptions[] = {
    {.label = "Option 1", .icon = facet::Icon("file"), .group = "Group 1"},
    {.label = "Option 2", .icon = facet::Icon("folder"), .group = "Group 1"},
    {.label = "Option 3", .icon = facet::Icon("image"), .detail = "Detail", .group = "Group 2"},
    {.label = "Option 4 (unavailable)", .icon = facet::Icon("layers"), .group = "Group 2", .disabled = true},
    {.label = "Option 5", .icon = facet::Icon("grid"), .group = "Group 2"},
};

const char* const kLabels[] = {"Option 1", "Option 2", "Option 3", "Option 4"};

const facet::CheckTreeItem kCheckTree[] = {
    {.label = "Group 1", .icon = facet::Icon("folder")},
    {.label = "Option 1", .depth = 1},
    {.label = "Option 2", .depth = 1},
    {.label = "Group 2", .icon = facet::Icon("folder")},
    {.label = "Group 2.1", .icon = facet::Icon("folder"), .depth = 1},
    {.label = "Option 3", .depth = 2},
    {.label = "Option 4", .depth = 1},
};

constexpr int kGeneratedCount = 10000;
constexpr int kChecklistCount = 1000;

// Generated entries for the getter overloads: "Option <n>". The text lives in one static buffer,
// which lists.h allows: a getter's strings need to stay valid only until its next call.
bool NumberedOption(void*, int index, facet::ChoiceItem* out)
{
    static char text[32];
    std::snprintf(text, sizeof(text), "Option %d", index + 1);
    out->label = text;
    return true;
}

struct ListsState
{
    // Dropdown
    int dropdown = 0;
    int plain = 1;
    int searchable = 4999;
    int sized[3] = {0, 1, 2};
    int states[4] = {1, 1, -1, 2};
    int narrow = 2;
    int writes = 0;

    // ListBox and SelectableList
    int listBox = 1;
    int listBoxLong = 41;
    int activated = -1;

    // MultiDropdown
    bool multi[5] = {true, false, true, false, false};
    bool multiCounted[4] = {false, true, true, false};
    bool multiNone[4] = {};

    // The planned widgets below.
    int combo = 0;
    bool checklist[kChecklistCount] = {};
    bool checklistFirst[5] = {false, false, true, false, false};
    bool transfer[5] = {false, true, false, false, false};
    facet::CheckState checkTree[7] = {facet::CheckState::Mixed, facet::CheckState::On, facet::CheckState::Off,
                                      facet::CheckState::Off, facet::CheckState::Off, facet::CheckState::Off,
                                      facet::CheckState::Off};
    facet::CheckState checkTreePlain[7] = {};
    ImGuiSelectionBasicStorage selection;

    // ReorderableList
    facet::ChoiceItem reorder[4] = {{.label = "Option 1", .icon = facet::Icon("file"), .id = 1},
                                    {.label = "Option 2", .icon = facet::Icon("image"), .id = 2},
                                    {.label = "Option 3", .icon = facet::Icon("layers"), .id = 3},
                                    {.label = "Option 4", .icon = facet::Icon("grid"), .id = 4}};
    const char* reorderLabels[4] = {"Option A", "Option B", "Option C", "Option D"};
    facet::ReorderMove lastMove;

    // TreeView
    ImGuiSelectionBasicStorage treeSelection;
    const char* treeActivated = nullptr;
    int lazyState = 0;  // 0 not loaded, 1 loading, 2 loaded
    double lazyStart = 0.0;
};

ListsState& State()
{
    static ListsState state;
    return state;
}

// Records the open list of the dropdown-like widget submitted last as probe `name`, so the selftest
// clicks its rows wherever the list opened (below or above the field, at any scale).
void ProbePopup(facet::demo::PageCtx& ctx, const char* name)
{
    const ImGuiID level = facet::internal::ListsLevelId(ImGui::GetItemID());
    if (!facet::internal::IsPopupLevelOpen(level))
        return;
    const ImRect rect = facet::internal::PopupLevelRect(level);
    facet::demo::ProbeRect(ctx, name, level, rect.Min, rect.Max);
}

// The label of item `index` of kOptions, or "none".
const char* OptionName(int index)
{
    return index >= 0 && index < static_cast<int>(std::size(kOptions)) ? kOptions[index].label : "none";
}

void DropdownSection(facet::demo::PageCtx& ctx, ListsState& s)
{
    SectionHeader("Dropdown");
    Paragraph("A field that opens a list. Click, Space, Enter or Down opens it; the arrows move, Enter or a click "
              "chooses, Escape closes without choosing, and typing on the closed field jumps to the next matching "
              "item. The list opens above the field when there is no room below.");

    if (ctx.capture && ctx.captureState == kDropdownOpen)
        ctx.ForceNextItemState(ForcedItemState::Open);
    else if (ctx.capture && ctx.captureState == kFocused)
        ctx.ForceNextItemState(ForcedItemState::Focused);
    if (facet::Dropdown("Icons and groups", &s.dropdown, kOptions, {.width = 320.0f}))
        ++s.writes;
    ctx.Probe("lists.dropdown");
    ProbePopup(ctx, "lists.dropdown.popup");
    if (ctx.capture && ctx.captureState == kDropdownOpen)
        ctx.RevealInCapture(0.25f);

    if (facet::Dropdown("Plain labels", &s.plain, kLabels, {.width = 320.0f}))
        ++s.writes;
    ctx.Probe("lists.dropdown.plain");

    if (ctx.capture && ctx.captureState == kDropdownSearch)
        ctx.ForceNextItemState(ForcedItemState::Open);
    if (facet::Dropdown("Searchable", &s.searchable, kGeneratedCount, &NumberedOption, nullptr,
                        {.searchable = true, .width = 320.0f, .helpText = "10000 generated entries, fuzzy filter"}))
        ++s.writes;
    ctx.Probe("lists.dropdown.search");
    if (ctx.capture && ctx.captureState == kDropdownSearch)
        ctx.RevealInCapture(0.25f);

    LabeledRow("Sizes");
    {
        const facet::ControlSize sizes[] = {facet::ControlSize::Small, facet::ControlSize::Regular,
                                            facet::ControlSize::Large};
        const char* names[] = {"##small", "##regular", "##large"};
        for (int i = 0; i < 3; ++i)
        {
            if (i > 0)
                LabeledRow("");
            facet::Dropdown(names[i], &s.sized[i], kLabels, {.size = sizes[i], .width = 200.0f});
        }
    }

    LabeledRow("Disabled");
    ImGui::BeginDisabled();
    facet::Dropdown("##disabled", &s.states[0], kLabels, {.width = 200.0f});
    ImGui::EndDisabled();
    ctx.Probe("lists.dropdown.disabled");
    LabeledRow("Read-only");
    facet::Dropdown("##readonly", &s.states[1], kLabels, {.width = 200.0f, .readOnly = true});
    LabeledRow("Error");
    facet::Dropdown("##error", &s.states[2], kLabels, {.width = 200.0f, .error = "Choose an option"});
    LabeledRow("Items failed");
    facet::Dropdown("##failed", &s.states[3], kLabels,
                    {.state = facet::DataState::Error, .width = 200.0f, .helpText = "The list shows the error"});
    LabeledRow("Items loading");
    facet::Dropdown("##loading", &s.states[3], kLabels, {.state = facet::DataState::Loading, .width = 200.0f});
    LabeledRow("Narrow");
    facet::Dropdown("##narrow", &s.narrow, kOptions, {.width = 96.0f});

    Note("Chosen: %s, %s, Option %d. Choices made on this page: %d.", OptionName(s.dropdown), kLabels[s.plain],
         s.searchable + 1, s.writes);
}

// Two widgets side by side while the content is wide enough, one under the other otherwise.
struct Columns
{
    bool split = facet::AvailableWidthClass() != facet::WidthClass::Compact;
    float width = split ? (ImGui::GetContentRegionAvail().x - facet::Px(facet::T().spacing.xl)) * 0.5f
                        : ImGui::GetContentRegionAvail().x;
    float start = ImGui::GetCursorPosX();
    float top = ImGui::GetCursorPosY();
    float bottom = 0.0f;
    void Next()
    {
        bottom = ImGui::GetCursorPosY();
        if (split)
            ImGui::SetCursorPos(ImVec2(start + width + facet::Px(facet::T().spacing.xl), top));
    }
    void End()
    {
        if (split)
            ImGui::SetCursorPosY(std::max(bottom, ImGui::GetCursorPosY()));
        ImGui::Dummy(ImVec2(0.0f, 0.0f));
    }
};

void ListBoxSection(facet::demo::PageCtx& ctx, ListsState& s)
{
    SectionHeader("List box");
    Paragraph("A framed list with one chosen row on the selection pill. It is one Tab stop: the arrows, Page Up and "
              "Down, Home and End move the choice, typing jumps to a matching label.");
    Columns columns;
    if (ctx.capture && ctx.captureState == kListFocused)
        ctx.ForceNextItemState(ForcedItemState::Focused, 2);
    if (facet::ListBox("Icons and groups", &s.listBox, kOptions, {.visibleRows = 5, .width = columns.width / facet::Px(1.0f)}))
        ++s.writes;
    ctx.Probe("lists.listbox");
    columns.Next();
    if (facet::ListBox("Generated entries", &s.listBoxLong, kGeneratedCount, &NumberedOption, nullptr,
                       {.visibleRows = 5, .width = columns.width / facet::Px(1.0f),
                        .helpText = "10000 generated entries, only the visible rows are drawn"}))
        ++s.writes;
    ctx.Probe("lists.listbox.long");
    columns.End();

    Columns states;
    facet::ListBox("Loading", &s.listBox, kOptions,
                   {.visibleRows = 2, .state = facet::DataState::Loading, .width = states.width / facet::Px(1.0f)});
    states.Next();
    facet::ListBox("Failed", &s.listBox, kOptions,
                   {.visibleRows = 2, .state = facet::DataState::Error, .width = states.width / facet::Px(1.0f),
                    .error = "Choose an option"});
    states.End();
    Note("List box choice: %s, Option %d.", OptionName(s.listBox), s.listBoxLong + 1);
}

void SelectableListSection(facet::demo::PageCtx& ctx, ListsState& s)
{
    SectionHeader("Selectable list");
    Paragraph("A click selects, Ctrl+click toggles, Shift+click and Shift with the arrows extend, Ctrl+A selects all "
              "enabled rows, Escape clears, and a drag from empty space selects the rows it covers. Double click or "
              "Enter activates a row.");
    if (ctx.capture && ctx.captureState == kListFocused && s.selection.Size == 0)
    {
        // The capture shows a selection of two rows and the hover on a third.
        s.selection.SetItemSelected(0, true);
        s.selection.SetItemSelected(2, true);
    }
    if (ctx.capture && ctx.captureState == kListFocused)
        ctx.ForceNextItemState(ForcedItemState::Hovered, 4);
    if (facet::SelectableList("Rows", &s.selection, kOptions, {.visibleRows = 5, .width = 320.0f}))
        ++s.writes;
    ctx.Probe("lists.selectable");
    if (const int row = facet::SelectableListActivatedRow(); row >= 0)
        s.activated = row;
    Note("Selected rows: %d. Last activated: %s.", s.selection.Size, OptionName(s.activated));
    if (ctx.capture && ctx.captureState == kListFocused)
        ctx.RevealInCapture(0.85f);
}

int CountChecked(std::span<const bool> checked)
{
    return static_cast<int>(std::count(checked.begin(), checked.end(), true));
}

void MultiDropdownSection(facet::demo::PageCtx& ctx, ListsState& s)
{
    SectionHeader("Multi dropdown");
    Paragraph("A dropdown whose rows have checkboxes. Space, Enter or a click toggles the active row and the list "
              "stays open; Escape, Tab or a click outside closes it. The field lists the checked labels or counts "
              "them.");
    if (ctx.capture && ctx.captureState == kMultiOpen)
        ctx.ForceNextItemState(ForcedItemState::Open);
    if (facet::MultiDropdown("With select all", s.multi, kOptions, {.selectAllRow = true, .width = 320.0f}))
        ++s.writes;
    ctx.Probe("lists.multi");
    ProbePopup(ctx, "lists.multi.popup");
    if (ctx.capture && ctx.captureState == kMultiOpen)
        ctx.RevealInCapture(0.25f);
    if (facet::MultiDropdown("Counted", s.multiCounted, kLabels,
                             {.summary = facet::MultiDropdownSummary::Count, .searchable = true, .width = 320.0f}))
        ++s.writes;
    ctx.Probe("lists.multi.count");
    LabeledRow("Read-only");
    facet::MultiDropdown("##multireadonly", s.multi, kOptions, {.width = 200.0f, .readOnly = true});
    LabeledRow("Nothing checked");
    facet::MultiDropdown("##multinone", s.multiNone, kLabels, {.width = 200.0f, .helpText = "Shows the placeholder"});
    Note("Checked: %d of %d, %d of %d.", CountChecked(s.multi), static_cast<int>(std::size(kOptions)),
         CountChecked(s.multiCounted), static_cast<int>(std::size(kLabels)));
}

void ComboSection(facet::demo::PageCtx& ctx, ListsState& s)
{
    SectionHeader("Combo with custom rows");
    Paragraph("The caller submits the rows. The list takes the keyboard focus on the chosen row; the arrows move, "
              "Enter or a click chooses, Escape closes. Rows wrapped in a disabled block are shown but refused.");
    if (ctx.capture && ctx.captureState == kComboOpen)
        ctx.ForceNextItemState(ForcedItemState::Open);
    if (facet::BeginCombo("Custom rows", kOptions[s.combo].label, {.icon = kOptions[s.combo].icon, .width = 320.0f}))
    {
        for (int i = 0; i < static_cast<int>(std::size(kOptions)); ++i)
        {
            ImGui::PushID(i);
            ImGui::BeginDisabled(kOptions[i].disabled);
            if (facet::ComboItem(kOptions[i].label, s.combo == i,
                                 {.icon = kOptions[i].icon, .detail = kOptions[i].detail}))
            {
                s.combo = i;
                ++s.writes;
            }
            ImGui::EndDisabled();
            ImGui::PopID();
        }
        facet::EndCombo();
    }
    ctx.Probe("lists.combo");
    ProbePopup(ctx, "lists.combo.popup");
    if (ctx.capture && ctx.captureState == kComboOpen)
        ctx.RevealInCapture(0.25f);
    LabeledRow("Read-only");
    {
        facet::ComboScope combo("##comboreadonly", kLabels[1], {.width = 200.0f, .readOnly = true});
    }
    LabeledRow("Error");
    {
        facet::ComboScope combo("##comboerror", nullptr, {.width = 200.0f, .error = "Choose an option"});
        if (combo)
            for (int i = 0; i < 4; ++i)
                facet::ComboItem(kLabels[i], false);
    }
    Note("Combo choice: %s.", OptionName(s.combo));
}

void ChecklistSection(facet::demo::PageCtx& ctx, ListsState& s)
{
    SectionHeader("Searchable checklist");
    Paragraph("A checklist for long lists: the filter field narrows the rows with the fuzzy matcher, the first row "
              "checks or clears every shown entry, and the label line counts the checked entries.");
    Columns columns;
    if (facet::SearchableChecklist("Generated entries", s.checklist, kChecklistCount, &NumberedOption, nullptr,
                                   {.visibleRows = 5, .width = columns.width / facet::Px(1.0f)}))
        ++s.writes;
    ctx.Probe("lists.checklist");
    columns.Next();
    facet::SearchableChecklist("Checked first", s.checklistFirst, kOptions,
                               {.checkedFirst = true, .visibleRows = 5, .width = columns.width / facet::Px(1.0f),
                                .helpText = "Checked entries move up while the pointer is elsewhere"});
    columns.End();
    LabeledRow("Failed");
    facet::SearchableChecklist("##checklistfailed", s.checklistFirst, kOptions,
                               {.visibleRows = 2, .state = facet::DataState::Error, .width = 320.0f,
                                .error = "Choose at least one entry"});
}

void TransferSection(facet::demo::PageCtx& ctx, ListsState& s)
{
    SectionHeader("Transfer list");
    Paragraph("Two lists with their own filter and selection. Add and Remove move the selected rows; a double click "
              "or Enter moves them too, and rows can be dragged from one list into the other. In a narrow width the "
              "lists stack.");
    if (facet::TransferList("transfer", s.transfer, kOptions,
                            {.sourceTitle = "Available", .targetTitle = "Chosen", .visibleRows = 4}))
        ++s.writes;
    ctx.Probe("lists.transfer");
    {
        // The two frames, wherever they are laid out, so the selftest finds the rows of each list.
        ImRect source;
        ImRect target;
        const ImGuiID id = ImGui::GetID("transfer");
        if (facet::internal::ListsTransferFrames(id, &source, &target))
        {
            facet::demo::ProbeRect(ctx, "lists.transfer.source", ImHashStr("source", 0, id), source.Min, source.Max);
            facet::demo::ProbeRect(ctx, "lists.transfer.target", ImHashStr("target", 0, id), target.Min, target.Max);
            // Both frames together: revealing it centers the place where the lists meet.
            ImRect both = source;
            both.Add(target);
            facet::demo::ProbeRect(ctx, "lists.transfer.frames", ImHashStr("frames", 0, id), both.Min, both.Max);
        }
    }
    int chosen = 0;
    for (const bool in : s.transfer)
        chosen += in ? 1 : 0;
    Note("Chosen entries: %d of %d.", chosen, static_cast<int>(std::size(kOptions)));
    Columns columns;
    facet::TransferList("transfernarrow", s.transfer, kOptions,
                        {.sourceTitle = "Available", .targetTitle = "Chosen", .searchable = false, .visibleRows = 2,
                         .width = std::min(columns.width / facet::Px(1.0f), 300.0f)});
    columns.Next();
    facet::TransferList("transferloading", s.transfer, kOptions,
                        {.searchable = false, .visibleRows = 2, .state = facet::DataState::Loading,
                         .width = columns.width / facet::Px(1.0f)});
    columns.End();
    if (ctx.capture && ctx.captureState == kTransfer)
        ctx.RevealInCapture(0.85f);
}

void ReorderSection(facet::demo::PageCtx& ctx, ListsState& s)
{
    SectionHeader("Reorderable list");
    Paragraph("Drag a row by its grip or anywhere on it; the other rows move aside to show where it lands, and the "
              "list scrolls when the drag rests at its edge. With the keyboard, Alt+Up and Alt+Down move the focused "
              "row, or Space picks it up, the arrows move it, Space drops it and Escape puts it back.");
    Columns columns;
    if (ctx.capture && ctx.captureState == kReorderDrag)
        ctx.ForceNextItemState(ForcedItemState::Dragging, 1);
    facet::ReorderMove move;
    if (facet::ReorderableList("reorder", &move, s.reorder, {.width = columns.width / facet::Px(1.0f)}))
    {
        facet::ApplyReorderMove(s.reorder, move);
        s.lastMove = move;
        ++s.writes;
    }
    ctx.Probe("lists.reorder");
    if (ctx.capture && ctx.captureState == kReorderDrag)
        ctx.RevealInCapture(0.5f);
    columns.Next();
    // Plain labels are keyed by their index; only the grip starts a drag.
    if (facet::ReorderableList("reorderlabels", &move, s.reorderLabels,
                               {.handleOnly = true, .width = columns.width / facet::Px(1.0f)}))
        facet::ApplyReorderMove(s.reorderLabels, move);
    ctx.Probe("lists.reorder.labels");
    columns.End();
    Columns states;
    facet::ReorderableList("reorderreadonly", &move, s.reorder,
                           {.width = states.width / facet::Px(1.0f), .readOnly = true});
    states.Next();
    facet::ReorderableList("reorderloading", &move, s.reorder,
                           {.state = facet::DataState::Loading, .width = states.width / facet::Px(1.0f)});
    states.End();
    if (s.lastMove.IsValid())
        Note("Last move: row %d to %d. Order: %s, %s, %s, %s.", s.lastMove.from + 1, s.lastMove.to + 1,
             s.reorder[0].label, s.reorder[1].label, s.reorder[2].label, s.reorder[3].label);
    else
        Note("No row moved yet. Below: a read-only list without grips, and a list whose rows are loading.");
}

// A leaf of the sample tree with its probe "lists.tree.<label>"; counts an activation.
void SampleLeaf(facet::demo::PageCtx& ctx, ListsState& s, const char* label, ImGuiID id)
{
    if (facet::TreeLeaf(label, {.icon = facet::Icon("file"), .id = id}))
        s.treeActivated = label;
    char probe[64];
    std::snprintf(probe, sizeof(probe), "lists.tree.%s", label);
    ctx.Probe(probe);
}

void TreeViewSection(facet::demo::PageCtx& ctx, ListsState& s)
{
    SectionHeader("Tree view");
    Paragraph("Nodes with icons, details and indent guides. The arrows move; Right opens a node or goes to its first "
              "child, Left closes it or goes to its parent; Enter or a double click opens a node or activates a leaf. "
              "Click, Ctrl+click and Shift+click select; a closed node keeps its selected children selected.");
    const bool treeCapture = ctx.capture && ctx.captureState == kTree;
    if (treeCapture)
    {
        // The capture shows a selected node, the focus ring on another and the loading row.
        if (s.treeSelection.Size == 0)
            s.treeSelection.SetItemSelected(11, true);
        s.lazyState = 1;
        s.lazyStart = ImGui::GetTime();
        ctx.ForceNextItemState(ForcedItemState::Focused, 2);
    }
    if (facet::BeginTreeView("tree", &s.treeSelection, {.width = 360.0f}))
    {
        if (facet::BeginTreeNode("Group 1", {.icon = facet::Icon("folder"), .detail = "3 entries", .defaultOpen = true,
                                             .id = 1}))
        {
            ctx.Probe("lists.tree.Group 1");
            SampleLeaf(ctx, s, "Option 1", 11);
            SampleLeaf(ctx, s, "Option 2", 12);
            if (facet::BeginTreeNode("Group 1.1", {.icon = facet::Icon("folder"), .defaultOpen = true, .id = 13}))
            {
                SampleLeaf(ctx, s, "Option 3", 131);
                facet::EndTreeNode();
            }
            facet::EndTreeNode();
        }
        else
        {
            ctx.Probe("lists.tree.Group 1");
        }
        // Children that arrive later: the node shows the loading row for a second after it first
        // opens (a simulated delay standing in for a real fetch).
        if (treeCapture)
            ImGui::SetNextItemOpen(true);
        const bool open = facet::BeginTreeNode("Loaded on open", {.icon = facet::Icon("folder"), .id = 2,
                                                                  .loading = s.lazyState == 1});
        ctx.Probe("lists.tree.lazy");
        if (ImGui::IsItemToggledOpen() && s.lazyState == 0)
        {
            s.lazyState = 1;
            s.lazyStart = ImGui::GetTime();
        }
        if (s.lazyState == 1 && ImGui::GetTime() - s.lazyStart > 1.0)
            s.lazyState = 2;
        if (open)
        {
            if (s.lazyState == 2)
                SampleLeaf(ctx, s, "Option 4", 21);
            facet::EndTreeNode();
        }
        SampleLeaf(ctx, s, "Option 5", 3);
        facet::EndTreeView();
    }
    ctx.Probe("lists.tree");  // the view's area, recorded also while it is out of sight
    Note("Selected nodes: %d. Last activated: %s.", s.treeSelection.Size,
         s.treeActivated != nullptr ? s.treeActivated : "none");

    LabeledRow("Empty");
    if (facet::BeginTreeView("emptytree", nullptr, {.indentGuides = false, .emptyText = "No entries (placeholder)",
                                                    .width = 240.0f}))
        facet::EndTreeView();
    LabeledRow("Loading");
    if (facet::BeginTreeView("loadingtree", nullptr, {.state = facet::DataState::Loading, .width = 240.0f}))
        facet::EndTreeView();
    if (treeCapture)
        ctx.RevealInCapture(0.8f);
}

const char* CheckName(facet::CheckState state)
{
    switch (state)
    {
    case facet::CheckState::On:
        return "on";
    case facet::CheckState::Mixed:
        return "mixed";
    default:
        return "off";
    }
}

void CheckboxTreeSection(facet::demo::PageCtx& ctx, ListsState& s)
{
    SectionHeader("Checkbox tree");
    Paragraph("A click or Space toggles a node: a group sets all its entries, and a group shows on, off or mixed "
              "from them. Enter or the arrows open and close groups.");
    Columns columns;
    if (facet::CheckboxTree("checktree", s.checkTree, kCheckTree, {.width = columns.width / facet::Px(1.0f)}))
        ++s.writes;
    ctx.Probe("lists.checktree");
    columns.Next();
    facet::CheckboxTree("checktreeplain", s.checkTreePlain, kCheckTree,
                        {.cascade = false, .width = columns.width / facet::Px(1.0f)});
    ctx.Probe("lists.checktree.plain");
    columns.End();
    Note("Group 1 is %s, Group 2 is %s. The right tree toggles each node on its own.", CheckName(s.checkTree[0]),
         CheckName(s.checkTree[3]));
    Columns states;
    facet::CheckboxTree("checktreereadonly", s.checkTree, kCheckTree,
                        {.width = states.width / facet::Px(1.0f), .readOnly = true});
    states.Next();
    facet::CheckboxTree("checktreefailed", s.checkTree, kCheckTree,
                        {.state = facet::DataState::Error, .width = states.width / facet::Px(1.0f)});
    states.End();
    if (ctx.capture && ctx.captureState == kChecks)
        ctx.RevealInCapture(0.45f);
}

} // namespace

FACET_DEMO_PAGE("lists", "Lists", "Widgets", 60, "default", "dropdown-open", "dropdown-search", "focused",
                 "multi-open", "combo-open", "reorder-drag", "list-focused", "tree", "checks", "transfer")
{
    facet::demo::PageHeader("Lists",
                            "Dropdowns, combo boxes, list boxes, selectable and reorderable lists, tree views, "
                            "checkbox trees, transfer lists and checklists.");
    facet::demo::PlannedFamilyNote("lists");
    ListsState& s = State();
    DropdownSection(ctx, s);
    ListBoxSection(ctx, s);
    SelectableListSection(ctx, s);
    MultiDropdownSection(ctx, s);
    ComboSection(ctx, s);
    TreeViewSection(ctx, s);
    ReorderSection(ctx, s);
    CheckboxTreeSection(ctx, s);
    ChecklistSection(ctx, s);
    TransferSection(ctx, s);
}

FACET_DEMO_SELFTEST("lists")
{
    using T = facet::demo::Target;
    test.Do([] {
        ListsState& s = State();
        s.dropdown = 0;
        s.plain = 1;
        s.searchable = 4999;
        s.writes = 0;
        s.states[0] = 1;
        s.listBox = 1;  // its row is in view at the list's first scroll position
    });

    // Rows of an open list without a filter field: the list padding and the area padding (8 units)
    // before row 0, 32 units per row, wherever the list opened.
    const auto popupRow = [](const char* probe, int row) {
        return T::Item(probe, 0.5f, 0.0f).Offset(0.0f, 8.0f + 32.0f * (static_cast<float>(row) + 0.5f));
    };

    // Dropdown, pointer: a click opens the list, a click on a row chooses it.
    test.Click("lists.dropdown");
    test.Click("lists.dropdown");  // a click on the field of the open list closes it
    test.ExpectNear("a click on the open field closes without choosing", [] { return State().writes; }, 0.0, 0.0);
    test.Click("lists.dropdown");
    test.MouseMove(popupRow("lists.dropdown.popup", 2));
    test.MouseDown();
    test.MouseUp();
    test.ExpectNear("a click on the third row (Option 2) chooses it", [] { return State().dropdown; }, 1.0, 0.0);
    test.ExpectNear("one choice was written", [] { return State().writes; }, 1.0, 0.0);

    // Keyboard: Space opens, Down moves past the disabled row's neighbours, Enter chooses.
    test.Click("lists.dropdown.plain");
    test.KeyPress(ImGuiKey_Escape);
    test.ExpectFocused("lists.dropdown.plain");
    test.KeyPress(ImGuiKey_Space);
    test.KeyPress(ImGuiKey_DownArrow);
    test.KeyPress(ImGuiKey_Enter);
    test.ExpectNear("Space, Down and Enter choose the next item", [] { return State().plain; }, 2.0, 0.0);
    test.KeyPress(ImGuiKey_DownArrow);
    test.KeyPress(ImGuiKey_UpArrow);
    test.KeyPress(ImGuiKey_Enter);
    test.ExpectNear("Down opens, Up and Enter choose the item before", [] { return State().plain; }, 1.0, 0.0);
    test.KeyPress(ImGuiKey_Enter);
    test.KeyPress(ImGuiKey_DownArrow);
    test.KeyPress(ImGuiKey_Escape);
    test.ExpectNear("Escape closes without choosing", [] { return State().plain; }, 1.0, 0.0);
    test.Type("o");
    test.Wait(2);
    test.ExpectNear("typing on the closed field jumps to the next match", [] { return State().plain; }, 2.0, 0.0);

    // Searchable: the filter field takes the typing; Enter takes the best match.
    test.Click("lists.dropdown.search");
    test.Wait(2);
    test.Type("7777");
    test.Wait(2);
    test.KeyPress(ImGuiKey_Enter);
    test.ExpectNear("the filter finds Option 7777", [] { return State().searchable; }, 7776.0, 0.0);

    // Disabled: no list, no choice.
    test.Click("lists.dropdown.disabled");
    test.ExpectNear("a disabled dropdown does not change", [] { return State().states[0]; }, 1.0, 0.0);

    // ListBox: rows counted from the bottom of the frame (5 rows of 32 units, 4 units of padding).
    // Rows: Group 1, Option 1, Option 2, Group 2, Option 3.
    const auto boxRow = [](const char* probe, int row) {
        return T::Item(probe, 0.5f, 1.0f).Offset(0.0f, -(4.0f + 32.0f * (5.0f - static_cast<float>(row) - 0.5f)));
    };
    test.MouseMove(boxRow("lists.listbox", 1));
    test.MouseDown();
    test.MouseUp();
    test.ExpectNear("a click chooses Option 1 in the list box", [] { return State().listBox; }, 0.0, 0.0);
    test.MouseMove(boxRow("lists.listbox", 0));
    test.MouseDown();
    test.MouseUp();
    test.ExpectNear("a click on a group header chooses nothing", [] { return State().listBox; }, 0.0, 0.0);
    test.KeyPress(ImGuiKey_DownArrow);
    test.KeyPress(ImGuiKey_DownArrow);
    test.ExpectNear("Down twice passes the group header to Option 3", [] { return State().listBox; }, 2.0, 0.0);
    test.KeyPress(ImGuiKey_End);
    test.ExpectNear("End skips the disabled last but one row to Option 5", [] { return State().listBox; }, 4.0, 0.0);
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectNear("Tab leaves the list box without a change", [] { return State().listBox; }, 4.0, 0.0);

    // SelectableList: no group headers, one row per item.
    test.Do([] { State().selection.Clear(); });
    test.MouseMove(boxRow("lists.selectable", 0));
    test.MouseDown();
    test.MouseUp();
    test.ExpectNear("a click selects one row", [] { return State().selection.Size; }, 1.0, 0.0);
    test.KeyDown(ImGuiMod_Shift);
    test.MouseMove(boxRow("lists.selectable", 4));
    test.MouseDown();
    test.MouseUp();
    test.KeyUp(ImGuiMod_Shift);
    test.ExpectNear("Shift+click selects the range without the disabled row", [] { return State().selection.Size; },
                    4.0, 0.0);
    test.KeyPress(ImGuiKey_Escape);
    test.ExpectNear("Escape clears the selection", [] { return State().selection.Size; }, 0.0, 0.0);
    test.KeyPress(ImGuiKey_A | ImGuiMod_Ctrl);
    test.ExpectNear("Ctrl+A selects every enabled row", [] { return State().selection.Size; }, 4.0, 0.0);
    test.KeyPress(ImGuiKey_UpArrow | ImGuiMod_Shift);
    test.ExpectNear("Shift+Up shrinks the range to three rows", [] { return State().selection.Size; }, 3.0, 0.0);
    test.KeyPress(ImGuiKey_Enter);
    test.ExpectNear("Enter activates the focused row", [] { return State().activated; }, 2.0, 0.0);

    // MultiDropdown. Rows: Select all, Group 1, Option 1, Option 2, Group 2, Option 3, Option 4, Option 5.
    test.Do([] {
        ListsState& s = State();
        const bool multi[5] = {true, false, true, false, false};
        const bool counted[4] = {false, true, true, false};
        std::copy(std::begin(multi), std::end(multi), s.multi);
        std::copy(std::begin(counted), std::end(counted), s.multiCounted);
    });
    test.Click("lists.multi");
    test.MouseMove(popupRow("lists.multi.popup", 3));
    test.MouseDown();
    test.MouseUp();
    test.Expect("a click on Option 2 checks it and the list stays open", [] { return State().multi[1]; });
    test.Wait(20);
    test.MouseDown();
    test.MouseUp();
    test.Expect("a second click clears it", [] { return !State().multi[1]; });
    test.KeyPress(ImGuiKey_Space);
    test.ExpectNear("Space toggles the active row", [] { return CountChecked(State().multi); }, 3.0, 0.0);
    test.KeyPress(ImGuiKey_Escape);
    test.KeyPress(ImGuiKey_DownArrow);
    test.KeyPress(ImGuiKey_Space);
    test.ExpectNear("Down opens with the first row active; Space on Select all checks every enabled item",
                    [] { return CountChecked(State().multi); }, 4.0, 0.0);
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Space);
    test.ExpectNear("Tab closes the list without a change", [] { return CountChecked(State().multi); }, 4.0, 0.0);
    test.KeyPress(ImGuiKey_Escape);

    test.Click("lists.multi.count");
    test.Wait(2);
    test.Type("3");
    test.Wait(2);
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("the filter field finds Option 3 and Enter clears it", [] { return !State().multiCounted[2]; });
    test.KeyPress(ImGuiKey_Escape);

    // Combo: the caller's rows. Rows: Option 1 to Option 5, Option 4 disabled.
    test.Do([] { State().combo = 0; });
    test.Click("lists.combo");
    test.MouseMove(popupRow("lists.combo.popup", 2));
    test.MouseDown();
    test.MouseUp();
    test.ExpectNear("a click on the third row chooses Option 3", [] { return State().combo; }, 2.0, 0.0);
    test.Click("lists.combo");
    test.MouseMove(popupRow("lists.combo.popup", 3));
    test.MouseDown();
    test.MouseUp();
    test.ExpectNear("the disabled row is refused", [] { return State().combo; }, 2.0, 0.0);
    test.KeyPress(ImGuiKey_Escape);
    test.ExpectFocused("lists.combo");
    test.KeyPress(ImGuiKey_Enter);
    test.KeyPress(ImGuiKey_DownArrow);
    test.KeyPress(ImGuiKey_Enter);
    test.ExpectNear("Enter opens on the chosen row; Down passes the disabled row, Enter chooses Option 5",
                    [] { return State().combo; }, 4.0, 0.0);
    test.ExpectFocused("lists.combo");

    // TreeView. Ids: Group 1 = 1, Option 1 = 11, Option 2 = 12, Option 5 = 3.
    test.Do([] {
        ListsState& s = State();
        s.treeSelection.Clear();
        s.treeActivated = nullptr;
        s.lazyState = 0;
    });
    test.Reveal("lists.tree");
    test.Click("lists.tree.Option 1");
    test.Expect("a click selects Option 1", [] { return State().treeSelection.Contains(11); });
    test.KeyDown(ImGuiMod_Ctrl);
    test.Click("lists.tree.Option 5");
    test.KeyUp(ImGuiMod_Ctrl);
    test.ExpectNear("Ctrl+click adds Option 5", [] { return State().treeSelection.Size; }, 2.0, 0.0);
    // The expander of Group 1 (spacing.xs plus half the indent from the row start) closes it; its
    // selected child stays selected.
    test.MouseMove(T::Item("lists.tree.Group 1", 0.0f, 0.5f).Offset(12.0f, 0.0f));
    test.MouseDown();
    test.MouseUp();
    test.ExpectNotVisible("lists.tree.Option 1");
    test.Expect("closing Group 1 keeps Option 1 selected", [] { return State().treeSelection.Contains(11); });
    test.Click("lists.tree.Group 1");
    test.KeyPress(ImGuiKey_RightArrow);
    test.ExpectVisible("lists.tree.Option 1");
    test.KeyPress(ImGuiKey_RightArrow);
    test.ExpectFocused("lists.tree.Option 1");
    test.KeyPress(ImGuiKey_Enter);
    test.ExpectText("Enter activates the leaf",
                    [] { return std::string(State().treeActivated != nullptr ? State().treeActivated : ""); }, "Option 1");
    test.KeyPress(ImGuiKey_LeftArrow);
    test.ExpectFocused("lists.tree.Group 1");
    // Children that load after the node opens.
    test.Click("lists.tree.lazy");
    test.KeyPress(ImGuiKey_RightArrow);
    test.ExpectNear("opening the node starts loading", [] { return State().lazyState; }, 1.0, 0.0);
    test.WaitSeconds(1.2f);
    test.ExpectNear("the children arrive", [] { return State().lazyState; }, 2.0, 0.0);
    test.Reveal("lists.tree.Option 4");
    test.ExpectVisible("lists.tree.Option 4");

    // ReorderableList: four rows, each a quarter of the list.
    test.Do([] {
        ListsState& s = State();
        const char* labels[] = {"Option 1", "Option 2", "Option 3", "Option 4"};
        for (int i = 0; i < 4; ++i)
        {
            s.reorder[i].label = labels[i];
            s.reorder[i].id = static_cast<ImGuiID>(i + 1);
        }
        s.lastMove = {};
    });
    test.Reveal("lists.reorder");
    test.Drag({T::Item("lists.reorder", 0.5f, 0.125f), T::Item("lists.reorder", 0.5f, 0.4f),
               T::Item("lists.reorder", 0.5f, 0.7f)},
              0.5f);
    test.Wait(2);
    test.ExpectText("dragging the first row into the lower half of the third moves it there",
                    [] { return std::string(State().reorder[2].label); }, "Option 1");
    test.ExpectNear("the reported move ends at index 2", [] { return State().lastMove.to; }, 2.0, 0.0);
    test.Drag({T::Item("lists.reorder", 0.5f, 0.875f), T::Item("lists.reorder", 0.5f, 0.5f),
               T::Item("lists.reorder", 0.5f, 0.05f)},
              0.5f);
    test.Wait(2);
    test.ExpectText("dragging the last row to the top moves it up", [] { return std::string(State().reorder[0].label); },
                    "Option 4");
    test.ExpectNear("the reported move starts at index 3", [] { return State().lastMove.from; }, 3.0, 0.0);
    // Keyboard: Alt+Down moves the focused row; Space picks a row up, Down moves it, Space drops it.
    test.Click("lists.reorder");
    test.KeyPress(ImGuiKey_Escape);
    test.MouseMove(T::Item("lists.reorder", 0.5f, 0.125f));
    test.MouseDown();
    test.MouseUp();
    test.KeyPress(ImGuiKey_DownArrow | ImGuiMod_Alt);
    test.ExpectText("Alt+Down moves the focused row down", [] { return std::string(State().reorder[1].label); },
                    "Option 4");
    test.KeyPress(ImGuiKey_Space);
    test.KeyPress(ImGuiKey_DownArrow);
    test.KeyPress(ImGuiKey_Space);
    test.Wait(2);
    test.ExpectText("Space, Down and Space move it one more place", [] { return std::string(State().reorder[2].label); },
                    "Option 4");
    test.KeyPress(ImGuiKey_Space);
    test.KeyPress(ImGuiKey_UpArrow);
    test.KeyPress(ImGuiKey_Escape);
    test.Wait(2);
    test.ExpectText("Escape puts a picked-up row back", [] { return std::string(State().reorder[2].label); },
                    "Option 4");

    // CheckboxTree. Rows: Group 1, Option 1, Option 2, Group 2, Group 2.1, Option 3, Option 4.
    test.Do([] {
        ListsState& s = State();
        const facet::CheckState start[7] = {facet::CheckState::Mixed, facet::CheckState::On, facet::CheckState::Off,
                                            facet::CheckState::Off, facet::CheckState::Off, facet::CheckState::Off,
                                            facet::CheckState::Off};
        std::copy(std::begin(start), std::end(start), s.checkTree);
    });
    const auto treeRow = [](const char* probe, int row) {
        return T::Item(probe, 0.5f, 0.0f).Offset(0.0f, 4.0f + 32.0f * (static_cast<float>(row) + 0.5f));
    };
    test.Reveal("lists.checktree");
    test.MouseMove(treeRow("lists.checktree", 2));
    test.MouseDown();
    test.MouseUp();
    test.Expect("checking Option 2 turns Group 1 on", [] { return State().checkTree[0] == facet::CheckState::On; });
    test.KeyPress(ImGuiKey_UpArrow);
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Space clears Option 1 and Group 1 shows mixed",
                [] { return State().checkTree[0] == facet::CheckState::Mixed; });
    test.MouseMove(treeRow("lists.checktree", 5));
    test.MouseDown();
    test.MouseUp();
    test.Expect("checking Option 3 turns Group 2.1 on and Group 2 mixed", [] {
        return State().checkTree[4] == facet::CheckState::On && State().checkTree[3] == facet::CheckState::Mixed;
    });
    test.KeyPress(ImGuiKey_UpArrow);
    test.KeyPress(ImGuiKey_UpArrow);
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Space on Group 2 checks all of it", [] {
        return State().checkTree[3] == facet::CheckState::On && State().checkTree[6] == facet::CheckState::On;
    });

    // SearchableChecklist over 1000 generated entries. Rows: Select all, Option 1, Option 2, ...
    test.Do([] { std::fill(std::begin(State().checklist), std::end(State().checklist), false); });
    test.Reveal("lists.checklist");
    test.MouseMove(boxRow("lists.checklist", 1));
    test.MouseDown();
    test.MouseUp();
    test.Expect("a click checks Option 1", [] { return State().checklist[0]; });
    test.KeyPress(ImGuiKey_DownArrow);
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Down and Space check Option 2", [] { return State().checklist[1]; });
    test.KeyPress(ImGuiKey_Tab | ImGuiMod_Shift);  // back to the filter field
    test.Type("999");
    test.Wait(2);
    test.MouseMove(boxRow("lists.checklist", 0));
    test.MouseDown();
    test.MouseUp();
    test.ExpectNear("select all checks the entries that match the filter", [] { return CountChecked(State().checklist); },
                    3.0, 0.0);
    test.Expect("one of them is Option 999", [] { return State().checklist[998]; });

    // TransferList: four rows a side in each frame (side by side, or stacked in a narrow width).
    test.Do([] { std::fill(std::begin(State().transfer), std::end(State().transfer), false); });
    const auto transferRow = [](int side, int row) {
        return T::Item(side == 0 ? "lists.transfer.source" : "lists.transfer.target", 0.5f, 0.0f)
            .Offset(0.0f, 4.0f + 32.0f * (static_cast<float>(row) + 0.5f));
    };
    test.Reveal("lists.transfer");
    // Source rows: Option 1, Option 2, Option 3, Option 4 (disabled), Option 5.
    test.MouseMove(transferRow(0, 1));
    test.MouseDown();
    test.MouseUp();
    test.MouseDown();
    test.MouseUp();
    test.Expect("a double click moves Option 2 to the chosen list", [] { return State().transfer[1]; });
    test.MouseMove(transferRow(0, 0));
    test.MouseDown();
    test.MouseUp();
    test.KeyPress(ImGuiKey_Tab);  // to Add
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("Tab to Add and Enter move the selected Option 1", [] { return State().transfer[0]; });
    test.Reveal("lists.transfer.frames");  // the drag resolves its points at its start
    // Dropped on the last row of the source list, the place nearest to the target list in both layouts.
    // Each point is resolved when the pointer gets there, as the page may scroll while the lists stack.
    test.MouseMove(transferRow(1, 0));
    test.MouseDown();
    test.MouseMove(transferRow(1, 1));
    test.MouseMove(transferRow(0, 3));
    test.Wait(10);
    test.MouseUp();
    test.Wait(2);
    test.Wait(2);
    test.Expect("dragging Option 1 back returns it alone",
                [] { return !State().transfer[0] && State().transfer[1]; });
}
