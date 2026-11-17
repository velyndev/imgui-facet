// The declared API of the content families (display, charts, media, composites) while their
// widgets are placeholders: every function is called once inside a real Dear ImGui frame, in a
// window and again inside ImGui::BeginDisabled. After each call the id, window, group and
// Facet scope stacks must be as before, a widget must have submitted one item of non-zero size
// with the id of its label, the neutral result must come back and no value argument may have
// changed. Every options struct is checked against the conventions at compile time.

#include <cmath>
#include <cstring>
#include <string>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "core/core_internal.h"
#include "internal/scope_stack.h"
#include "support/options_check.h"
#include "support/session.h"

namespace
{

using facet::test::OptionsFollowConventions;

static_assert(OptionsFollowConventions<facet::StyledTextOptions>());
static_assert(OptionsFollowConventions<facet::TruncatedTextOptions>());
static_assert(OptionsFollowConventions<facet::SectionHeaderOptions>());
static_assert(OptionsFollowConventions<facet::BannerOptions>());
static_assert(OptionsFollowConventions<facet::InlineAlertOptions>());
static_assert(OptionsFollowConventions<facet::EmptyStateOptions>());
static_assert(OptionsFollowConventions<facet::DataStateViewOptions>());
static_assert(OptionsFollowConventions<facet::ProgressBarOptions>());
static_assert(OptionsFollowConventions<facet::ProgressRingOptions>());
static_assert(OptionsFollowConventions<facet::SpinnerOptions>());
static_assert(OptionsFollowConventions<facet::SkeletonOptions>());
static_assert(OptionsFollowConventions<facet::BadgeOptions>());
static_assert(OptionsFollowConventions<facet::CountBadgeOptions>());
static_assert(OptionsFollowConventions<facet::StatusDotOptions>());
static_assert(OptionsFollowConventions<facet::StatusPillOptions>());
static_assert(OptionsFollowConventions<facet::KeyCapOptions>());
static_assert(OptionsFollowConventions<facet::ShortcutHintOptions>());
static_assert(OptionsFollowConventions<facet::CharacterCounterOptions>());
static_assert(OptionsFollowConventions<facet::InlineCodeOptions>());
static_assert(OptionsFollowConventions<facet::CodeViewOptions>());
static_assert(OptionsFollowConventions<facet::LogViewOptions>());
static_assert(OptionsFollowConventions<facet::KeyValueListOptions>());
static_assert(OptionsFollowConventions<facet::KeyValueOptions>());
static_assert(OptionsFollowConventions<facet::StatTileOptions>());
static_assert(OptionsFollowConventions<facet::DataTableOptions>());

// Log rows have one height, so the view can read only the visible entries.
TEST_CASE("Values that mean none default to NaN, outside every value range")
{
    // -1 would be a valid x or time for a range or clock that includes it.
    CHECK(std::isnan(facet::CurveEditorOptions{}.marker));
    CHECK(std::isnan(facet::LogEntry{}.time));
}

// Entries that can change order carry a stable id (rule 2 of the conventions).
template <class T>
concept HasEntryId = requires { requires std::is_same_v<decltype(T::id), ImGuiID>; };
static_assert(!HasEntryId<facet::LogEntry>);  // the check itself works
static_assert(HasEntryId<facet::LegendItem>);
static_assert(HasEntryId<facet::AvatarItem>);
static_assert(HasEntryId<facet::ImageItem>);
static_assert(HasEntryId<facet::FilterChip>);
static_assert(HasEntryId<facet::KeybindListItem>);
static_assert(HasEntryId<facet::PresetItem>);
static_assert(HasEntryId<facet::InventoryItem>);
static_assert(HasEntryId<facet::LayerItem>);

// One alignment enum for styled text and table columns.
static_assert(std::is_same_v<decltype(facet::StyledTextOptions::align), facet::ColumnAlign>);
static_assert(std::is_same_v<decltype(facet::DataTableColumn::align), facet::ColumnAlign>);

template <class T>
concept HasWrap = requires(T options) { options.wrap; };
static_assert(!HasWrap<facet::LogViewOptions>);
static_assert(HasWrap<facet::CodeViewOptions>);  // the check itself works

static_assert(OptionsFollowConventions<facet::SparklineOptions>());
static_assert(OptionsFollowConventions<facet::LineChartOptions>());
static_assert(OptionsFollowConventions<facet::BarChartOptions>());
static_assert(OptionsFollowConventions<facet::DonutChartOptions>());
static_assert(OptionsFollowConventions<facet::GaugeOptions>());
static_assert(OptionsFollowConventions<facet::LevelMeterOptions>());
static_assert(OptionsFollowConventions<facet::FrameTimeGraphOptions>());
static_assert(OptionsFollowConventions<facet::LegendOptions>());
static_assert(OptionsFollowConventions<facet::ChartRangeBrushOptions>());

static_assert(OptionsFollowConventions<facet::ImageViewOptions>());
static_assert(OptionsFollowConventions<facet::AvatarOptions>());
static_assert(OptionsFollowConventions<facet::AvatarGroupOptions>());
static_assert(OptionsFollowConventions<facet::ThumbnailStripOptions>());
static_assert(OptionsFollowConventions<facet::CarouselOptions>());
static_assert(OptionsFollowConventions<facet::ImageViewerOptions>());
static_assert(OptionsFollowConventions<facet::ImageCompareOptions>());
static_assert(OptionsFollowConventions<facet::TimelineScrubberOptions>());
static_assert(OptionsFollowConventions<facet::DropZoneOptions>());

static_assert(OptionsFollowConventions<facet::CurveEditorOptions>());
static_assert(OptionsFollowConventions<facet::BezierEditorOptions>());
static_assert(OptionsFollowConventions<facet::GradientEditorOptions>());
static_assert(OptionsFollowConventions<facet::PropertyGridOptions>());
static_assert(OptionsFollowConventions<facet::PropertyCategoryOptions>());
static_assert(OptionsFollowConventions<facet::PropertyRowOptions>());
static_assert(OptionsFollowConventions<facet::ConsoleViewOptions>());
static_assert(OptionsFollowConventions<facet::SearchFilterBarOptions>());
static_assert(OptionsFollowConventions<facet::SettingsSearchMatchOptions>());
static_assert(OptionsFollowConventions<facet::KeybindListOptions>());
static_assert(OptionsFollowConventions<facet::PresetListOptions>());
static_assert(OptionsFollowConventions<facet::InventoryGridOptions>());
static_assert(OptionsFollowConventions<facet::ItemTileOptions>());
static_assert(OptionsFollowConventions<facet::ItemSlotOptions>());
static_assert(OptionsFollowConventions<facet::ItemDetailCardOptions>());
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

// The stacks a call must leave as it found them.
struct Stacks
{
    int ids = 0;
    int windows = 0;
    int groups = 0;
    int scopes = 0;
    int usageErrors = 0;

    bool operator==(const Stacks&) const = default;
};

Stacks CurrentStacks()
{
    const ImGuiContext& g = *GImGui;
    return {g.CurrentWindow->IDStack.Size, g.CurrentWindowStack.Size, g.GroupStack.Size,
            facet::internal::OpenScopeCount(), facet::internal::UsageErrorCount()};
}

// How a call is checked besides the stacks.
enum class Expect
{
    Item,     // submits one item of non-zero size; with a label, the item id is that of the label
    NoItem,   // a query or a Begin that returns false: submits nothing that is checked here
};

// Calls `call` and checks the stacks, and for Expect::Item the last item.
template <class Call>
void Check(const char* name, Expect expect, const char* label, Call&& call)
{
    INFO("call: " << name);
    ImGui::Dummy(ImVec2(0.0f, 0.0f));  // the last item now has a zero size
    const Stacks before = CurrentStacks();
    call();
    CHECK(CurrentStacks() == before);
    if (expect == Expect::Item)
    {
        CHECK(ImGui::GetItemRectSize().x > 0.0f);
        CHECK(ImGui::GetItemRectSize().y > 0.0f);
        if (label != nullptr)
            CHECK(ImGui::GetItemID() == ImGui::GetID(label));
    }
}

// Runs `calls` once in the test window and once inside ImGui::BeginDisabled, in one frame.
template <class Calls>
void RunFrame(facet::test::Session& session, int* runs, Calls&& calls)
{
    session.SetUi([&] {
        calls();
        ImGui::BeginDisabled();
        calls();
        ImGui::EndDisabled();
        ++*runs;
    });
    session.Frame();
}

void DisplayCalls()
{
    using namespace facet;
    Check("StyledText", Expect::Item, "Body text", [] { StyledText("Body text", {.tone = Tone::Accent}); });
    Check("TruncatedText", Expect::Item, "A long line", [] { TruncatedText("A long line", {.mode = TruncateMode::Middle}); });
    Check("SectionHeader", Expect::Item, "Section", [] {
        CHECK_FALSE(SectionHeader("Section", {.subtitle = "Subtitle", .actionLabel = "Action"}));
    });

    bool open = true;
    Check("Banner", Expect::Item, "Title##banner", [&] {
        CHECK(Banner("Title##banner", &open, "Message", {.primaryAction = "Action"}) == BannerAction::None);
    });
    CHECK(open);
    Check("Banner without close button", Expect::Item, "##banner2",
          [] { CHECK(Banner("##banner2", nullptr, "Message") == BannerAction::None); });
    Check("InlineAlert", Expect::Item, "Title##alert", [&] {
        CHECK_FALSE(InlineAlert("Title##alert", &open, "Message", {.tone = Tone::Warning, .actionLabel = "Action"}));
    });
    CHECK(open);
    Check("EmptyState", Expect::Item, "Nothing here", [] {
        CHECK_FALSE(EmptyState("Nothing here", {.message = "Message", .actionLabel = "Action"}));
    });

    // A data state view with content opens a group and a scope for it.
    Check("BeginDataStateView, Ready", Expect::NoItem, nullptr, [] {
        const bool content = BeginDataStateView("data", DataState::Ready, 3);
        CHECK(content);
        if (content)
        {
            CHECK(internal::OpenScopeCount() > 0);
            ImGui::TextUnformatted("content");
            EndDataStateView();
        }
    });
    for (const DataState state : {DataState::Loading, DataState::Error})
        Check("BeginDataStateView, Loading or Error", Expect::Item, "data",
              [state] { CHECK_FALSE(BeginDataStateView("data", state, 3, {.retryLabel = "Retry"})); });
    Check("BeginDataStateView, empty", Expect::Item, "data", [] { CHECK_FALSE(BeginDataStateView("data", DataState::Ready, 0)); });
    Check("DataStateViewRetryPressed", Expect::NoItem, nullptr, [] { CHECK_FALSE(DataStateViewRetryPressed()); });

    Check("ProgressBar", Expect::Item, "Progress", [] { ProgressBar("Progress", 0.4f, {.showPercent = true}); });
    Check("ProgressBar, indeterminate", Expect::Item, "Progress##2", [] { ProgressBar("Progress##2", kIndeterminate); });
    Check("ProgressRing", Expect::Item, "Ring", [] { ProgressRing("Ring", 0.4f, {.showPercent = true}); });
    Check("Spinner", Expect::Item, "Loading", [] { Spinner("Loading"); });
    Check("Skeleton, text", Expect::Item, nullptr, [] { Skeleton({.lines = 3}); });
    Check("Skeleton, circle", Expect::Item, nullptr, [] { Skeleton({.shape = SkeletonShape::Circle}); });
    Check("Badge", Expect::Item, "Label", [] { Badge("Label", {.tone = Tone::Success}); });
    Check("CountBadge", Expect::Item, nullptr, [] { CountBadge(120); });
    Check("CountBadge, zero", Expect::NoItem, nullptr, [] { CountBadge(0); });
    Check("StatusDot", Expect::Item, "Status", [] { StatusDot("Status", {.tone = Tone::Success}); });
    Check("StatusPill", Expect::Item, "Status##pill", [] { StatusPill("Status##pill", {.liveDot = true}); });
    Check("KeyCap", Expect::Item, nullptr, [] { KeyCap(ImGuiMod_Ctrl | ImGuiKey_S); });
    Check("ShortcutHint", Expect::Item, "Save", [] { ShortcutHint("Save", ImGuiMod_Ctrl | ImGuiKey_S); });
    Check("ShortcutHint, registered", Expect::Item, "Open", [] { ShortcutHint("Open", ShortcutId{}); });
    Check("CharacterCounter", Expect::Item, nullptr, [] { CharacterCounter(12, 40); });
    Check("CharacterCounter, ring", Expect::Item, nullptr, [] { CharacterCounter(12, 40, {.ring = true}); });
    Check("InlineCode", Expect::Item, "value", [] { CHECK_FALSE(InlineCode("value", {.copyOnActivate = true})); });
    Check("CodeView", Expect::Item, "code", [] { CHECK_FALSE(CodeView("code", "int x = 0;", {.language = CodeLanguage::Cpp})); });

    static const LogEntry entries[] = {{"Line 1"}, {"Line 2", LogLevel::Warning}};
    Check("LogView", Expect::Item, "log", [] { LogView("log", entries, {.height = 120.0f}); });
    Check("LogView, getter", Expect::Item, "log2", [] {
        LogView("log2", 2, [](void*, int index, LogEntry* out) {
            *out = entries[index];
            return true;
        }, nullptr, {.height = 120.0f});
    });

    Check("KeyValueList", Expect::NoItem, nullptr, [] {
        BeginKeyValueList("details", {.copyable = true});
        CHECK_FALSE(KeyValue("Key", "Value"));
        EndKeyValueList();
    });
    Check("KeyValue", Expect::Item, "Key 2", [] { CHECK_FALSE(KeyValue("Key 2", "Value")); });
    // The scope form; the list's group is the last item afterwards.
    Check("KeyValueListScope", Expect::Item, nullptr, [] {
        KeyValueListScope list("details2");
        CHECK_FALSE(KeyValue("Key 3", "Value"));
    });
    static const float trend[] = {1.0f, 2.0f, 3.0f};
    Check("StatTile", Expect::Item, "Tile", [] { StatTile("Tile", 42.0, {.delta = 1.5, .trend = trend}); });

    static const DataTableColumn columns[] = {{"Name"}, {"Size", 80.0f, ColumnAlign::End}};
    ImGuiSelectionBasicStorage selection;
    selection.SetItemSelected(1, true);
    // Ready with rows: a real Dear ImGui table is open between Begin and End, so the caller's
    // rows and cells work; it is closed again by End.
    Check("BeginDataTable, Ready", Expect::NoItem, nullptr, [&] {
        ImGuiTable* const outer = ImGui::GetCurrentTable();
        const bool open = BeginDataTable("table", columns, 10, &selection, {.sortable = true, .frozenColumns = 1});
        CHECK(open);
        if (open)
        {
            CHECK(ImGui::GetCurrentTable() != outer);
            CHECK(ImGui::TableGetColumnCount() == 2);
            CHECK_FALSE(DataTableRow(0));
            CHECK(ImGui::TableNextColumn());
            ImGui::TextUnformatted("cell");
            CHECK_FALSE(BeginDataTableCellEdit());
            CHECK(ImGui::TableNextColumn());
            EndDataTable();
        }
        CHECK(ImGui::GetCurrentTable() == outer);
    });
    for (const DataState state : {DataState::Loading, DataState::Error})
        Check("BeginDataTable, Loading or Error", Expect::Item, "table",
              [&] { CHECK_FALSE(BeginDataTable("table", columns, 10, &selection, {.state = state, .height = 120.0f})); });
    Check("BeginDataTable, empty", Expect::Item, "table",
          [&] { CHECK_FALSE(BeginDataTable("table", columns, 0, &selection, {.height = 120.0f})); });
    CHECK(selection.Size == 1);
    CHECK(selection.Contains(1));
    // Outside a table the row call does nothing and the cell edit never opens.
    Check("DataTableRow outside a table", Expect::NoItem, nullptr, [] { CHECK_FALSE(DataTableRow(0)); });
    Check("BeginDataTableCellEdit", Expect::NoItem, nullptr, [] { CHECK_FALSE(BeginDataTableCellEdit()); });
}

void ChartCalls()
{
    using namespace facet;
    static const float values[] = {1.0f, 3.0f, 2.0f};
    static const ChartSeries series[] = {{.label = "Series 1", .values = values}};
    static const char* const categories[] = {"Category 1", "Category 2", "Category 3"};
    static const DonutSlice slices[] = {{"Slice 1", 2.0}, {"Slice 2", 1.0}};
    static const GaugeZone zones[] = {{0.8, 1.0, Tone::Destructive}};
    const ChartGetter getter = [](void*, int index, ChartPoint* out) {
        out->x = index;
        out->y = values[index];
        return true;
    };

    Check("Sparkline", Expect::Item, "Trend", [] { Sparkline("Trend", values, {.bars = true}); });
    Check("Sparkline, getter", Expect::Item, "Trend 2", [&] { Sparkline("Trend 2", 3, getter, nullptr); });
    Check("LineChart", Expect::Item, "line", [] { LineChart("line", series, {.yAxis = {.min = 0.0, .max = 4.0}}); });
    Check("BarChart", Expect::Item, "bars", [] { BarChart("bars", series, categories, {.stacked = true}); });
    Check("DonutChart", Expect::Item, "donut", [] { DonutChart("donut", slices, {.pie = true}); });
    Check("Gauge", Expect::Item, "Gauge", [] { Gauge("Gauge", 0.5, 0.0, 1.0, {.zones = zones}); });
    Check("LevelMeter", Expect::Item, "Meter", [] { LevelMeter("Meter", values, 0.0f, 4.0f); });
    Check("LevelMeter, vertical", Expect::Item, "Meter 2",
          [] { LevelMeter("Meter 2", values, 0.0f, 4.0f, {.orientation = Orientation::Vertical}); });
    Check("FrameTimeGraph", Expect::Item, "frames", [] { FrameTimeGraph("frames"); });
    Check("FrameTimeGraph, samples", Expect::Item, "frames 2", [] { FrameTimeGraph("frames 2", values, {.compact = true}); });

    LegendItem legend[] = {{"Series 1"}, {"Series 2", 0, true}};
    Check("Legend", Expect::Item, "legend", [&] { CHECK_FALSE(Legend("legend", legend)); });
    CHECK_FALSE(legend[0].hidden);
    CHECK(legend[1].hidden);

    double low = 1.0;
    double high = 2.0;
    Check("ChartRangeBrush", Expect::Item, "Range", [&] {
        CHECK_FALSE(ChartRangeBrush("Range", &low, &high, 0.0, 3.0, series, {.minGap = 0.5}));
    });
    CHECK(low == 1.0);
    CHECK(high == 2.0);
}

void MediaCalls()
{
    using namespace facet;
    const ImageHandle none;
    static const AvatarItem avatars[] = {{"Name 1"}, {"Name 2"}};
    static const ImageItem images[] = {{"Image 1"}, {"Image 2"}};
    static const char* const types[] = {"test.payload"};

    Check("ImageView", Expect::Item, "Image", [&] { CHECK_FALSE(ImageView("Image", none, {.activatable = true})); });
    Check("ImageView, sized", Expect::Item, "Image 2",
          [&] { CHECK_FALSE(ImageView("Image 2", none, {.width = 64.0f, .height = 32.0f})); });
    Check("ImageView, texture", Expect::Item, "Texture", [] {
        CHECK_FALSE(ImageView("Texture", static_cast<ID3D11ShaderResourceView*>(nullptr)));
    });
    Check("ImageView, retry", Expect::Item, "Image 3",
          [&] { CHECK_FALSE(ImageView("Image 3", none, {.activatable = true, .retryLabel = "Retry"})); });
    Check("ImageViewRetryPressed", Expect::NoItem, nullptr, [] { CHECK_FALSE(ImageViewRetryPressed()); });
    Check("Avatar", Expect::Item, "Name", [&] { CHECK_FALSE(Avatar("Name", none, {.activatable = true})); });
    Check("AvatarGroup", Expect::Item, "Group", [] { CHECK_FALSE(AvatarGroup("Group", avatars, {.maxVisible = 1})); });

    int selected = 1;
    Check("ThumbnailStrip", Expect::Item, "Strip", [&] { CHECK_FALSE(ThumbnailStrip("Strip", &selected, images)); });
    Check("ThumbnailStrip, getter", Expect::Item, "Strip 2", [&] {
        CHECK_FALSE(ThumbnailStrip("Strip 2", &selected, 2, [](void*, int index, ImageItem* out) {
            *out = images[index];
            return true;
        }, nullptr));
    });
    Check("Carousel", Expect::Item, "Slides", [&] { CHECK_FALSE(Carousel("Slides", &selected, images, {.loop = true})); });
    CHECK(selected == 1);

    ImageViewerState view{.zoom = 2.0f, .center = ImVec2(3.0f, 4.0f)};
    Check("ImageViewer", Expect::Item, "Viewer", [&] { CHECK_FALSE(ImageViewer("Viewer", none, &view, {.height = 100.0f})); });
    CHECK(view.zoom == 2.0f);
    CHECK(view.center.x == 3.0f);
    CHECK(view.center.y == 4.0f);

    float split = 0.25f;
    Check("ImageCompare", Expect::Item, "Compare", [&] { CHECK_FALSE(ImageCompare("Compare", &split, none, none)); });
    CHECK(split == 0.25f);

    double playhead = 1.0;
    double loopIn = 0.5;
    double loopOut = 2.0;
    Check("TimelineScrubber", Expect::Item, "Time", [&] { CHECK_FALSE(TimelineScrubber("Time", &playhead, 5.0)); });
    Check("TimelineScrubber, loop", Expect::Item, "Time 2",
          [&] { CHECK_FALSE(TimelineScrubber("Time 2", &playhead, &loopIn, &loopOut, 5.0)); });
    CHECK(playhead == 1.0);
    CHECK(loopIn == 0.5);
    CHECK(loopOut == 2.0);

    Check("DropZone", Expect::Item, "Drop", [] { CHECK(DropZone("Drop", types) == DropZoneResult::None); });
    Check("DropZonePayload", Expect::NoItem, nullptr, [] { CHECK(DropZonePayload() == nullptr); });
}

bool SameVec2(ImVec2 a, ImVec2 b)
{
    return a.x == b.x && a.y == b.y;
}

void EditorCalls()
{
    using namespace facet;
    ImVec2 points[4] = {{0.0f, 0.0f}, {0.5f, 0.25f}, {1.0f, 1.0f}};
    int count = 3;
    Check("CurveEditor", Expect::Item, "Curve", [&] {
        CHECK_FALSE(CurveEditor("Curve", points, &count, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), {.marker = 0.5f}));
    });
    CHECK(count == 3);
    CHECK(SameVec2(points[1], ImVec2(0.5f, 0.25f)));

    CubicBezier curve{{0.25f, 0.1f}, {0.25f, 1.0f}};
    Check("BezierEditor", Expect::Item, "Easing", [&] { CHECK_FALSE(BezierEditor("Easing", &curve)); });
    CHECK(SameVec2(curve.p1, ImVec2(0.25f, 0.1f)));
    CHECK(SameVec2(curve.p2, ImVec2(0.25f, 1.0f)));

    GradientStop stops[3] = {{0.0f}, {1.0f, {1.0f, 1.0f, 1.0f, 1.0f}}};
    int stopCount = 2;
    Check("GradientEditor", Expect::Item, "Gradient", [&] {
        CHECK_FALSE(GradientEditor("Gradient", stops, &stopCount, {.alpha = true}));
    });
    CHECK(stopCount == 2);
    CHECK(stops[1].position == 1.0f);

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
    Check("SearchFilterBar", Expect::Item, "Search", [&] {
        CHECK_FALSE(SearchFilterBar("Search", &command, filters, {.resultCount = 3}));
    });
    Check("SearchFilterBar, buffer", Expect::Item, "Search 2",
          [&] { CHECK_FALSE(SearchFilterBar("Search 2", buffer, sizeof(buffer), filters)); });
    CHECK(command == "text");
    CHECK(std::strcmp(buffer, "text") == 0);
    CHECK_FALSE(filters[0].active);
    CHECK(filters[1].active);

    Check("SettingsSearch", Expect::NoItem, nullptr, [] {
        BeginSettingsSearch("settings", "query");
        CHECK(SettingsSearchMatch("Row", {.keywords = "word", .section = "Section"}));
        EndSettingsSearch();
    });
    Check("SettingsSearchScope", Expect::NoItem, nullptr, [] { SettingsSearchScope search("settings 2", ""); });
    Check("SettingsSearchHitCount", Expect::NoItem, nullptr,
          [] { CHECK(SettingsSearchHitCount("settings", nullptr) == 0); });

    Check("KeybindList, registry", Expect::Item, "binds", [] { CHECK_FALSE(KeybindList("binds", {.filter = "x"})); });
    KeybindListItem binds[] = {{"Action", "Group", {ImGuiKey_A}, {ImGuiKey_B}}};
    Check("KeybindList, items", Expect::Item, "binds 2", [&] { CHECK_FALSE(KeybindList("binds 2", binds)); });
    CHECK(binds[0].bind.primary == ImGuiKey_A);
    CHECK(binds[0].defaultBind.primary == ImGuiKey_B);

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
    Check("AccentPicker", Expect::Item, "Accent", [&] { CHECK_FALSE(AccentPicker("Accent", &accent)); });
    CHECK(accent.x == 0.1f);
    CHECK(accent.z == 0.3f);
    float scale = 1.25f;
    Check("UiScaleControl", Expect::Item, "Scale",
          [&] { CHECK_FALSE(UiScaleControl("Scale", &scale, 0.75f, 2.0f, {.keyboardZoom = true})); });
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
    Check("ItemTile", Expect::Item, "tile", [] { CHECK_FALSE(ItemTile("tile", slots[0], {.selected = true})); });
    Check("ItemSlot", Expect::Item, "Slot", [&] { CHECK(neutral(ItemSlot("Slot", slots[1]))); });

    static const ItemStat stats[] = {{"Stat", 2.0, 1.0}};
    static const char* const actions[] = {"Action"};
    Check("ItemDetailCard", Expect::Item, "Item", [] {
        CHECK(ItemDetailCard("Item", ImageHandle{}, {.stats = stats, .actions = actions}) == -1);
    });
    Check("MediaCard", Expect::Item, "Card", [] { CHECK_FALSE(MediaCard("Card", ImageHandle{}, {.selected = true})); });
    Check("UserChip", Expect::Item, "Name", [] { CHECK_FALSE(UserChip("Name", ImageHandle{}, {.detail = "Detail"})); });
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
    Check("PreviewCanvas", Expect::NoItem, nullptr, [&] {
        BeginPreviewCanvas("canvas", ImVec2(1920.0f, 1080.0f), {.gridSpacing = 120.0f});
        CHECK_FALSE(CanvasElement("Element", &element, {.resizable = true}));
        EndPreviewCanvas();
    });
    Check("PreviewCanvasScope, empty size", Expect::NoItem, nullptr,
          [] { PreviewCanvasScope canvas("canvas 2", ImVec2(0.0f, 0.0f)); });
    Check("CanvasElement", Expect::Item, "Element 2", [&] { CHECK_FALSE(CanvasElement("Element 2", &element)); });
    CHECK(element.anchor == CanvasAnchor::Center);
    CHECK(SameVec2(element.offset, ImVec2(10.0f, 20.0f)));
    CHECK(SameVec2(element.extent, ImVec2(100.0f, 50.0f)));

    LayerItem layers[] = {{"Layer 1"}, {"Layer 2", {}, 1, false, true}};
    int selected = 1;
    Check("LayerList", Expect::Item, "layers", [&] {
        const LayerListResult result = LayerList("layers", &selected, layers);
        CHECK(result.action == LayerAction::None);
        CHECK(result.index == -1);
        CHECK(result.to == -1);
    });
    CHECK(selected == 1);
    CHECK(layers[0].visible);
    CHECK_FALSE(layers[1].visible);
    CHECK(layers[1].locked);

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

TEST_CASE("The display stubs submit one item each, return the neutral result and keep every stack")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int runs = 0;
    RunFrame(session, &runs, &DisplayCalls);
    CHECK(runs == 1);
    CHECK(facet::internal::OpenScopeCount() == 0);
}

TEST_CASE("The chart stubs submit one item each, return the neutral result and keep every stack")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int runs = 0;
    RunFrame(session, &runs, &ChartCalls);
    CHECK(runs == 1);
    CHECK(facet::internal::OpenScopeCount() == 0);
}

TEST_CASE("The media stubs submit one item each, return the neutral result and keep every stack")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int runs = 0;
    RunFrame(session, &runs, &MediaCalls);
    CHECK(runs == 1);
    CHECK(facet::internal::OpenScopeCount() == 0);
}

TEST_CASE("The composite stubs submit one item each, return the neutral result and keep every stack")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int runs = 0;
    RunFrame(session, &runs, &CompositeCalls);
    CHECK(runs == 1);
    CHECK(facet::internal::OpenScopeCount() == 0);
}

TEST_CASE("Stubs without a label submit an item whose id is not the window id")
{
    // An empty label hashes to the id of the window, which Dear ImGui asserts on for an item at
    // the root of a window. Each call runs first in a visible window, where ItemAdd checks it.
    facet::test::Session session;
    REQUIRE(session.Ready());
    int checked = 0;
    const auto first = [&](const char* name, auto&& call) {
        INFO("call: " << name);
        session.SetUi([&] {
            call();
            CHECK(ImGui::GetItemID() != 0);
            CHECK(ImGui::GetItemID() != ImGui::GetCurrentWindow()->ID);
            CHECK(ImGui::IsItemVisible());
            ++checked;
        });
        session.Frame();
    };
    first("Skeleton", [] { facet::Skeleton(); });
    first("Skeleton, circle", [] { facet::Skeleton({.shape = facet::SkeletonShape::Circle}); });
    first("CharacterCounter, ring", [] { facet::CharacterCounter(1, 0, {.ring = true}); });
    first("Badge, empty text", [] { facet::Badge(""); });
    first("StyledText, null text", [] { facet::StyledText(nullptr); });
    CHECK(checked == 5);
}

TEST_CASE("Content-sized stubs are wide enough to name the widget and say planned")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int checked = 0;
    session.SetUi([&] {
        using namespace facet;
        const auto wide = [&](const char* widget, auto&& call) {
            INFO("widget: " << widget);
            call();
            char text[64];
            ImFormatString(text, sizeof(text), "%s: planned", widget);
            CHECK(ImGui::GetItemRectSize().x >= MeasureText(TextStyle::Footnote, text).x);
            ++checked;
        };
        static const AvatarItem avatars[] = {{"Name 1"}};
        const InventoryItem item{"Item"};
        ThemeMode mode = ThemeMode::Light;
        ImVec4 accent(0.1f, 0.2f, 0.3f, 1.0f);
        CanvasElementState element;
        wide("StyledText", [] { StyledText("A"); });
        wide("ProgressRing", [] { ProgressRing("##ring", 0.5f); });
        wide("Spinner", [] { Spinner("##spinner"); });
        wide("Badge", [] { Badge("A"); });
        wide("CountBadge", [] { CountBadge(1); });
        wide("StatusDot", [] { StatusDot("##dot"); });
        wide("StatusPill", [] { StatusPill("A"); });
        wide("KeyCap", [] { KeyCap(ImGuiKey_A); });
        wide("CharacterCounter", [] { CharacterCounter(1, 0); });
        wide("CharacterCounter", [] { CharacterCounter(1, 0, {.ring = true}); });
        wide("InlineCode", [] { InlineCode("a"); });
        wide("ImageView", [] { ImageView("A", ImageHandle{}); });
        wide("Avatar", [] { Avatar("A", ImageHandle{}); });
        wide("AvatarGroup", [] { AvatarGroup("A", avatars); });
        wide("ItemTile", [&] { ItemTile("tile", item); });
        wide("ItemSlot", [&] { ItemSlot("A", item); });
        wide("UserChip", [] { UserChip("A", ImageHandle{}, {.compact = true}); });
        wide("ThemeModeControl", [&] { ThemeModeControl("##theme", &mode); });
        wide("AccentPicker", [&] { AccentPicker("##accent", &accent, {.customColor = false}); });
        BeginPreviewCanvas("canvas", ImVec2(100.0f, 100.0f));
        wide("CanvasElement", [&] { CanvasElement("A", &element); });
        EndPreviewCanvas();
    });
    session.Frame();
    CHECK(checked == 20);
}

TEST_CASE("An End without its Begin is reported and changes no Dear ImGui stack")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int reported = 0;
    session.SetUi([&] {
        const Stacks before = CurrentStacks();
        const int errors = facet::internal::UsageErrorCount();
        facet::EndDataTable();
        facet::EndDataStateView();
        facet::EndKeyValueList();
        facet::EndPropertyRow();
        facet::EndPropertyGrid();
        facet::EndDragSource();
        facet::EndDropTarget();
        reported = facet::internal::UsageErrorCount() - errors;
        Stacks after = CurrentStacks();
        after.usageErrors = before.usageErrors;
        CHECK(after == before);
    });
    session.Frame();
    CHECK(reported == 7);
}
