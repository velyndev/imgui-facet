// The declared API of the charts family (facet/widgets/charts.h): every options struct follows
// the conventions, and every function is called once without input inside a real Dear ImGui frame, in a window and again inside ImGui::BeginDisabled. After each
// call the id, window, group and Facet scope stacks must be as before, a widget must have
// submitted one item of non-zero size with the id of its label, the neutral result must come back
// and no value argument may have changed.

#include <cstddef>
#include <span>
#include <string>
#include <type_traits>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "internal/scope_stack.h"
#include "support/content_api_check.h"
#include "support/options_check.h"
#include "support/session.h"

namespace
{

using facet::test::OptionsFollowConventions;
using namespace facet::test::content;

static_assert(HasEntryId<facet::LegendItem>);
// Views of caller data have the data block of rule 7.
static_assert(HasDataBlock<facet::LineChartOptions>());
static_assert(HasDataBlock<facet::BarChartOptions>());
static_assert(HasDataBlock<facet::DonutChartOptions>());
static_assert(!HasDataBlock<facet::SparklineOptions>());  // the check itself works
static_assert(offsetof(facet::DonutChartOptions, diameter) < offsetof(facet::DonutChartOptions, state));
// Every widget that writes a value or item members has readOnly (rule 7).
static_assert(!HasReadOnly<facet::GaugeOptions>);  // the check itself works
static_assert(HasReadOnly<facet::LegendOptions>);
static_assert(HasReadOnly<facet::ChartRangeBrushOptions>);

// Chart x values are doubles like ChartPoint, ChartAxis and the range brush: a float near
// 1.7e9 seconds steps by 128 s.
static_assert(std::is_same_v<decltype(facet::ChartSeries::x), std::span<const double>>);
// Range bounds are double across the chart family (Gauge, LevelMeter, ChartRangeBrush).
static_assert(std::is_same_v<decltype(&facet::LevelMeter), void (*)(const char*, std::span<const float>, double,
                                                                    double, const facet::LevelMeterOptions&)>);

static_assert(OptionsFollowConventions<facet::SparklineOptions>());
static_assert(OptionsFollowConventions<facet::LineChartOptions>());
static_assert(OptionsFollowConventions<facet::BarChartOptions>());
static_assert(OptionsFollowConventions<facet::DonutChartOptions>());
static_assert(OptionsFollowConventions<facet::GaugeOptions>());
static_assert(OptionsFollowConventions<facet::LevelMeterOptions>());
static_assert(OptionsFollowConventions<facet::FrameTimeGraphOptions>());
static_assert(OptionsFollowConventions<facet::LegendOptions>());
static_assert(OptionsFollowConventions<facet::ChartRangeBrushOptions>());

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
    const Snapshot legendBefore(legend);
    Check("Legend", Expect::Item, "legend", [&] { CHECK_FALSE(Legend("legend", legend)); });
    CHECK(legendBefore.Unchanged());

    double low = 1.0;
    double high = 2.0;
    Check("ChartRangeBrush", Expect::Item, "Range", [&] {
        CHECK_FALSE(ChartRangeBrush("Range", &low, &high, 0.0, 3.0, series, {.minGap = 0.5}));
    });
    CHECK(low == 1.0);
    CHECK(high == 2.0);
}

} // namespace

TEST_CASE("The chart widgets submit one item each, return false without input and keep every stack")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int runs = 0;
    RunFrame(session, &runs, &ChartCalls);
    CHECK(runs == 1);
    CHECK(facet::internal::OpenScopeCount() == 0);
}

TEST_CASE("A vertical level meter follows kFill and widths relative to the available width")
{
    // Rule 6: kFill is the available width, any other negative width the available width minus
    // that many units.
    facet::test::Session session;
    REQUIRE(session.Ready());
    int checked = 0;
    session.SetUi([&] {
        using namespace facet;
        static const float channels[] = {0.5f};
        for (const float units : {kFill, -10.0f})
        {
            INFO("width " << units);
            const float expected = ImGui::GetContentRegionAvail().x + (units == kFill ? 0.0f : Px(units));
            LevelMeter("Meter", channels, 0.0, 1.0, {.orientation = Orientation::Vertical, .width = units});
            CHECK(ImGui::GetItemRectSize().x == doctest::Approx(expected));
            ++checked;
        }
    });
    session.Frame();
    CHECK(checked == 2);
}
