// The placeholder of declared but not yet implemented widgets, and the options check helper
// the API tests use.

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "internal/planned_widget.h"
#include "support/options_check.h"
#include "support/session.h"

namespace
{

struct GoodOptions
{
    int steps = 0;
    facet::ControlSize size = facet::ControlSize::Inherit;
    float width = 0.0f;
    bool readOnly = false;
    const char* error = nullptr;
    const char* helpText = nullptr;
};

struct WrongOrder
{
    float width = 0.0f;
    facet::ControlSize size = facet::ControlSize::Inherit;
};

struct WrongType
{
    int size = 0;
};

struct NotAggregate
{
    NotAggregate() {}
    float width = 0.0f;
};

static_assert(facet::test::OptionsFollowConventions<GoodOptions>());
static_assert(!facet::test::OptionsFollowConventions<WrongOrder>());
static_assert(!facet::test::OptionsFollowConventions<WrongType>());
static_assert(!facet::test::OptionsFollowConventions<NotAggregate>());

} // namespace

TEST_CASE("A planned widget is one item of control height that is not a Tab stop")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    facet::SetTheme(facet::Theme::Light(), false);
    ImGuiID expectedId = 0;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        expectedId = ImGui::GetID("Volume##value");
        facet::internal::PlannedWidget("Slider", "Volume##value");
        session.Track("planned");
        facet::internal::PlannedWidget("Toggle", "##hidden", 120.0f, 40.0f);
        session.Track("sized");
        ImGui::Button("after");
        session.Track("after");
    });
    session.Frame();

    const ImRect planned = session.ItemRect("planned");
    CHECK(session.ItemId("planned") == expectedId);
    CHECK(planned.GetHeight() == doctest::Approx(facet::Px(facet::T().metrics.controlHeight)));
    CHECK(planned.GetWidth() > 200.0f);
    const ImRect sized = session.ItemRect("sized");
    CHECK(sized.GetWidth() == 120.0f);
    CHECK(sized.GetHeight() == 40.0f);

    // Tab goes from the button before straight to the button after. After a click, the first
    // Tab only makes the keyboard cursor visible on the clicked item.
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("before"));
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("after"));
    // A click on the placeholder activates nothing.
    session.Click(planned.GetCenter());
    CHECK_FALSE(ImGui::IsAnyItemActive());
}
