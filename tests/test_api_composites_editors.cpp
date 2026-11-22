// The declared API of the editor, property grid, canvas and layer composites
// (facet/widgets/composites.h): every options struct is checked against the conventions at compile
// time, and an End without its Begin is reported without touching a Dear ImGui stack. Every widget of
// this group is implemented; their behaviour tests are in tests/test_composites_editors.cpp. The
// other composites have files of their own (tests/test_api_composites_cards.cpp and
// test_api_composites_settings.cpp).

#include <cmath>

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

} // namespace

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

TEST_CASE("Editor, grid, canvas and layer calls keep every stack and need no placeholder")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int runs = 0;
    RunFrame(session, &runs, [] {
        using namespace facet;
        ImVec2 points[4] = {{0.0f, 0.0f}, {1.0f, 1.0f}};
        int count = 2;
        CurveEditor("Curve", points, &count, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f));
        CubicBezier curve;
        BezierEditor("Easing", &curve);
        GradientStop stops[2] = {{0.0f}, {1.0f}};
        int stopCount = 2;
        GradientEditor("Gradient", stops, &stopCount);
        {
            PropertyGridScope grid("grid");
            if (PropertyCategoryScope category("Category"); category)
                if (PropertyRowScope row("Property"); row)
                    ImGui::TextUnformatted("value");
        }
        CanvasElementState element{CanvasAnchor::Center, {10.0f, 20.0f}, {100.0f, 50.0f}};
        {
            PreviewCanvasScope canvas("canvas", ImVec2(1920.0f, 1080.0f), {.gridSpacing = 120.0f});
            CanvasElement("Element", &element, {.resizable = true});
        }
        {
            PreviewCanvasScope canvas("empty canvas", ImVec2(0.0f, 0.0f));
            CHECK_FALSE(CanvasElement("Element", &element));
        }
        LayerItem layers[] = {{"Layer 1"}, {"Layer 2", {}, 1, false, true}};
        int selected = 1;
        LayerList("layers", &selected, layers);
    });
    CHECK(runs == 1);
    CHECK(facet::internal::OpenScopeCount() == 0);
}
