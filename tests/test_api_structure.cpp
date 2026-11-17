// The frozen API of the structure families (lists, navigation, containers, layout, overlays):
// every options struct follows the conventions, every planned widget is safe to call inside a
// real frame (stacks balanced, no value written, the neutral result), and the shared
// placeholder of src/widgets/structure_stub.h sizes, truncates and closes as documented.

#include <array>
#include <span>
#include <string>
#include <vector>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "core/core_internal.h"
#include "internal/scope_stack.h"
#include "support/options_check.h"
#include "support/session.h"
#include "widgets/structure_stub.h"

namespace
{

using facet::test::OptionsFollowConventions;

// lists.h
static_assert(OptionsFollowConventions<facet::DropdownOptions>());
static_assert(OptionsFollowConventions<facet::MultiDropdownOptions>());
static_assert(OptionsFollowConventions<facet::ComboOptions>());
static_assert(OptionsFollowConventions<facet::ComboItemOptions>());
static_assert(OptionsFollowConventions<facet::ListBoxOptions>());
static_assert(OptionsFollowConventions<facet::SelectableListOptions>());
static_assert(OptionsFollowConventions<facet::ReorderableListOptions>());
static_assert(OptionsFollowConventions<facet::TreeViewOptions>());
static_assert(OptionsFollowConventions<facet::TreeNodeOptions>());
static_assert(OptionsFollowConventions<facet::CheckboxTreeOptions>());
static_assert(OptionsFollowConventions<facet::TransferListOptions>());
static_assert(OptionsFollowConventions<facet::SearchableChecklistOptions>());
// containers.h
static_assert(OptionsFollowConventions<facet::ShellOptions>());
static_assert(OptionsFollowConventions<facet::AppFrameOptions>());
static_assert(OptionsFollowConventions<facet::TitleBarOptions>());
static_assert(OptionsFollowConventions<facet::OverlayOptions>());
static_assert(OptionsFollowConventions<facet::MiniWindowOptions>());
static_assert(OptionsFollowConventions<facet::ToolbarOptions>());
static_assert(OptionsFollowConventions<facet::ToolbarButtonOptions>());
static_assert(OptionsFollowConventions<facet::StatusBarOptions>());
static_assert(OptionsFollowConventions<facet::StatusItemOptions>());
static_assert(OptionsFollowConventions<facet::AccordionOptions>());
static_assert(OptionsFollowConventions<facet::AccordionSectionOptions>());
static_assert(OptionsFollowConventions<facet::DisclosureOptions>());
static_assert(OptionsFollowConventions<facet::CardOptions>());
static_assert(OptionsFollowConventions<facet::GroupBoxOptions>());
static_assert(OptionsFollowConventions<facet::ScrollAreaOptions>());
static_assert(OptionsFollowConventions<facet::VirtualListOptions>());
static_assert(OptionsFollowConventions<facet::InspectorPaneOptions>());
// layout.h
static_assert(OptionsFollowConventions<facet::StackOptions>());
static_assert(OptionsFollowConventions<facet::GridOptions>());
static_assert(OptionsFollowConventions<facet::GridCellOptions>());
static_assert(OptionsFollowConventions<facet::FlowOptions>());
static_assert(OptionsFollowConventions<facet::SplitViewOptions>());
static_assert(OptionsFollowConventions<facet::PanelsOptions>());
static_assert(OptionsFollowConventions<facet::PanelOptions>());
static_assert(OptionsFollowConventions<facet::ListDetailOptions>());
static_assert(OptionsFollowConventions<facet::SettingsRowOptions>());
static_assert(OptionsFollowConventions<facet::DependentGroupOptions>());
static_assert(OptionsFollowConventions<facet::SeparatorOptions>());
static_assert(OptionsFollowConventions<facet::SeparatorTextOptions>());
static_assert(OptionsFollowConventions<facet::ItemGridOptions>());
// navigation.h
static_assert(OptionsFollowConventions<facet::SidebarOptions>());
static_assert(OptionsFollowConventions<facet::SidebarItemOptions>());
static_assert(OptionsFollowConventions<facet::SidebarSectionOptions>());
static_assert(OptionsFollowConventions<facet::TabBarOptions>());
static_assert(OptionsFollowConventions<facet::MenuBarOptions>());
static_assert(OptionsFollowConventions<facet::MenuOptions>());
static_assert(OptionsFollowConventions<facet::MenuItemOptions>());
static_assert(OptionsFollowConventions<facet::BreadcrumbsOptions>());
static_assert(OptionsFollowConventions<facet::PaginationOptions>());
static_assert(OptionsFollowConventions<facet::StepIndicatorOptions>());
static_assert(OptionsFollowConventions<facet::WizardOptions>());
static_assert(OptionsFollowConventions<facet::WizardFooterOptions>());
static_assert(OptionsFollowConventions<facet::ShortcutSheetOptions>());
// overlays.h
static_assert(OptionsFollowConventions<facet::ModalOptions>());
static_assert(OptionsFollowConventions<facet::ConfirmDialogOptions>());
static_assert(OptionsFollowConventions<facet::PopoverOptions>());
static_assert(OptionsFollowConventions<facet::ContextMenuOptions>());
static_assert(OptionsFollowConventions<facet::TooltipOptions>());
static_assert(OptionsFollowConventions<facet::ToastOptions>());
static_assert(OptionsFollowConventions<facet::DrawerOptions>());
static_assert(OptionsFollowConventions<facet::LoadingOverlayOptions>());
static_assert(OptionsFollowConventions<facet::CommandPaletteOptions>());
static_assert(OptionsFollowConventions<facet::WatermarkOptions>());
static_assert(OptionsFollowConventions<facet::UnsavedChangesBarOptions>());

// Every value a stub receives through a pointer or a writable span. The stubs must leave all
// of it alone.
struct Values
{
    int index = 2;
    std::array<bool, 3> checked = {true, false, true};
    std::array<facet::CheckState, 3> states = {facet::CheckState::On, facet::CheckState::Mixed,
                                               facet::CheckState::Off};
    facet::ReorderMove move = {7, 8};
    bool open = true;
    bool closed = false;
    bool pinned = true;
    bool visible = true;
    float ratio = 0.25f;
    std::array<float, 2> sizes = {0.4f, 0.6f};
    int step = 1;
    int page = 3;
    int picked = 4;
    facet::ShortcutId pickedShortcut = {5};
    facet::FramePlacement placement = {
        .position = {10.0f, 20.0f}, .size = {300.0f, 200.0f}, .scale = 1.5f, .maximized = true};

    bool operator==(const Values& o) const
    {
        const facet::FramePlacement& p = placement;
        const facet::FramePlacement& q = o.placement;
        return index == o.index && checked == o.checked && states == o.states && move.from == o.move.from &&
               move.to == o.move.to && open == o.open && closed == o.closed && pinned == o.pinned &&
               visible == o.visible && ratio == o.ratio && sizes == o.sizes && step == o.step && page == o.page &&
               picked == o.picked && pickedShortcut == o.pickedShortcut && p.position.x == q.position.x &&
               p.position.y == q.position.y && p.size.x == q.size.x && p.size.y == q.size.y &&
               p.scale == q.scale && p.maximized == q.maximized;
    }
};

// The Dear ImGui and Facet stacks a stub must leave as it found them, the placeholder frame's
// narrowing of the window included, plus the values and the usage error count.
struct Snapshot
{
    int ids = 0;
    int windows = 0;
    int groups = 0;
    int scopes = 0;
    int errors = 0;
    ImGuiItemFlags itemFlags = 0;
    float indent = 0.0f;
    float workMaxX = 0.0f;
    float contentMaxX = 0.0f;
    Values values;

    static Snapshot Take(const Values& values)
    {
        ImGuiContext& g = *GImGui;
        Snapshot s;
        s.ids = g.CurrentWindow->IDStack.Size;
        s.windows = g.CurrentWindowStack.Size;
        s.groups = g.GroupStack.Size;
        s.scopes = facet::internal::OpenScopeCount();
        s.errors = facet::internal::UsageErrorCount();
        s.itemFlags = g.CurrentItemFlags;
        s.indent = g.CurrentWindow->DC.Indent.x;
        s.workMaxX = g.CurrentWindow->WorkRect.Max.x;
        s.contentMaxX = g.CurrentWindow->ContentRegionRect.Max.x;
        s.values = values;
        return s;
    }

    bool operator==(const Snapshot&) const = default;
};

// Runs a statement and checks that it left every stack, the window rect and the values as they
// were and reported no usage error.
#define CHECK_BALANCED(...)                                                                        \
    do                                                                                             \
    {                                                                                              \
        const Snapshot before_ = Snapshot::Take(v);                                                \
        __VA_ARGS__;                                                                               \
        const Snapshot after_ = Snapshot::Take(v);                                                 \
        CHECK_MESSAGE(after_ == before_, #__VA_ARGS__);                                            \
    } while (false)

const facet::ChoiceItem kItems[] = {{"Option 1"}, {"Option 2"}, {"Option 3"}};
const char* const kLabels[] = {"Option 1", "Option 2", "Option 3"};
const facet::CheckTreeItem kTree[] = {{"Group 1"}, {.label = "Option 1", .depth = 1}, {.label = "Option 2", .depth = 1}};
const facet::WatermarkField kFields[] = {{"Field 1", "Value 1"}};

bool Getter(void*, int index, facet::ChoiceItem* out)
{
    out->label = kLabels[index % 3];
    return true;
}

// Calls every function of the five headers once, checking balance and the neutral result.
void CallEveryStub(Values& v)
{
    ImGuiSelectionBasicStorage selection;
    const std::span<bool> checked(v.checked);
    const std::span<facet::CheckState> states(v.states);

    // --- lists.h
    CHECK_BALANCED(CHECK_FALSE(facet::Dropdown("Dropdown", &v.index, kItems)));
    CHECK_BALANCED(CHECK_FALSE(facet::Dropdown("Dropdown labels", &v.index, kLabels)));
    CHECK_BALANCED(CHECK_FALSE(facet::Dropdown("Dropdown getter", &v.index, 3, &Getter, nullptr)));
    CHECK_BALANCED(CHECK_FALSE(facet::MultiDropdown("Multi", checked, kItems)));
    CHECK_BALANCED(CHECK_FALSE(facet::MultiDropdown("Multi labels", checked, kLabels)));
    CHECK_BALANCED(if (facet::BeginCombo("Combo", "Option 1")) {
        FAIL("BeginCombo opened");
        facet::EndCombo();
    });
    CHECK_BALANCED(CHECK_FALSE(facet::ComboItem("Option 1", true)));
    CHECK_BALANCED(CHECK_FALSE(facet::ListBox("List", &v.index, kItems)));
    CHECK_BALANCED(CHECK_FALSE(facet::ListBox("List labels", &v.index, kLabels)));
    CHECK_BALANCED(CHECK_FALSE(facet::ListBox("List getter", &v.index, 3, &Getter, nullptr)));
    CHECK_BALANCED(CHECK_FALSE(facet::SelectableList("Selectable", &selection, kItems)));
    CHECK_BALANCED(CHECK_FALSE(facet::SelectableList("Selectable labels", &selection, kLabels)));
    CHECK_BALANCED(CHECK_FALSE(facet::SelectableList("Selectable getter", &selection, 3, &Getter, nullptr)));
    CHECK_BALANCED(CHECK(facet::SelectableListActivatedRow() == -1));
    CHECK_BALANCED(CHECK_FALSE(facet::ReorderableList("Reorder", &v.move, kItems)));
    CHECK_BALANCED(CHECK_FALSE(facet::ReorderableList("Reorder labels", &v.move, kLabels)));
    CHECK_BALANCED({
        const bool open = facet::BeginTreeView("Tree", &selection);
        CHECK(open);
        if (open)
        {
            if (facet::BeginTreeNode("Node"))
            {
                FAIL("BeginTreeNode opened");
                facet::EndTreeNode();
            }
            CHECK_FALSE(facet::TreeLeaf("Leaf"));
            facet::EndTreeView();
        }
    });
    CHECK_BALANCED(CHECK_FALSE(facet::TreeViewSelectionChanged()));
    CHECK_BALANCED(CHECK_FALSE(facet::CheckboxTree("Check tree", states, kTree)));
    CHECK_BALANCED(CHECK_FALSE(facet::TransferList("Transfer", checked, kItems)));
    CHECK_BALANCED(CHECK_FALSE(facet::TransferList("Transfer labels", checked, kLabels)));
    CHECK_BALANCED(CHECK_FALSE(facet::SearchableChecklist("Checklist", checked, kItems)));
    CHECK_BALANCED(CHECK_FALSE(facet::SearchableChecklist("Checklist labels", checked, kLabels)));
    CHECK_BALANCED(CHECK_FALSE(facet::SearchableChecklist("Checklist getter", checked, 3, &Getter, nullptr)));
    CHECK(selection.Size == 0);

    // --- containers.h
    CHECK_BALANCED(if (facet::BeginShell("Shell", &v.open)) {
        FAIL("BeginShell opened");
        facet::EndShell();
    });
    CHECK_BALANCED(if (facet::BeginShell("Shell without close", nullptr)) {
        FAIL("BeginShell opened");
        facet::EndShell();
    });
    CHECK_BALANCED({
        const bool open = facet::BeginAppFrame("App");
        CHECK(open);
        if (open)
        {
            ImGui::TextUnformatted("child");
            facet::EndAppFrame();
        }
    });
    CHECK_BALANCED(CHECK(facet::AppFrameHitTest(ImVec2(1.0f, 1.0f)) == facet::FrameHit::Client));
    CHECK_BALANCED(CHECK(facet::AppFrameMinSize().x == 0.0f));
    CHECK_BALANCED(CHECK(facet::AppFrameMaxSize().y == 0.0f));
    CHECK_BALANCED(facet::SaveAppFramePlacement("app", v.placement));
    CHECK_BALANCED(CHECK_FALSE(facet::LoadAppFramePlacement("app", &v.placement)));
    CHECK_BALANCED({
        facet::BeginTitleBar("Title", {.windowButtons = true});
        ImGui::TextUnformatted("child");
        facet::EndTitleBar();
    });
    CHECK_BALANCED(CHECK(facet::TitleBarPressedButton() == facet::WindowButton::None));
    CHECK_BALANCED({
        const bool open = facet::BeginOverlay("Overlay", &v.visible);
        CHECK(open);
        if (open)
            facet::EndOverlay();
    });
    CHECK_BALANCED(CHECK_FALSE(facet::OverlayWantsMouse()));
    CHECK_BALANCED(CHECK_FALSE(facet::OverlayWantsKeyboard()));
    CHECK_BALANCED(CHECK_FALSE(facet::OverlayHitTest(ImVec2(1.0f, 1.0f))));
    CHECK_BALANCED({
        std::array<facet::ScreenRegion, 2> regions;
        CHECK(facet::GetOverlayInputRegions(regions) == 0);
    });
    CHECK_BALANCED(if (facet::BeginMiniWindow("Mini", &v.pinned)) {
        FAIL("BeginMiniWindow opened");
        facet::EndMiniWindow();
    });
    CHECK_BALANCED({
        facet::BeginToolbar("Toolbar");
        CHECK_FALSE(facet::ToolbarButton("Add", facet::Icon("plus")));
        facet::ToolbarSeparator();
        facet::ToolbarSpacer();
        facet::EndToolbar();
    });
    CHECK_BALANCED({
        facet::BeginStatusBar("Status");
        facet::StatusItem("Item 1");
        facet::EndStatusBar();
    });
    CHECK_BALANCED({
        facet::BeginAccordion("Accordion");
        if (facet::BeginAccordionSection("Section"))
        {
            FAIL("BeginAccordionSection opened");
            facet::EndAccordionSection();
        }
        facet::EndAccordion();
    });
    CHECK_BALANCED(if (facet::BeginDisclosure("Disclosure", &v.open)) {
        FAIL("BeginDisclosure opened");
        facet::EndDisclosure();
    });
    CHECK_BALANCED({
        facet::BeginCard("Card", {.title = "Title"});
        ImGui::TextUnformatted("child");
        facet::EndCard();
    });
    CHECK_BALANCED({
        const bool open = facet::BeginGroupBox("Group");
        CHECK(open);
        if (open)
            facet::EndGroupBox();
    });
    CHECK_BALANCED({
        const bool open = facet::BeginScrollArea("Scroll");
        CHECK(open);
        if (open)
            facet::EndScrollArea();
    });
    CHECK_BALANCED({
        const bool open = facet::BeginVirtualList("Virtual", 100, &selection);
        CHECK(open);
        if (open)
        {
            facet::RowRange rows;
            while (facet::NextVisibleRows(&rows))
                for (int row = rows.first; row < rows.last; ++row)
                    facet::VirtualListRow(row);
            CHECK_FALSE(facet::VirtualListRow(0));
            facet::EndVirtualList();
        }
    });
    CHECK_BALANCED(facet::SetNextVirtualListScrollTo(10));
    CHECK_BALANCED({
        facet::BeginInspectorPane("Inspector", &v.visible);
        ImGui::TextUnformatted("main");
        if (facet::InspectorPaneNext())
            ImGui::TextUnformatted("pane");
        facet::EndInspectorPane();
    });

    // --- layout.h
    CHECK_BALANCED({
        facet::BeginStack("Stack");
        ImGui::TextUnformatted("child");
        facet::StackSpacer();
        facet::EndStack();
    });
    CHECK_BALANCED({
        facet::BeginGrid("Grid");
        facet::GridNext();
        ImGui::TextUnformatted("cell");
        facet::EndGrid();
    });
    CHECK_BALANCED({
        facet::BeginFlow("Flow");
        CHECK(facet::FlowNext());
        facet::EndFlow();
    });
    CHECK_BALANCED({
        facet::BeginSplitView("Split", &v.ratio);
        ImGui::TextUnformatted("first");
        facet::SplitViewNext();
        ImGui::TextUnformatted("second");
        facet::EndSplitView();
    });
    CHECK_BALANCED({
        facet::BeginPanels("Panels", v.sizes);
        const bool open = facet::BeginPanel("Panel");
        CHECK(open);
        if (open)
            facet::EndPanel();
        facet::EndPanels();
    });
    CHECK_BALANCED({
        facet::BeginListDetail("ListDetail", &v.closed);
        if (facet::BeginListPane())
            facet::EndListPane();
        if (facet::BeginDetailPane())
            facet::EndDetailPane();
        facet::EndListDetail();
    });
    CHECK_BALANCED({
        facet::BeginSettingsRow("Row");
        ImGui::TextUnformatted("control");
        facet::EndSettingsRow();
    });
    CHECK_BALANCED(CHECK_FALSE(facet::IsSettingsRowResetPressed()));
    CHECK_BALANCED({
        const bool open = facet::BeginDependentGroup("Dependent", false);
        CHECK(open);
        if (open)
            facet::EndDependentGroup();
    });
    CHECK_BALANCED(facet::Separator());
    CHECK_BALANCED(facet::SeparatorText("Section"));
    CHECK_BALANCED({
        const bool open = facet::BeginItemGrid("Items", 50, &selection);
        CHECK(open);
        if (open)
        {
            CHECK_FALSE(facet::ItemGridTile(0));
            facet::EndItemGrid();
        }
    });

    // --- navigation.h
    CHECK_BALANCED({
        const bool open = facet::BeginSidebar("Sidebar");
        CHECK(open);
        if (open)
        {
            CHECK_FALSE(facet::SidebarItem("Page 1", facet::Icon("settings"), {.selected = true}));
            if (facet::BeginSidebarSection("Section"))
                facet::EndSidebarSection();
            facet::EndSidebar();
        }
    });
    CHECK_BALANCED(CHECK_FALSE(facet::TabBar("Tabs", &v.index, kItems)));
    CHECK_BALANCED(CHECK_FALSE(facet::TabBar("Tabs labels", &v.index, kLabels)));
    CHECK_BALANCED(CHECK(facet::TabBarClosedTab() == -1));
    CHECK_BALANCED({
        const bool open = facet::BeginMenuBar("Menus");
        CHECK(open);
        if (open)
        {
            if (facet::BeginMenu("Menu 1"))
            {
                FAIL("BeginMenu opened");
                facet::EndMenu();
            }
            facet::EndMenuBar();
        }
    });
    CHECK_BALANCED(CHECK_FALSE(facet::MenuItem("Item")));
    CHECK_BALANCED(CHECK_FALSE(facet::MenuItem("Check item", &v.open)));
    CHECK_BALANCED(CHECK_FALSE(facet::MenuItem("Radio item", &v.index, 0)));
    CHECK_BALANCED(CHECK(facet::Breadcrumbs("Path", kItems) == -1));
    CHECK_BALANCED(CHECK(facet::Breadcrumbs("Path labels", kLabels) == -1));
    CHECK_BALANCED(CHECK_FALSE(facet::Pagination("Pages", &v.page, 10)));
    CHECK_BALANCED(facet::StepIndicator("Steps", 1, kItems));
    CHECK_BALANCED(facet::StepIndicator("Steps labels", 1, kLabels));
    CHECK_BALANCED(CHECK_FALSE(facet::StepIndicator("Steps edit", &v.step, kItems)));
    CHECK_BALANCED(CHECK_FALSE(facet::StepIndicator("Steps edit labels", &v.step, kLabels)));
    CHECK_BALANCED({
        facet::BeginWizard("Wizard", &v.step, kItems);
        ImGui::TextUnformatted("step");
        CHECK(facet::WizardFooter() == facet::WizardAction::None);
        facet::EndWizard();
    });
    CHECK_BALANCED({
        facet::WizardScope wizard("Wizard labels", &v.step, kLabels);
        ImGui::TextUnformatted("step");
    });
    CHECK_BALANCED(facet::ShortcutSheet("Shortcuts", &v.open));

    // --- overlays.h
    CHECK_BALANCED(if (facet::BeginModal("Modal", &v.open)) {
        FAIL("BeginModal opened");
        facet::EndModal();
    });
    CHECK_BALANCED(CHECK(facet::ConfirmDialog("Confirm", &v.open, "Message") == facet::DialogResult::None));
    CHECK_BALANCED(if (facet::BeginPopover("Popover", &v.open)) {
        FAIL("BeginPopover opened");
        facet::EndPopover();
    });
    CHECK_BALANCED(if (facet::BeginContextMenu("Context")) {
        FAIL("BeginContextMenu opened");
        facet::EndContextMenu();
    });
    CHECK_BALANCED(facet::ItemTooltip("Tooltip %d", 1));
    CHECK_BALANCED(if (facet::BeginRichTooltip()) {
        FAIL("BeginRichTooltip opened");
        facet::EndRichTooltip();
    });
    CHECK_BALANCED({
        const facet::ToastId toast = facet::ShowToast("Toast");
        CHECK_FALSE(toast.IsValid());
        facet::UpdateToast(toast, "Toast");
        CHECK_FALSE(facet::ToastActionPressed(toast));
        CHECK_FALSE(facet::IsToastVisible(toast));
        facet::DismissToast(toast);
        facet::SetToastCorner(facet::ScreenCorner::TopLeft);
    });
    CHECK_BALANCED(if (facet::BeginDrawer("Drawer", &v.open)) {
        FAIL("BeginDrawer opened");
        facet::EndDrawer();
    });
    CHECK_BALANCED({
        facet::BeginLoadingOverlay("Loading", {.loading = true});
        ImGui::TextUnformatted("child");
        facet::EndLoadingOverlay();
    });
    CHECK_BALANCED(CHECK_FALSE(facet::CommandPalette("Commands", &v.open, &v.picked, kItems)));
    CHECK_BALANCED(CHECK_FALSE(facet::CommandPalette("Command labels", &v.open, &v.picked, kLabels)));
    CHECK_BALANCED(CHECK_FALSE(facet::CommandPalette("Shortcuts palette", &v.open, &v.pickedShortcut)));
    CHECK_BALANCED(facet::WatermarkOverlay("Watermark", kFields));
    CHECK_BALANCED(CHECK(facet::UnsavedChangesBar("Changes", 2) == facet::UnsavedChangesAction::None));
}

// Every vertex the draw list received since `from` lies within `rect`, give or take the
// anti-aliasing fringe.
bool VerticesInside(const ImDrawList* drawList, int from, const ImRect& rect)
{
    for (int i = from; i < drawList->VtxBuffer.Size; ++i)
    {
        const ImVec2 p = drawList->VtxBuffer[i].pos;
        if (p.x < rect.Min.x - 1.0f || p.x > rect.Max.x + 1.0f || p.y < rect.Min.y - 1.0f || p.y > rect.Max.y + 1.0f)
            return false;
    }
    return true;
}

int VerticesWithColor(const ImDrawList* drawList, int from, ImU32 color)
{
    int count = 0;
    for (int i = from; i < drawList->VtxBuffer.Size; ++i)
        count += drawList->VtxBuffer[i].col == color ? 1 : 0;
    return count;
}

} // namespace

TEST_CASE("Every structure widget stub is safe to call, also disabled, and returns the neutral result")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    const int errorsBefore = facet::internal::UsageErrorCount();
    Values v;
    session.SetUi([&] {
        CallEveryStub(v);
        ImGui::BeginDisabled();
        CallEveryStub(v);
        ImGui::EndDisabled();
    });
    session.Frames(2);
    CHECK(v == Values{});
    // facet::EndFrame found no scope left open.
    CHECK(facet::internal::UsageErrorCount() == errorsBefore);
    CHECK(facet::internal::OpenScopeCount() == 0);
}

TEST_CASE("A structure placeholder takes its width by the precedence of rule 6")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int index = 0;
    float available = 0.0f;
    ImGuiID defaultId = 0;
    session.SetUi([&] {
        available = ImGui::GetContentRegionAvail().x;
        defaultId = ImGui::GetID("default");
        facet::Dropdown("units", &index, kItems, {.width = 100.0f});
        session.Track("units");
        facet::Dropdown("fill", &index, kItems, {.width = facet::kFill});
        session.Track("fill");
        facet::Dropdown("minus", &index, kItems, {.width = -20.0f});
        session.Track("minus");
        ImGui::SetNextItemWidth(150.0f);
        facet::Dropdown("next", &index, kItems);
        session.Track("next");
        ImGui::PushItemWidth(170.0f);
        facet::Dropdown("pushed", &index, kItems);
        session.Track("pushed");
        ImGui::PopItemWidth();
        facet::Dropdown("default", &index, kItems);
        session.Track("default");
        facet::Pagination("content", &index, 10);
        session.Track("content");
        facet::Dropdown("small", &index, kItems, {.size = facet::ControlSize::Small});
        session.Track("small");
    });
    session.Frame();

    CHECK(session.ItemRect("units").GetWidth() == doctest::Approx(facet::Px(100.0f)));
    CHECK(session.ItemRect("fill").GetWidth() == doctest::Approx(available));
    CHECK(session.ItemRect("minus").GetWidth() == doctest::Approx(available - facet::Px(20.0f)));
    CHECK(session.ItemRect("next").GetWidth() == doctest::Approx(150.0f));
    CHECK(session.ItemRect("pushed").GetWidth() == doctest::Approx(170.0f));
    CHECK(session.ItemRect("default").GetWidth() == doctest::Approx(available));
    const float content = session.ItemRect("content").GetWidth();
    CHECK(content == doctest::Approx(facet::internal::StubContentWidth("Pagination", "content")));
    CHECK(content < available);
    CHECK(session.ItemRect("default").GetHeight() == doctest::Approx(facet::ControlHeight(facet::ControlSize::Regular)));
    CHECK(session.ItemRect("small").GetHeight() == doctest::Approx(facet::ControlHeight(facet::ControlSize::Small)));
    CHECK(session.ItemId("default") == defaultId);
}

TEST_CASE("A narrow structure placeholder truncates its text inside its outline")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int index = 0;
    int from = 0;
    ImDrawList* drawList = nullptr;
    session.SetUi([&] {
        drawList = ImGui::GetWindowDrawList();
        from = drawList->VtxBuffer.Size;
        facet::Dropdown("A label that is far too long for the box", &index, kItems, {.width = 60.0f});
        session.Track("narrow");
        CHECK(VerticesInside(drawList, from, ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax())));
        CHECK(drawList->VtxBuffer.Size > from);
    });
    session.Frame();
    CHECK(session.ItemRect("narrow").GetWidth() == doctest::Approx(facet::Px(60.0f)));
}

TEST_CASE("A structure placeholder dims its text inside BeginDisabled")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    const facet::Theme saved = facet::T();
    facet::SetTheme(facet::Theme::Light(), false);
    const ImU32 secondary = facet::ToU32(facet::T().colors.labelSecondary);
    const ImU32 tertiary = facet::ToU32(facet::T().colors.labelTertiary);
    REQUIRE(secondary != tertiary);
    int index = 0;
    int enabledSecondary = 0;
    int disabledSecondary = 0;
    int disabledTertiary = 0;
    session.SetUi([&] {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        int from = drawList->VtxBuffer.Size;
        facet::Dropdown("enabled", &index, kItems);
        enabledSecondary = VerticesWithColor(drawList, from, secondary);
        ImGui::BeginDisabled();
        from = drawList->VtxBuffer.Size;
        facet::Dropdown("disabled", &index, kItems);
        disabledSecondary = VerticesWithColor(drawList, from, secondary);
        disabledTertiary = VerticesWithColor(drawList, from, tertiary);
        ImGui::EndDisabled();
    });
    session.Frame();
    facet::SetTheme(saved, false);
    CHECK(enabledSecondary > 0);
    CHECK(disabledSecondary == 0);
    CHECK(disabledTertiary > 0);
}

TEST_CASE("Tab skips structure placeholders and a click on one activates nothing")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int index = 0;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        facet::Dropdown("planned", &index, kItems);
        session.Track("planned");
        facet::BeginCard("card");
        ImGui::Button("inside");
        session.Track("inside");
        facet::EndCard();
        ImGui::Button("after");
        session.Track("after");
    });
    session.Frame();
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("before"));
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("inside"));
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("after"));
    session.Click(session.ItemCenter("planned"));
    CHECK_FALSE(ImGui::IsAnyItemActive());
    CHECK(index == 0);
}

TEST_CASE("A structure container placeholder frames its content inside the available width")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float available = 0.0f;
    float contentRight = 0.0f;
    ImRect inside;
    session.SetUi([&] {
        available = ImGui::GetContentRegionAvail().x;
        const float left = ImGui::GetCursorScreenPos().x;
        facet::BeginCard("card");
        ImGui::Button("inside", ImVec2(-FLT_MIN, 0.0f));
        session.Track("inside");
        contentRight = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
        facet::EndCard();
        session.Track("card");
        inside = ImRect(ImVec2(left, 0.0f), ImVec2(left + available, 0.0f));
    });
    session.Frame();
    const ImRect card = session.ItemRect("card");
    const ImRect button = session.ItemRect("inside");
    // The card spans the available width and its content sits inside it with the inset.
    CHECK(card.GetWidth() == doctest::Approx(available));
    CHECK(button.Min.x > card.Min.x);
    CHECK(button.Max.x < card.Max.x);
    CHECK(button.Max.y < card.Max.y);
    CHECK(contentRight < inside.Max.x);
}

TEST_CASE("An End without its Begin is reported and changes no stack")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values v;
    int reported = 0;
    session.SetUi([&] {
        const int before = facet::internal::UsageErrorCount();
        const Snapshot start = Snapshot::Take(v);
        facet::EndCard();
        facet::EndCombo();
        facet::EndShell();
        Snapshot end = Snapshot::Take(v);
        reported = facet::internal::UsageErrorCount() - before;
        end.errors = start.errors;
        CHECK(end == start);
    });
    session.Frame();
    CHECK(reported == 3);
}

TEST_CASE("An outer End closes the placeholder frames left open inside it")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values v;
    int reported = 0;
    session.SetUi([&] {
        const int before = facet::internal::UsageErrorCount();
        const Snapshot start = Snapshot::Take(v);
        facet::BeginCard("outer");
        facet::BeginSettingsRow("row");   // pushes an id and is never ended
        facet::BeginStack("stack");       // never ended
        facet::EndCard();
        Snapshot end = Snapshot::Take(v);
        reported = facet::internal::UsageErrorCount() - before;
        end.errors = start.errors;
        CHECK(end == start);
    });
    session.Frame();
    CHECK(reported == 2);
}

TEST_CASE("A placeholder frame left open at the end of a frame is reported and forgotten")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool leaveOpen = true;
    Values v;
    float windowLeft = 0.0f;
    session.SetUi([&] {
        if (leaveOpen)
        {
            facet::BeginCard("open");
            return;
        }
        // The next frame starts clean: a balanced pair reports nothing and draws its frame.
        windowLeft = ImGui::GetCursorScreenPos().x;
        CHECK_BALANCED({
            facet::BeginCard("next");
            ImGui::Button("inside");
            session.Track("inside");
            facet::EndCard();
        });
    });
    // More frames left open than the placeholder stack holds: if they were kept, the stack
    // would be full and the next card would draw no frame.
    const int before = facet::internal::UsageErrorCount();
    constexpr int kLeftOpen = facet::internal::kScopeStackDepth + 4;
    session.Frames(kLeftOpen);
    CHECK(facet::internal::UsageErrorCount() == before + kLeftOpen);
    CHECK(facet::internal::OpenScopeCount() == 0);
    leaveOpen = false;
    session.Frame();
    CHECK(facet::internal::UsageErrorCount() == before + kLeftOpen);
    CHECK(session.ItemRect("inside").Min.x > windowLeft);
}

TEST_CASE("Structure stubs outside a frame draw nothing and return the neutral result")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    session.Frame();
    // Between frames: no frame scope.
    const int before = facet::internal::UsageErrorCount();
    int index = 1;
    CHECK_FALSE(facet::Dropdown("outside", &index, kItems));
    CHECK_FALSE(facet::BeginScrollArea("outside"));
    CHECK_FALSE(facet::BeginTreeView("outside", nullptr));
    facet::BeginCard("outside");
    facet::EndCard();
    CHECK(index == 1);
    CHECK(facet::internal::OpenScopeCount() == 0);
    CHECK(facet::internal::UsageErrorCount() == before);
}

TEST_CASE("Scope classes of the structure families end only what began")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values v;
    session.SetUi([&] {
        CHECK_BALANCED({
            facet::CardScope card("card");
            facet::StackScope stack("stack");
            facet::SettingsRowScope row("row");
        });
        CHECK_BALANCED({
            facet::ComboScope combo("combo", "Option 1");
            CHECK_FALSE(static_cast<bool>(combo));
        });
        CHECK_BALANCED({
            facet::ScrollAreaScope area("area");
            CHECK(static_cast<bool>(area));
        });
        CHECK_BALANCED({
            facet::ModalScope modal("modal", &v.open);
            CHECK_FALSE(static_cast<bool>(modal));
        });
    });
    session.Frame();
}

TEST_CASE("Structure stubs return false where their own arguments hide the content")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values v;
    session.SetUi([&] {
        bool hidden = false;
        CHECK_BALANCED({
            facet::BeginInspectorPane("Hidden pane", &hidden);
            CHECK_FALSE(facet::InspectorPaneNext());
            facet::EndInspectorPane();
        });
        CHECK_BALANCED({
            facet::BeginInspectorPane("Pane without a flag", nullptr);
            CHECK(facet::InspectorPaneNext());
            facet::EndInspectorPane();
        });
        CHECK_BALANCED(CHECK_FALSE(facet::BeginDependentGroup("Hidden group", false, {.mode = facet::DependentMode::Hide})));
        CHECK_BALANCED({
            const bool open = facet::BeginDependentGroup("Shown group", true, {.mode = facet::DependentMode::Hide});
            CHECK(open);
            if (open)
                facet::EndDependentGroup();
        });
        CHECK_BALANCED(CHECK_FALSE(facet::BeginSidebar("Hidden sidebar", {.mode = facet::SidebarMode::Hidden})));
        CHECK_BALANCED(CHECK_FALSE(facet::BeginSidebarSection("Closed section", {.collapsible = true, .defaultOpen = false})));
        CHECK_BALANCED(CHECK_FALSE(facet::BeginOverlay("Hidden overlay", &hidden)));
        CHECK_BALANCED(CHECK_FALSE(facet::BeginOverlay("Overlay without a flag", nullptr)));
        CHECK_BALANCED(CHECK_FALSE(facet::BeginGroupBox("Closed group box", {.collapsible = true, .defaultOpen = false})));
        CHECK_BALANCED(CHECK_FALSE(facet::BeginVirtualList("Loading list", 10, nullptr, {.state = facet::DataState::Loading})));
        CHECK_BALANCED(CHECK_FALSE(facet::BeginVirtualList("Empty list", 0, nullptr)));
        CHECK_BALANCED(CHECK_FALSE(facet::BeginItemGrid("Failed grid", 10, nullptr, {.state = facet::DataState::Error})));
        CHECK_BALANCED(CHECK_FALSE(facet::BeginItemGrid("Empty grid", 0, nullptr)));
        CHECK_BALANCED(CHECK_FALSE(facet::BeginTreeView("Loading tree", nullptr, {.state = facet::DataState::Loading})));
        CHECK(hidden == false);
    });
    session.Frame();
}

TEST_CASE("A list-detail placeholder shows one pane in a Compact width and both otherwise")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    struct Shown
    {
        bool list = false;
        bool detail = false;
    };
    Shown compactList;
    Shown compactDetail;
    Shown regular;
    const auto run = [](const char* id, bool detailOpen) {
        Shown shown;
        facet::BeginListDetail(id, &detailOpen);
        shown.list = facet::BeginListPane();
        if (shown.list)
            facet::EndListPane();
        shown.detail = facet::BeginDetailPane();
        if (shown.detail)
            facet::EndDetailPane();
        facet::EndListDetail();
        return shown;
    };
    session.SetUi([&] {
        const facet::Theme& theme = facet::T();
        const float height = facet::Px(theme.metrics.controlHeightLarge * 4.0f);
        ImGui::BeginChild("compact", ImVec2(facet::Px(theme.layout.regularFrom * 0.5f), height));
        CHECK(facet::AvailableWidthClass() == facet::WidthClass::Compact);
        compactList = run("list", false);
        compactDetail = run("detail", true);
        ImGui::EndChild();
        ImGui::BeginChild("regular", ImVec2(facet::Px(theme.layout.regularFrom + 40.0f), height));
        CHECK(facet::AvailableWidthClass() != facet::WidthClass::Compact);
        regular = run("both", true);
        ImGui::EndChild();
    });
    session.Frame();
    CHECK(compactList.list);
    CHECK_FALSE(compactList.detail);
    CHECK_FALSE(compactDetail.list);
    CHECK(compactDetail.detail);
    CHECK(regular.list);
    CHECK(regular.detail);
}

TEST_CASE("ApplyReorderMove moves one element and shifts the others")
{
    // `to` is the moved element's index after the move.
    std::vector<int> data = {0, 1, 2, 3, 4};
    facet::ApplyReorderMove(data, {1, 3});
    CHECK(data == std::vector<int>{0, 2, 3, 1, 4});
    CHECK(data[3] == 1);
    facet::ApplyReorderMove(data, {3, 0});
    CHECK(data == std::vector<int>{1, 0, 2, 3, 4});
    facet::ApplyReorderMove(data, {4, 4});
    facet::ApplyReorderMove(data, {-1, 2});
    facet::ApplyReorderMove(data, {2, 5});
    CHECK(data == std::vector<int>{1, 0, 2, 3, 4});
    CHECK_FALSE(facet::ReorderMove{}.IsValid());
    CHECK(facet::ReorderMove{0, 1}.IsValid());
}

TEST_CASE("ApplyReorderMove takes the caller's containers as they are")
{
    // A C array, a std::array, a std::span and a std::vector<bool> without spelling a span.
    int array[3] = {0, 1, 2};
    facet::ApplyReorderMove(array, {0, 2});
    CHECK(array[0] == 1);
    CHECK(array[1] == 2);
    CHECK(array[2] == 0);
    std::array<facet::ChoiceItem, 3> items = {{{"Option 1"}, {"Option 2"}, {"Option 3"}}};
    facet::ApplyReorderMove(items, {2, 0});
    CHECK(std::string(items[0].label) == "Option 3");
    CHECK(std::string(items[1].label) == "Option 1");
    std::vector<int> data = {0, 1, 2};
    facet::ApplyReorderMove(std::span<int>(data).subspan(1), {0, 1});
    CHECK(data == std::vector<int>{0, 2, 1});
    std::vector<bool> flags = {true, false, false};
    facet::ApplyReorderMove(flags, {0, 2});
    CHECK(flags == std::vector<bool>{false, false, true});
}
