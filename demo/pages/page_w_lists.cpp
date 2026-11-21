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
    if (ctx.capture && ctx.captureState == kFocused)
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
    if (facet::SelectableList("Rows", &s.selection, kOptions, {.visibleRows = 5, .width = 320.0f}))
        ++s.writes;
    ctx.Probe("lists.selectable");
    if (const int row = facet::SelectableListActivatedRow(); row >= 0)
        s.activated = row;
    Note("Selected rows: %d. Last activated: %s.", s.selection.Size, OptionName(s.activated));
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

void ComboSection(ListsState& s)
{
    SectionHeader("Combo with custom rows");
    LabeledRow("Custom rows");
    if (facet::BeginCombo("##combo", kLabels[s.combo]))
    {
        for (int i = 0; i < 4; ++i)
            if (facet::ComboItem(kLabels[i], s.combo == i, {.detail = "Detail"}))
                s.combo = i;
        facet::EndCombo();
    }
}

void ChooseSeveral(ListsState& s)
{
    SectionHeader("Choose several values");
    Paragraph("A checklist over a thousand generated entries with a filter field, and a transfer "
              "list that moves entries between two lists.");
    facet::SearchableChecklist("Checklist", s.checklist, kChecklistCount, &NumberedOption, nullptr,
                               {.visibleRows = 4});
    facet::TransferList("transfer", s.transfer, kOptions,
                        {.sourceTitle = "Available", .targetTitle = "Chosen", .visibleRows = 4});
}

void SelectAndReorder(ListsState& s)
{
    SectionHeader("Reorder");
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

FACET_DEMO_PAGE("lists", "Lists", "Widgets", 60, "default", "dropdown-open", "dropdown-search", "focused",
                 "multi-open")
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
    ComboSection(s);
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
}
