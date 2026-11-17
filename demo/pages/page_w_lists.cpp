// Demo page: Lists. Calls every v1 widget of facet/widgets/lists.h once, grouped by purpose.
// The widgets are planned, so each call draws its placeholder; Phase 2 replaces them one by one
// on this page. Sample entries are neutral ("Option 1") with built-in icons.

#include <cstdio>

#include <imgui.h>

#include <facet/facet.h>

#include "gallery.h"
#include "pages/pages.h"

namespace
{

using facet::demo::LabeledRow;
using facet::demo::Note;
using facet::demo::Paragraph;
using facet::demo::SectionHeader;

const facet::ChoiceItem kOptions[] = {
    {.label = "Option 1", .icon = facet::Icon("file")},
    {.label = "Option 2", .icon = facet::Icon("folder")},
    {.label = "Option 3", .icon = facet::Icon("image"), .group = "Group 2"},
    {.label = "Option 4", .icon = facet::Icon("layers"), .group = "Group 2"},
};

const char* const kLabels[] = {"Option 1", "Option 2", "Option 3", "Option 4"};

const facet::CheckTreeItem kCheckTree[] = {
    {.label = "Group 1", .icon = facet::Icon("folder")},
    {.label = "Option 1", .depth = 1},
    {.label = "Option 2", .depth = 1},
    {.label = "Group 2", .icon = facet::Icon("folder")},
    {.label = "Option 3", .depth = 1},
};

constexpr int kGeneratedCount = 1000;

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
    int dropdown = 0;
    int searchable = -1;
    int listBox = 1;
    int combo = 0;
    bool multi[4] = {true, false, true, false};
    bool checklist[kGeneratedCount] = {};
    bool transfer[4] = {false, true, false, false};
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

void ChooseOne(ListsState& s)
{
    SectionHeader("Choose one value");
    LabeledRow("Dropdown");
    facet::Dropdown("##dropdown", &s.dropdown, kOptions);
    LabeledRow("Searchable");
    facet::Dropdown("##searchable", &s.searchable, kGeneratedCount, &NumberedOption, nullptr, {.searchable = true});
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
    facet::SearchableChecklist("Checklist", s.checklist, kGeneratedCount, &NumberedOption, nullptr,
                               {.visibleRows = 4});
    facet::TransferList("transfer", s.transfer, kOptions,
                        {.sourceTitle = "Available", .targetTitle = "Chosen", .visibleRows = 4});
}

void SelectAndReorder(ListsState& s)
{
    SectionHeader("Select and reorder");
    facet::SelectableList("Selectable list", &s.selection, kOptions, {.visibleRows = 4});
    facet::ReorderMove move;
    if (facet::ReorderableList("reorder", s.reorder, &move))
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

FACET_DEMO_PAGE("lists", "Lists", "Widgets", 60)
{
    facet::demo::PageHeader("Lists",
                            "Dropdowns, combo boxes, list boxes, selectable and reorderable lists, "
                            "tree views, checkbox trees, transfer lists and checklists. Every widget "
                            "on this page is a placeholder until Phase 2 implements it: it shows its "
                            "name and the word planned, and does not react to input.");
    Note("Declared in facet/widgets/lists.h; the declarations and usage examples are in "
         "docs/widgets/lists.md.");
    ListsState& s = State();
    ChooseOne(s);
    ChooseSeveral(s);
    SelectAndReorder(s);
    Hierarchies(s);
}
