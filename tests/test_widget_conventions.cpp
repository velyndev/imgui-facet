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

TEST_CASE("Interactive widgets of every family show the hand cursor, disabled ones the not-allowed cursor")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    static const char* const kItems[] = {"One", "Two", "Three"};
    // One widget per family, each drawn alone so the pointer rests on it.
    int which = 0;
    bool disabled = false;
    bool toggle = false;
    bool check = false;
    float level = 0.5f;
    int choice = 0;
    int segment = 0;
    ImGuiMouseCursor cursor = ImGuiMouseCursor_Arrow;
    session.SetUi([&] {
        if (disabled)
            ImGui::BeginDisabled();
        switch (which)
        {
        case 0: (void)facet::Button("Button"); break;
        case 1: (void)facet::Toggle("##toggle", &toggle); break;
        case 2: (void)facet::Checkbox("##check", &check); break;
        case 3: (void)facet::Slider("##slider", &level, 0.0f, 1.0f, {.width = 200.0f}); break;
        case 4: (void)facet::Dropdown("##dropdown", &choice, kItems, {.width = 200.0f}); break;
        default: (void)facet::SegmentedControl("##segments", &segment, kItems); break;
        }
        session.Track("widget");
        if (disabled)
            ImGui::EndDisabled();
        cursor = ImGui::GetMouseCursor();
    });
    for (which = 0; which < 6; ++which)
    {
        CAPTURE(which);
        disabled = false;
        session.Frame();
        session.MouseMove(session.ItemCenter("widget"));
        session.Frame();
        CHECK(cursor == ImGuiMouseCursor_Hand);
        disabled = true;
        session.Frames(2);
        CHECK(cursor == ImGuiMouseCursor_NotAllowed);
    }
}