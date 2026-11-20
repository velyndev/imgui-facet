// The frozen API of the structure families that are still grouped (navigation, layout, overlays):
// every options struct follows the conventions, every planned widget is safe to call inside a
// real frame (stacks balanced, no value written, the neutral result), and the shared placeholder
// of src/widgets/structure_stub.h sizes, truncates and closes as documented. The lists and
// containers families have their own API tests (tests/test_api_lists.cpp,
// tests/test_api_containers.cpp); the tests here use only widgets of the grouped families.

#include <cfloat>
#include <span>
#include <string>
#include <type_traits>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "support/options_check.h"
#include "support/session.h"
#include "support/structure_stub_check.h"
#include "widgets/structure_stub.h"

namespace
{

using facet::test::OptionsFollowConventions;
using namespace facet::test::structure;

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

// The item grid is a data view (rule 7).
static_assert(HasDataBlock<facet::ItemGridOptions>());

// Calls every function of layout.h, navigation.h and overlays.h once, checking balance and the
// neutral result.
void CallEveryStub(Values& v)
{
    ImGuiSelectionBasicStorage selection;
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
    CHECK_BALANCED(CHECK_FALSE(facet::SettingsRowResetPressed()));
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
    CHECK_BALANCED(CHECK_FALSE(facet::TabBar("Tabs reorder", &v.index, kItems, {.reorderable = true})));
    CHECK_BALANCED(CHECK_FALSE(facet::TabBarMovedTab().IsValid()));
    CHECK_FALSE(facet::TabBarOptions{}.reorderable);
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
    CHECK_BALANCED(CHECK_FALSE(facet::MenuCheckItem("Check item", &v.open)));
    CHECK_BALANCED(CHECK_FALSE(facet::MenuRadioItem("Radio item", &v.index, 0)));
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
    CHECK(selection.Size == 0);
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
        facet::TabBar("units", &index, kItems, {.width = 100.0f});
        session.Track("units");
        facet::TabBar("fill", &index, kItems, {.width = facet::kFill});
        session.Track("fill");
        facet::TabBar("minus", &index, kItems, {.width = -20.0f});
        session.Track("minus");
        ImGui::SetNextItemWidth(150.0f);
        facet::TabBar("next", &index, kItems);
        session.Track("next");
        ImGui::PushItemWidth(170.0f);
        facet::TabBar("pushed", &index, kItems);
        session.Track("pushed");
        ImGui::PopItemWidth();
        facet::TabBar("default", &index, kItems);
        session.Track("default");
        facet::Pagination("content", &index, 10);
        session.Track("content");
        facet::TabBar("small", &index, kItems, {.size = facet::ControlSize::Small});
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
        facet::TabBar("A label that is far too long for the box", &index, kItems, {.width = 60.0f});
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
        facet::TabBar("enabled", &index, kItems);
        enabledSecondary = VerticesWithColor(drawList, from, secondary);
        ImGui::BeginDisabled();
        from = drawList->VtxBuffer.Size;
        facet::TabBar("disabled", &index, kItems);
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
TEST_CASE("Menu item placeholders name their own function in the caption")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    const facet::Theme saved = facet::T();
    facet::SetTheme(facet::Theme::Light(), false);
    const ImU32 secondary = facet::ToU32(facet::T().colors.labelSecondary);
    // One glyph quad (4 vertices) per character that is not a space.
    const auto glyphVertices = [](const char* text) {
        int count = 0;
        for (const char* c = text; *c != '\0'; ++c)
            count += *c != ' ' ? 4 : 0;
        return count;
    };
    bool checked = false;
    int selection = 0;
    int plain = 0;
    int check = 0;
    int radio = 0;
    session.SetUi([&] {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        int from = drawList->VtxBuffer.Size;
        facet::MenuItem("Item 1");
        plain = VerticesWithColor(drawList, from, secondary);
        from = drawList->VtxBuffer.Size;
        facet::MenuCheckItem("Item 2", &checked);
        check = VerticesWithColor(drawList, from, secondary);
        from = drawList->VtxBuffer.Size;
        facet::MenuRadioItem("Item 3", &selection, 1);
        radio = VerticesWithColor(drawList, from, secondary);
    });
    session.Frame();
    facet::SetTheme(saved, false);
    CHECK(plain == glyphVertices("MenuItem: planned (Item 1)"));
    CHECK(check == glyphVertices("MenuCheckItem: planned (Item 2)"));
    CHECK(radio == glyphVertices("MenuRadioItem: planned (Item 3)"));
}

TEST_CASE("Tab skips structure placeholders and a click on one activates nothing")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int index = 0;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        facet::TabBar("planned", &index, kItems);
        session.Track("planned");
        facet::BeginStack("stack");
        ImGui::Button("inside");
        session.Track("inside");
        facet::EndStack();
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
        facet::BeginStack("stack");
        ImGui::Button("inside", ImVec2(-FLT_MIN, 0.0f));
        session.Track("inside");
        contentRight = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
        facet::EndStack();
        session.Track("stack");
        inside = ImRect(ImVec2(left, 0.0f), ImVec2(left + available, 0.0f));
    });
    session.Frame();
    const ImRect stack = session.ItemRect("stack");
    const ImRect button = session.ItemRect("inside");
    // The frame spans the available width and its content sits inside it with the inset.
    CHECK(stack.GetWidth() == doctest::Approx(available));
    CHECK(button.Min.x > stack.Min.x);
    CHECK(button.Max.x < stack.Max.x);
    CHECK(button.Max.y < stack.Max.y);
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
        facet::EndStack();
        facet::EndGrid();
        facet::EndModal();
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
        facet::BeginStack("outer");
        facet::BeginSettingsRow("row");   // pushes an id and is never ended
        facet::BeginGrid("grid");         // never ended
        facet::EndStack();
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
            facet::BeginStack("open");
            return;
        }
        // The next frame starts clean: a balanced pair reports nothing and draws its frame.
        windowLeft = ImGui::GetCursorScreenPos().x;
        CHECK_BALANCED({
            facet::BeginStack("next");
            ImGui::Button("inside");
            session.Track("inside");
            facet::EndStack();
        });
    });
    // More frames left open than the placeholder stack holds: if they were kept, the stack
    // would be full and the next frame placeholder would draw no frame.
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
    CHECK_FALSE(facet::TabBar("outside", &index, kItems));
    CHECK_FALSE(facet::BeginSidebar("outside"));
    facet::BeginStack("outside");
    facet::EndStack();
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
            facet::StackScope stack("stack");
            facet::SettingsRowScope row("row");
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
        CHECK_BALANCED(CHECK_FALSE(facet::BeginDependentGroup("Hidden group", false, {.mode = facet::DependentMode::Hide})));
        CHECK_BALANCED({
            const bool open = facet::BeginDependentGroup("Shown group", true, {.mode = facet::DependentMode::Hide});
            CHECK(open);
            if (open)
                facet::EndDependentGroup();
        });
        CHECK_BALANCED(CHECK_FALSE(facet::BeginSidebar("Hidden sidebar", {.mode = facet::SidebarMode::Hidden})));
        CHECK_BALANCED(CHECK_FALSE(facet::BeginSidebarSection("Closed section", {.collapsible = true, .defaultOpen = false})));
        CHECK_BALANCED(CHECK_FALSE(facet::BeginItemGrid("Failed grid", 10, nullptr, {.state = facet::DataState::Error})));
        CHECK_BALANCED(CHECK_FALSE(facet::BeginItemGrid("Empty grid", 0, nullptr)));
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

TEST_CASE("An outer list-detail placeholder keeps its state past a nested one")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values v;
    bool outerList = false;
    bool innerDetail = false;
    bool outerDetail = true;
    session.SetUi([&] {
        const facet::Theme& theme = facet::T();
        const float height = facet::Px(theme.metrics.controlHeightLarge * 8.0f);
        ImGui::BeginChild("compact", ImVec2(facet::Px(theme.layout.regularFrom * 0.5f), height));
        CHECK(facet::AvailableWidthClass() == facet::WidthClass::Compact);
        CHECK_BALANCED({
            bool outerOpen = false;
            bool innerOpen = true;
            facet::BeginListDetail("outer", &outerOpen);
            outerList = facet::BeginListPane();
            if (outerList)
            {
                facet::BeginListDetail("inner", &innerOpen);
                if (facet::BeginListPane())
                    facet::EndListPane();
                innerDetail = facet::BeginDetailPane();
                if (innerDetail)
                    facet::EndDetailPane();
                facet::EndListDetail();
                facet::EndListPane();
            }
            outerDetail = facet::BeginDetailPane();
            if (outerDetail)
                facet::EndDetailPane();
            facet::EndListDetail();
        });
        ImGui::EndChild();
    });
    session.Frame();
    CHECK(outerList);
    CHECK(innerDetail);
    CHECK_FALSE(outerDetail);
}

// MenuItem has no overload, so MenuItem("Save", {}) cannot bind to a value form with a null pointer.
static_assert(std::is_same_v<decltype(&facet::MenuItem), bool (*)(const char*, const facet::MenuItemOptions&)>);