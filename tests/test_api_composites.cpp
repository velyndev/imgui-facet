// The declared API of the composites family (facet/widgets/composites.h) while its widgets are
// placeholders: every function is called once inside a real Dear ImGui frame, in a window and
// again inside ImGui::BeginDisabled. After each call the id, window, group and Facet scope stacks
// must be as before, a widget must have submitted one item of non-zero size with the id of its
// label, the neutral result must come back and no value argument may have changed. Every options
// struct is checked against the conventions at compile time. DataTable, charts and media have
// files of their own (tests/test_api_data_table.cpp, test_api_charts.cpp, test_api_media.cpp).

#include <cmath>
#include <cstddef>
#include <cstring>
#include <type_traits>
#include <string>
#include <vector>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "core/core_internal.h"
#include "internal/scope_stack.h"
#include "support/content_stub_check.h"
#include "support/options_check.h"
#include "support/session.h"

namespace
{

using facet::test::OptionsFollowConventions;
using namespace facet::test::content;

TEST_CASE("Values that mean none default to NaN, outside every value range")
{
    // -1 would be a valid x for a range that includes it.
    CHECK(std::isnan(facet::CurveEditorOptions{}.marker));
}

// Entries that can change order carry a stable id (rule 2 of the conventions).
static_assert(!HasEntryId<facet::LogEntry>);  // the check itself works
static_assert(HasEntryId<facet::FilterChip>);
static_assert(HasEntryId<facet::KeybindListItem>);
static_assert(HasEntryId<facet::PresetItem>);
static_assert(HasEntryId<facet::InventoryItem>);
static_assert(HasEntryId<facet::LayerItem>);

// Views of caller data have the data block of rule 7 (HasDataBlock in support/content_stub_check.h).
static_assert(HasDataBlock<facet::ConsoleViewOptions>());
static_assert(HasDataBlock<facet::KeybindListOptions>());
static_assert(HasDataBlock<facet::PresetListOptions>());
static_assert(HasDataBlock<facet::InventoryGridOptions>());
static_assert(HasDataBlock<facet::ProfileSwitcherOptions>());
static_assert(HasDataBlock<facet::LayerListOptions>());

// Every widget that writes a value, a selection or item members has readOnly (rule 7).
static_assert(HasReadOnly<facet::CurveEditorOptions>);
static_assert(HasReadOnly<facet::BezierEditorOptions>);
static_assert(HasReadOnly<facet::GradientEditorOptions>);
static_assert(HasReadOnly<facet::ConsoleViewOptions>);
static_assert(HasReadOnly<facet::SearchFilterBarOptions>);
static_assert(HasReadOnly<facet::KeybindListOptions>);
static_assert(HasReadOnly<facet::PresetListOptions>);
static_assert(HasReadOnly<facet::InventoryGridOptions>);
static_assert(HasReadOnly<facet::ProfileSwitcherOptions>);
static_assert(HasReadOnly<facet::CanvasElementOptions>);
static_assert(HasReadOnly<facet::LayerListOptions>);
static_assert(HasReadOnly<facet::ThemeModeControlOptions>);
static_assert(HasReadOnly<facet::AccentPickerOptions>);
static_assert(HasReadOnly<facet::UiScaleControlOptions>);

static_assert(OptionsFollowConventions<facet::CurveEditorOptions>());
static_assert(OptionsFollowConventions<facet::BezierEditorOptions>());
static_assert(OptionsFollowConventions<facet::GradientEditorOptions>());
static_assert(OptionsFollowConventions<facet::PropertyGridOptions>());
static_assert(OptionsFollowConventions<facet::PropertyCategoryOptions>());
static_assert(OptionsFollowConventions<facet::PropertyRowOptions>());
static_assert(OptionsFollowConventions<facet::ConsoleViewOptions>());
static_assert(OptionsFollowConventions<facet::SearchFilterBarOptions>());
static_assert(OptionsFollowConventions<facet::SettingsSearchOptions>());
static_assert(OptionsFollowConventions<facet::SettingsSearchMatchOptions>());
static_assert(OptionsFollowConventions<facet::KeybindListOptions>());
static_assert(OptionsFollowConventions<facet::PresetListOptions>());
static_assert(OptionsFollowConventions<facet::InventoryGridOptions>());
static_assert(OptionsFollowConventions<facet::InventoryTileOptions>());
static_assert(OptionsFollowConventions<facet::InventorySlotOptions>());
static_assert(OptionsFollowConventions<facet::InventoryDetailCardOptions>());
static_assert(OptionsFollowConventions<facet::MediaCardOptions>());
static_assert(OptionsFollowConventions<facet::UserChipOptions>());
static_assert(OptionsFollowConventions<facet::ProfileSwitcherOptions>());
static_assert(OptionsFollowConventions<facet::PreviewCanvasOptions>());
static_assert(OptionsFollowConventions<facet::CanvasElementOptions>());
static_assert(OptionsFollowConventions<facet::LayerListOptions>());
static_assert(OptionsFollowConventions<facet::OnboardingOptions>());
static_assert(OptionsFollowConventions<facet::ThemeModeControlOptions>());
static_assert(OptionsFollowConventions<facet::AccentPickerOptions>());
static_assert(OptionsFollowConventions<facet::UiScaleControlOptions>());
static_assert(OptionsFollowConventions<facet::DragSourceOptions>());
static_assert(OptionsFollowConventions<facet::AcceptDropOptions>());
void EditorCalls()
{
    using namespace facet;
    ImVec2 points[4] = {{0.0f, 0.0f}, {0.5f, 0.25f}, {1.0f, 1.0f}};
    int count = 3;
    const Snapshot pointsBefore(points);
    Check("CurveEditor", Expect::Item, "Curve", [&] {
        CHECK_FALSE(CurveEditor("Curve", points, &count, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), {.marker = 0.5f}));
    });
    CHECK(count == 3);
    CHECK(pointsBefore.Unchanged());
    std::vector<ImVec2> pointVector = {{0.0f, 0.0f}, {1.0f, 1.0f}};
    Check("CurveEditor, vector", Expect::Item, "Curve 2", [&] {
        CHECK_FALSE(CurveEditor("Curve 2", &pointVector, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f)));
    });
    CHECK(pointVector.size() == 2);

    CubicBezier curve{{0.25f, 0.1f}, {0.25f, 1.0f}};
    const Snapshot curveBefore(curve);
    Check("BezierEditor", Expect::Item, "Easing", [&] { CHECK_FALSE(BezierEditor("Easing", &curve)); });
    CHECK(curveBefore.Unchanged());

    GradientStop stops[3] = {{0.0f}, {1.0f, {1.0f, 1.0f, 1.0f, 1.0f}}};
    int stopCount = 2;
    const Snapshot stopsBefore(stops);
    Check("GradientEditor", Expect::Item, "Gradient", [&] {
        CHECK_FALSE(GradientEditor("Gradient", stops, &stopCount, {.alpha = true}));
    });
    CHECK(stopCount == 2);
    CHECK(stopsBefore.Unchanged());
    std::vector<GradientStop> stopVector = {{0.0f}, {1.0f}};
    Check("GradientEditor, vector", Expect::Item, "Gradient 2", [&] {
        CHECK_FALSE(GradientEditor("Gradient 2", &stopVector));
    });
    CHECK(stopVector.size() == 2);

    // Open categories and rows take content; the row pushes its label on the id stack.
    Check("PropertyGrid", Expect::NoItem, nullptr, [] {
        BeginPropertyGrid("grid", {.filter = "prop"});
        const bool category = BeginPropertyCategory("Category");
        CHECK(category);
        if (category)
        {
            const ImGuiID outside = ImGui::GetID("##value");
            const bool row = BeginPropertyRow("Property", {.modified = true});
            CHECK(row);
            if (row)
            {
                CHECK(ImGui::GetID("##value") != outside);
                ImGui::Button("##value");
                EndPropertyRow();
            }
            CHECK(ImGui::GetID("##value") == outside);
            EndPropertyCategory();
        }
        CHECK_FALSE(BeginPropertyCategory("Closed", {.defaultOpen = false}));
        EndPropertyGrid();
    });
    Check("PropertyGridScope", Expect::NoItem, nullptr, [] {
        PropertyGridScope grid("grid2");
        if (PropertyCategoryScope category("Category"); category)
            if (PropertyRowScope row("Property"); row)
                ImGui::TextUnformatted("value");
    });
}

void ToolCalls()
{
    using namespace facet;
    static const LogEntry lines[] = {{"Line 1"}};
    static const char* const commands[] = {"command"};
    const LogGetter getter = [](void*, int index, LogEntry* out) {
        *out = lines[index];
        return true;
    };
    std::string command = "text";
    char buffer[16] = "text";
    Check("ConsoleView", Expect::Item, "console", [&] {
        CHECK_FALSE(ConsoleView("console", &command, lines, {.commands = commands, .height = 80.0f}));
    });
    Check("ConsoleView, getter", Expect::Item, "console 2",
          [&] { CHECK_FALSE(ConsoleView("console 2", &command, 1, getter, nullptr, {.height = 80.0f})); });
    Check("ConsoleView, buffer", Expect::Item, "console 3",
          [&] { CHECK_FALSE(ConsoleView("console 3", buffer, sizeof(buffer), lines, {.height = 80.0f})); });
    Check("ConsoleView, buffer and getter", Expect::Item, "console 4", [&] {
        CHECK_FALSE(ConsoleView("console 4", buffer, sizeof(buffer), 1, getter, nullptr, {.height = 80.0f}));
    });
    CHECK(command == "text");
    CHECK(std::strcmp(buffer, "text") == 0);

    FilterChip filters[] = {{"Filter 1"}, {"Filter 2", {}, true}};
    const Snapshot filtersBefore(filters);
    Check("SearchFilterBar", Expect::Item, "Search", [&] {
        CHECK_FALSE(SearchFilterBar("Search", &command, filters, {.resultCount = 3}));
    });
    Check("SearchFilterBar, buffer", Expect::Item, "Search 2",
          [&] { CHECK_FALSE(SearchFilterBar("Search 2", buffer, sizeof(buffer), filters)); });
    CHECK(command == "text");
    CHECK(std::strcmp(buffer, "text") == 0);
    CHECK(filtersBefore.Unchanged());

    Check("SettingsSearch", Expect::NoItem, nullptr, [] {
        BeginSettingsSearch("settings", {.filter = "query"});
        CHECK(SettingsSearchMatch("Row", {.keywords = "word", .section = "Section"}));
        EndSettingsSearch();
    });
    Check("SettingsSearchScope", Expect::NoItem, nullptr, [] { SettingsSearchScope search("settings 2"); });
    Check("SettingsSearchHitCount", Expect::NoItem, nullptr,
          [] { CHECK(SettingsSearchHitCount("settings", nullptr) == 0); });

    Check("KeybindList, registry", Expect::Item, "binds", [] { CHECK_FALSE(KeybindList("binds", {.filter = "x"})); });
    KeybindListItem binds[] = {{"Action", "Group", {ImGuiKey_A}, {ImGuiKey_B}}};
    const Snapshot bindsBefore(binds);
    Check("KeybindList, items", Expect::Item, "binds 2", [&] { CHECK_FALSE(KeybindList("binds 2", binds)); });
    CHECK(bindsBefore.Unchanged());

    static const PresetItem presets[] = {{"Preset 1"}, {"Preset 2", true}};
    int active = 1;
    Check("PresetList", Expect::Item, "presets", [&] {
        const PresetListResult result = PresetList("presets", &active, presets);
        CHECK(result.action == PresetAction::None);
        CHECK(result.index == -1);
        CHECK(result.name == nullptr);
    });
    CHECK(active == 1);

    ThemeMode mode = ThemeMode::Dark;
    Check("ThemeModeControl", Expect::Item, "Theme", [&] { CHECK_FALSE(ThemeModeControl("Theme", &mode)); });
    CHECK(mode == ThemeMode::Dark);
    ImVec4 accent(0.1f, 0.2f, 0.3f, 1.0f);
    const Snapshot accentBefore(accent);
    bool themeDefault = false;
    Check("AccentPicker", Expect::Item, "Accent",
          [&] { CHECK_FALSE(AccentPicker("Accent", &themeDefault, &accent)); });
    Check("AccentPicker, no theme default", Expect::Item, "Accent 2",
          [&] { CHECK_FALSE(AccentPicker("Accent 2", nullptr, &accent)); });
    CHECK(accentBefore.Unchanged());
    CHECK_FALSE(themeDefault);
    float scale = 1.25f;
    Check("UiScaleControl", Expect::Item, "Scale",
          [&] { CHECK_FALSE(UiScaleControl("Scale", &scale, 0.75f, 2.0f, {.wheelZoom = true})); });
    CHECK(scale == 1.25f);
}

void InventoryCalls()
{
    using namespace facet;
    static const InventoryItem slots[] = {{"Item 1", {}, Icon("file"), 2}, {}, {"Item 2"}};
    const auto neutral = [](const InventoryResult& result) {
        return result.action == InventoryAction::None && result.source == 0 && result.from == -1 &&
               result.to == -1 && result.count == 0;
    };
    int selected = 2;
    Check("InventoryGrid", Expect::Item, "bag", [&] { CHECK(neutral(InventoryGrid("bag", &selected, slots))); });
    Check("InventoryGrid, columns", Expect::Item, "bag 2",
          [&] { CHECK(neutral(InventoryGrid("bag 2", &selected, slots, {.columns = 2, .slotSize = 40.0f}))); });
    CHECK(selected == 2);
    Check("InventoryTile", Expect::Item, "tile", [] { CHECK_FALSE(InventoryTile("tile", slots[0], {.selected = true})); });
    Check("InventorySlot", Expect::Item, "Slot", [&] { CHECK(neutral(InventorySlot("Slot", slots[1]))); });

    static const InventoryStat stats[] = {{"Stat", 2.0, 1.0}};
    static const char* const actions[] = {"Action"};
    Check("InventoryDetailCard", Expect::Item, "Item", [] {
        CHECK(InventoryDetailCard("Item", ImageHandle{}, {.stats = stats, .actions = actions}) == -1);
    });
    Check("MediaCard", Expect::Item, "Card", [] { CHECK_FALSE(MediaCard("Card", ImageHandle{}, {.selected = true})); });
    Check("UserChip", Expect::Item, "Name", [] { CHECK_FALSE(UserChip("Name", ImageHandle{}, {.description = "Detail"})); });
    Check("UserChip, compact", Expect::Item, "Name 2", [] { CHECK_FALSE(UserChip("Name 2", ImageHandle{}, {.compact = true})); });

    static const AvatarItem profiles[] = {{"Profile 1"}, {"Profile 2"}};
    int current = 0;
    Check("ProfileSwitcher", Expect::Item, "profiles", [&] {
        CHECK(ProfileSwitcher("profiles", &current, profiles, {.addLabel = "Add"}) == ProfileSwitcherAction::None);
    });
    CHECK(current == 0);
}

void CanvasCalls()
{
    using namespace facet;
    CanvasElementState element{CanvasAnchor::Center, {10.0f, 20.0f}, {100.0f, 50.0f}};
    const Snapshot elementBefore(element);
    Check("PreviewCanvas", Expect::NoItem, nullptr, [&] {
        BeginPreviewCanvas("canvas", ImVec2(1920.0f, 1080.0f), {.gridSpacing = 120.0f});
        CHECK_FALSE(CanvasElement("Element", &element, {.resizable = true}));
        EndPreviewCanvas();
    });
    Check("PreviewCanvasScope, empty size", Expect::NoItem, nullptr,
          [] { PreviewCanvasScope canvas("canvas 2", ImVec2(0.0f, 0.0f)); });
    Check("CanvasElement", Expect::Item, "Element 2", [&] { CHECK_FALSE(CanvasElement("Element 2", &element)); });
    CHECK(elementBefore.Unchanged());

    LayerItem layers[] = {{"Layer 1"}, {"Layer 2", {}, 1, false, true}};
    const Snapshot layersBefore(layers);
    int selected = 1;
    Check("LayerList", Expect::Item, "layers", [&] {
        const LayerListResult result = LayerList("layers", &selected, layers);
        CHECK(result.action == LayerAction::None);
        CHECK(result.index == -1);
        CHECK_FALSE(result.move.IsValid());
    });
    CHECK(selected == 1);
    CHECK(layersBefore.Unchanged());

    // A shown onboarding panel draws its placeholder and reports closed; a hidden one draws nothing.
    bool open = true;
    int step = 1;
    Check("BeginOnboarding, open", Expect::Item, "Welcome", [&] { CHECK_FALSE(BeginOnboarding("Welcome", &open, &step, 3)); });
    CHECK(open);
    CHECK(step == 1);
    bool closed = false;
    Check("BeginOnboarding, closed", Expect::NoItem, nullptr,
          [&] { CHECK_FALSE(BeginOnboarding("Welcome 2", &closed, &step, 3)); });
    CHECK_FALSE(closed);
    Check("OnboardingScope", Expect::NoItem, nullptr, [&] {
        OnboardingScope onboarding("Welcome 3", &closed, &step, 3);
        CHECK_FALSE(static_cast<bool>(onboarding));
    });
}

void DragDropCalls()
{
    using namespace facet;
    const int payload = 7;
    Check("BeginDragSource", Expect::NoItem, nullptr, [&] {
        ImGui::Button("source");
        if (BeginDragSource("test.item", &payload, sizeof(payload), {.previewText = "Item"}))
            EndDragSource();
        DragSourceScope scope("test.item", &payload, sizeof(payload));
        CHECK_FALSE(static_cast<bool>(scope));
    });
    Check("BeginDropTarget", Expect::NoItem, nullptr, [] {
        ImGui::Button("target");
        if (BeginDropTarget())
            EndDropTarget();
        CHECK(AcceptDrop("test.item", {.indicator = DropIndicator::LineBefore, .peek = true}) == nullptr);
        DropTargetScope scope;
        CHECK_FALSE(static_cast<bool>(scope));
    });
}

void CompositeCalls()
{
    EditorCalls();
    ToolCalls();
    InventoryCalls();
    CanvasCalls();
    DragDropCalls();
}

} // namespace

TEST_CASE("The composite stubs submit one item each, return the neutral result and keep every stack")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int runs = 0;
    RunFrame(session, &runs, &CompositeCalls);
    CHECK(runs == 1);
    CHECK(facet::internal::OpenScopeCount() == 0);
}

TEST_CASE("Composite stubs follow kFill and widths relative to the available width")
{
    // Rule 6: kFill is the available width, any other negative width the available width minus
    // that many units, for card and dialog stubs too.
    facet::test::Session session;
    REQUIRE(session.Ready());
    int checked = 0;
    session.SetUi([&] {
        using namespace facet;
        const auto fills = [&](const char* widget, float units, auto&& call) {
            INFO("widget: " << std::string(widget) << ", width " << units);
            const float expected = ImGui::GetContentRegionAvail().x + (units == kFill ? 0.0f : Px(units));
            call(units);
            CHECK(ImGui::GetItemRectSize().x == doctest::Approx(expected));
            ++checked;
        };
        for (const float units : {kFill, -10.0f})
        {
            fills("InventoryDetailCard", units, [](float w) { InventoryDetailCard("Item", ImageHandle{}, {.width = w}); });
            fills("MediaCard", units, [](float w) { MediaCard("Card", ImageHandle{}, {.width = w}); });
            fills("Onboarding", units, [](float w) {
                bool open = true;
                int step = 0;
                if (BeginOnboarding("Welcome", &open, &step, 2, {.width = w}))
                    EndOnboarding();
            });
        }
    });
    session.Frame();
    CHECK(checked == 6);
}

TEST_CASE("Content-sized composite stubs are wide enough to name the widget and say planned")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int checked = 0;
    session.SetUi([&] {
        using namespace facet;
        const auto wide = [&](const char* widget, auto&& call) {
            INFO("widget: " << std::string(widget));
            call();
            char text[64];
            ImFormatString(text, sizeof(text), "%s: planned", widget);
            CHECK(ImGui::GetItemRectSize().x >= MeasureText(TextStyle::Footnote, text).x);
            ++checked;
        };
        const InventoryItem item{"Item"};
        ThemeMode mode = ThemeMode::Light;
        ImVec4 accent(0.1f, 0.2f, 0.3f, 1.0f);
        CanvasElementState element;
        wide("InventoryTile", [&] { InventoryTile("tile", item); });
        wide("InventorySlot", [&] { InventorySlot("A", item); });
        wide("UserChip", [] { UserChip("A", ImageHandle{}, {.compact = true}); });
        wide("ThemeModeControl", [&] { ThemeModeControl("##theme", &mode); });
        wide("AccentPicker", [&] { AccentPicker("##accent", nullptr, &accent, {.customColor = false}); });
        BeginPreviewCanvas("canvas", ImVec2(100.0f, 100.0f));
        wide("CanvasElement", [&] { CanvasElement("A", &element); });
        EndPreviewCanvas();
    });
    session.Frame();
    CHECK(checked == 6);
}

TEST_CASE("A composite End without its Begin is reported and changes no Dear ImGui stack")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int reported = 0;
    session.SetUi([&] {
        const Stacks before = CurrentStacks();
        const int errors = facet::internal::UsageErrorCount();
        facet::EndPropertyRow();
        facet::EndPropertyGrid();
        facet::EndDragSource();
        facet::EndDropTarget();
        facet::EndPropertyCategory();
        facet::EndSettingsSearch();
        facet::EndPreviewCanvas();
        facet::EndOnboarding();
        reported = facet::internal::UsageErrorCount() - errors;
        Stacks after = CurrentStacks();
        after.usageErrors = before.usageErrors;
        CHECK(after == before);
    });
    session.Frame();
    CHECK(reported == 8);
}