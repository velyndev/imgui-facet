// The declared API of the content families (display, charts, media, composites) while their
// widgets are placeholders: every function is called once inside a real Dear ImGui frame, in a
// window and again inside ImGui::BeginDisabled. After each call the id, window, group and
// Facet scope stacks must be as before, a widget must have submitted one item of non-zero size
// with the id of its label, the neutral result must come back and no value argument may have
// changed. Every options struct is checked against the conventions at compile time.

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

static_assert(OptionsFollowConventions<facet::SparklineOptions>());
static_assert(OptionsFollowConventions<facet::LineChartOptions>());
static_assert(OptionsFollowConventions<facet::BarChartOptions>());
static_assert(OptionsFollowConventions<facet::DonutChartOptions>());
static_assert(OptionsFollowConventions<facet::GaugeOptions>());
static_assert(OptionsFollowConventions<facet::LevelMeterOptions>());
static_assert(OptionsFollowConventions<facet::FrameTimeGraphOptions>());
static_assert(OptionsFollowConventions<facet::LegendOptions>());
static_assert(OptionsFollowConventions<facet::ChartRangeBrushOptions>());

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
    INFO("call: ", name);
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
    Check("BeginDataTable", Expect::Item, "table", [&] {
        if (BeginDataTable("table", columns, 10, &selection, {.sortable = true, .frozenColumns = 1, .height = 120.0f}))
        {
            CHECK_FALSE(DataTableRow(0));
            if (BeginDataTableCellEdit())
                EndDataTableCellEdit();
            EndDataTable();
        }
    });
    CHECK(selection.Size == 1);
    CHECK(selection.Contains(1));
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
