// Demo page: Lists. One section per widget of facet/widgets/lists.h with its important variants
// and states, capture states for the open, focused and dragged looks, and a selftest that operates
// every ready widget by mouse and keyboard. A widget that is still planned draws its placeholder.
// Sample entries are neutral ("Option 1") with built-in icons; generated entries are numbered.

#include <cstdio>
#include <string>

#include <imgui.h>

#include <facet/facet.h>

#include "gallery.h"
#include "pages/pages.h"

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
    {.label = "Option 3", .depth = 1},
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

    // The planned widgets below.
    int listBox = 1;
    int combo = 0;
    bool multi[5] = {true, false, true, false, false};
    bool checklist[kChecklistCount] = {};
    bool transfer[5] = {false, true, false, false, false};
    facet::CheckState checkTree[5] = {facet::CheckState::Mixed, facet::CheckState::On, facet::CheckState::Off,
                                      facet::CheckState::Off, facet::CheckState::Off};
    facet::ChoiceItem reorder[4] = {{.label = "Option 1", .id = 1}, {.label = "Option 2", .id = 2},
                                    {.label = "Option 3", .id = 3}, {.label = "Option 4", .id = 4}};
    ImGuiSelectionBasicStorage selection;
    ImGuiSelectionBasicStorage treeSelection;
};

ListsState& State()
{
    static ListsState state;
    return state;
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

void ChooseOne(ListsState& s)
{
    SectionHeader("Custom rows and list boxes");
    LabeledRow("Custom rows");
    if (facet::BeginCombo("##combo", kLabels[s.combo]))
    {
        for (int i = 0; i < 4; ++i)
            if (facet::ComboItem(kLabels[i], s.combo == i, {.detail = "Detail"}))
                s.combo = i;
        facet::EndCombo();
    }
    LabeledRow("List box");
    facet::ListBox("##listbox", &s.listBox, kLabels, {.visibleRows = 4});
}

void ChooseSeveral(ListsState& s)
{
    SectionHeader("Choose several values");
    LabeledRow("Multi dropdown");
    facet::MultiDropdown("##multi", s.multi, kOptions, {.selectAllRow = true});
    Paragraph("A checklist over a thousand generated entries with a filter field, and a transfer "
              "list that moves entries between two lists.");
    facet::SearchableChecklist("Checklist", s.checklist, kChecklistCount, &NumberedOption, nullptr,
                               {.visibleRows = 4});
    facet::TransferList("transfer", s.transfer, kOptions,
                        {.sourceTitle = "Available", .targetTitle = "Chosen", .visibleRows = 4});
}

void SelectAndReorder(ListsState& s)
{
    SectionHeader("Select and reorder");
    facet::SelectableList("Selectable list", &s.selection, kOptions, {.visibleRows = 4});
    facet::ReorderMove move;
    if (facet::ReorderableList("reorder", &move, s.reorder))
        facet::ApplyReorderMove(s.reorder, move);
}

void Hierarchies(ListsState& s)
{
    SectionHeader("Hierarchies");
    if (facet::BeginTreeView("tree", &s.treeSelection))
    {
        if (facet::BeginTreeNode("Group 1", {.icon = facet::Icon("folder"), .defaultOpen = true}))
        {
            facet::TreeLeaf("Option 1", {.icon = facet::Icon("file")});
            facet::TreeLeaf("Option 2", {.icon = facet::Icon("file")});
            facet::EndTreeNode();
        }
        facet::TreeLeaf("Option 3", {.icon = facet::Icon("file")});
        facet::EndTreeView();
    }
    facet::CheckboxTree("checktree", s.checkTree, kCheckTree);
}

} // namespace

FACET_DEMO_PAGE("lists", "Lists", "Widgets", 60, "default", "dropdown-open", "dropdown-search", "focused")
{
    facet::demo::PageHeader("Lists",
                            "Dropdowns, combo boxes, list boxes, selectable and reorderable lists, tree views, "
                            "checkbox trees, transfer lists and checklists.");
    facet::demo::PlannedFamilyNote("lists");
    ListsState& s = State();
    DropdownSection(ctx, s);
    ChooseOne(s);
    ChooseSeveral(s);
    SelectAndReorder(s);
    Hierarchies(s);
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
    });

    // Dropdown, pointer: a click opens the list, a click on a row chooses it. The rows start below the
    // field: the anchor gap, the list padding and the area padding (28 units) before row 0, 32 per row.
    test.Click("lists.dropdown");
    test.Click("lists.dropdown");  // a click on the field of the open list closes it
    test.ExpectNear("a click on the open field closes without choosing", [] { return State().writes; }, 0.0, 0.0);
    test.Click("lists.dropdown");
    test.MouseMove(T::Item("lists.dropdown", 0.7f, 1.0f).Offset(0.0f, 28.0f + 32.0f * 2.0f));
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
    test.KeyPress(ImGuiKey_Enter);
    test.ExpectNear("a disabled dropdown does not change", [] { return State().states[0]; }, 1.0, 0.0);
}
