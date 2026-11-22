// The declared API of the editor, property grid, canvas and layer composites
// (facet/widgets/composites.h) while they are placeholders: every function is called once inside a
// real Dear ImGui frame, in a window and again inside ImGui::BeginDisabled. After each call the id,
// window, group and Facet scope stacks must be as before, a widget must have submitted one item of
// non-zero size with the id of its label, the neutral result must come back and no value argument
// may have changed. Every options struct is checked against the conventions at compile time. The
// other composites have files of their own (tests/test_api_composites_cards.cpp and
// test_api_composites_settings.cpp); the behaviour tests of these widgets go into
// tests/test_composites_editors.cpp.

#include <cmath>
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
static_assert(HasEntryId<facet::LayerItem>);

// Views of caller data have the data block of rule 7 (HasDataBlock in support/content_stub_check.h).
static_assert(HasDataBlock<facet::LayerListOptions>());

// Every widget that writes a value, a selection or item members has readOnly (rule 7).
static_assert(HasReadOnly<facet::CurveEditorOptions>);
static_assert(HasReadOnly<facet::BezierEditorOptions>);
static_assert(HasReadOnly<facet::GradientEditorOptions>);
static_assert(HasReadOnly<facet::CanvasElementOptions>);
static_assert(HasReadOnly<facet::LayerListOptions>);

static_assert(OptionsFollowConventions<facet::CurveEditorOptions>());
static_assert(OptionsFollowConventions<facet::BezierEditorOptions>());
static_assert(OptionsFollowConventions<facet::GradientEditorOptions>());
static_assert(OptionsFollowConventions<facet::PropertyGridOptions>());
static_assert(OptionsFollowConventions<facet::PropertyCategoryOptions>());
static_assert(OptionsFollowConventions<facet::PropertyRowOptions>());
static_assert(OptionsFollowConventions<facet::PreviewCanvasOptions>());
static_assert(OptionsFollowConventions<facet::CanvasElementOptions>());
static_assert(OptionsFollowConventions<facet::LayerListOptions>());

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
}

void EditorAndCanvasCalls()
{
    EditorCalls();
    CanvasCalls();
}

} // namespace

TEST_CASE("The editor, property grid, canvas and layer stubs submit one item each and keep every stack")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int runs = 0;
    RunFrame(session, &runs, &EditorAndCanvasCalls);
    CHECK(runs == 1);
    CHECK(facet::internal::OpenScopeCount() == 0);
}

TEST_CASE("The canvas element stub is wide enough to name the widget and say planned")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int checked = 0;
    session.SetUi([&] {
        using namespace facet;
        CanvasElementState element;
        BeginPreviewCanvas("canvas", ImVec2(100.0f, 100.0f));
        CanvasElement("A", &element);
        CHECK(ImGui::GetItemRectSize().x >= MeasureText(TextStyle::Footnote, "CanvasElement: planned").x);
        ++checked;
        EndPreviewCanvas();
    });
    session.Frame();
    CHECK(checked == 1);
}

TEST_CASE("An editor or canvas End without its Begin is reported and changes no Dear ImGui stack")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int reported = 0;
    session.SetUi([&] {
        const Stacks before = CurrentStacks();
        const int errors = facet::internal::UsageErrorCount();
        facet::EndPropertyRow();
        facet::EndPropertyGrid();
        facet::EndPropertyCategory();
        facet::EndPreviewCanvas();
        reported = facet::internal::UsageErrorCount() - errors;
        Stacks after = CurrentStacks();
        after.usageErrors = before.usageErrors;
        CHECK(after == before);
    });
    session.Frame();
    CHECK(reported == 4);
}
