// The placeholder of declared but not yet implemented widgets, and the options check helper
// the API tests use.

#include <span>
#include <string>

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

TEST_CASE("Widgets of every group take kFill, negative widths and readOnly the same way, placeholders or not")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    facet::SetTheme(facet::Theme::Light(), false);
    float available = 0.0f;
    ImVec4 value(0.25f, 0.5f, 0.5f, 1.0f);
    ImVec4 readOnlyValue(0.75f, 0.5f, 0.5f, 1.0f);
    int index = 1;
    static const char* const kItems[] = {"One", "Two"};
    session.SetUi([&] {
        available = ImGui::GetContentRegionAvail().x;
        // One widget of each group: controls, structure, content.
        facet::ColorChannelSlider("Fill##controls", &value, {.width = facet::kFill});
        session.Track("controls.fill");
        facet::ListBox("Fill##structure", &index, std::span<const char* const>(kItems), {.width = facet::kFill});
        session.Track("structure.fill");
        facet::ProgressBar("Fill##content", 0.5f, {.width = facet::kFill});
        session.Track("content.fill");
        facet::ColorChannelSlider("Less##controls", &value, {.width = -40.0f});
        session.Track("controls.less");
        facet::ListBox("Less##structure", &index, std::span<const char* const>(kItems), {.width = -40.0f});
        session.Track("structure.less");
        facet::ProgressBar("Less##content", 0.5f, {.width = -40.0f});
        session.Track("content.less");
        facet::ColorChannelSlider("Read-only##controls", &readOnlyValue, {.readOnly = true});
        session.Track("controls.readOnly");
    });
    session.Frame();

    for (const char* group : {"controls", "structure", "content"})
    {
        CAPTURE(group);
        const std::string fill = std::string(group) + ".fill";
        const std::string less = std::string(group) + ".less";
        CHECK(session.ItemRect(fill).GetWidth() == doctest::Approx(available));
        CHECK(session.ItemRect(less).GetWidth() == doctest::Approx(available - facet::Px(40.0f)));
    }
    // A read-only widget never writes its value, whether it is a placeholder or implemented; that
    // a placeholder writes nothing at all is checked per family in tests/test_api_<family>.cpp.
    session.Click(session.ItemCenter("controls.readOnly"));
    CHECK(readOnlyValue.x == 0.75f);
    CHECK(value.x == 0.25f);
}

TEST_CASE("PlannedWidth follows the width precedence of the API conventions")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float available = 0.0f;
    float units = 0.0f, fill = 0.0f, less = 0.0f, next = 0.0f, pushed = 0.0f, fallback = 0.0f, fallbackFill = 0.0f;
    float layerIgnoresItemWidth = 0.0f;
    session.SetUi([&] {
        available = ImGui::GetContentRegionAvail().x;
        units = facet::internal::PlannedWidth(100.0f, 50.0f);
        fill = facet::internal::PlannedWidth(facet::kFill, 50.0f);
        less = facet::internal::PlannedWidth(-30.0f, 50.0f);
        ImGui::SetNextItemWidth(77.0f);
        next = facet::internal::PlannedWidth(0.0f, 50.0f);
        ImGui::PushItemWidth(66.0f);
        pushed = facet::internal::PlannedWidth(0.0f, 50.0f);
        layerIgnoresItemWidth = facet::internal::PlannedWidth(0.0f, 50.0f, false);
        ImGui::PopItemWidth();
        fallback = facet::internal::PlannedWidth(0.0f, 50.0f);
        fallbackFill = facet::internal::PlannedWidth(0.0f, 0.0f);
    });
    session.Frame();
    CHECK(units == doctest::Approx(facet::Px(100.0f)));
    CHECK(fill == doctest::Approx(available));
    CHECK(less == doctest::Approx(available - facet::Px(30.0f)));
    CHECK(next == doctest::Approx(77.0f));
    CHECK(pushed == doctest::Approx(66.0f));
    CHECK(layerIgnoresItemWidth == doctest::Approx(50.0f));
    CHECK(fallback == doctest::Approx(50.0f));
    CHECK(fallbackFill == doctest::Approx(available));
    // Outside a frame nothing can be drawn: one pixel.
    CHECK(facet::internal::PlannedWidth(100.0f, 50.0f) == 1.0f);
}
