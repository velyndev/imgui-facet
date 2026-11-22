// Conventions every widget group shares: the options check helper the API tests use, and the
// width and readOnly rules of docs/api-conventions.md across the controls, structure and content
// groups.

#include <span>
#include <string>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

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

TEST_CASE("Widgets of every group take kFill, negative widths and readOnly the same way")
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
    // A read-only widget never writes its value; the read-only behaviour of each widget is
    // checked in its family's behaviour tests.
    session.Click(session.ItemCenter("controls.readOnly"));
    CHECK(readOnlyValue.x == 0.75f);
    CHECK(value.x == 0.25f);
}
