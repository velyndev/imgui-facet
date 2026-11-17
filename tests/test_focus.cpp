// Focus ring and keyboard modality (src/internal/focus.h).

#include <span>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "internal/focus.h"
#include "internal/item.h"
#include "support/session.h"

using facet::internal::ItemBehavior;
using facet::internal::ItemState;
using facet::internal::KeyboardModality;

namespace
{

// Motion and theme as the presets have them, whatever the Windows animation setting; restored
// at the end of the case.
struct ThemeGuard
{
    ThemeGuard() { facet::SetFollowSystemMotion(false); }
    ~ThemeGuard()
    {
        facet::SetFollowSystemMotion(true);
        facet::ClearAccent();
        facet::SetTheme(facet::Theme::Light(), false);
        facet::SetUiScale(1.0f);
    }
};

} // namespace

TEST_CASE("Keyboard modality: navigation keys turn it on, the mouse turns it off")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    session.SetUi([&] { ImGui::Button("a"); session.Track("a"); ImGui::Button("b"); });
    session.Frames(2);
    CHECK_FALSE(KeyboardModality());  // a fresh start
    session.KeyPress(ImGuiKey_Tab);
    CHECK(KeyboardModality());
    // Typing text and modifiers alone keep it.
    session.Type("abc");
    session.KeyPress(ImGuiMod_Shift);
    CHECK(KeyboardModality());
    // Mouse movement keeps it, a click ends it.
    session.MouseMove(ImVec2(300.0f, 300.0f));
    CHECK(KeyboardModality());
    session.Click(session.ItemCenter("a"));
    CHECK_FALSE(KeyboardModality());
    session.KeyPress(ImGuiKey_DownArrow);
    CHECK(KeyboardModality());
    session.Wheel(ImVec2(300.0f, 300.0f), -1.0f);
    CHECK_FALSE(KeyboardModality());
    session.KeyPress(ImGuiKey_Space);
    CHECK(KeyboardModality());
}

TEST_CASE("Keyboard modality is reset by a new start")
{
    {
        facet::test::Session session;
        REQUIRE(session.Ready());
        session.SetUi([] { ImGui::Button("a"); });
        session.KeyPress(ImGuiKey_Tab);
        CHECK(KeyboardModality());
    }
    facet::test::Session session;
    REQUIRE(session.Ready());
    CHECK_FALSE(KeyboardModality());
}

TEST_CASE("Focus ring: the color reaches 3:1 on every surface for every accent in both themes")
{
    ThemeGuard guard;
    for (const bool dark : {false, true})
    {
        for (const facet::AccentPreset& preset : facet::AccentPresets())
        {
            facet::Theme theme = dark ? facet::Theme::Dark() : facet::Theme::Light();
            theme.ApplyAccent(preset.color);
            facet::SetTheme(theme, false);
            const ImVec4 ring = facet::internal::FocusRingColor();
            const facet::Colors& c = facet::T().colors;
            const ImVec4* surfaces[] = {&c.bg, &c.bgSecondary, &c.bgTertiary, &c.bgElevated, &c.bgElevatedLevels[0],
                                        &c.bgElevatedLevels[1], &c.bgElevatedLevels[2]};
            for (const ImVec4* surface : surfaces)
            {
                INFO((dark ? "dark " : "light ") << preset.name);
                CHECK(facet::ContrastRatio(ring, *surface) >= 3.0f);
            }
        }
    }
}

TEST_CASE("Focus ring: it sits just outside the control and fades with the amount")
{
    ThemeGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    facet::SetTheme(facet::Theme::Light(), false);
    facet::SetUiScale(1.0f);
    const ImRect control(100.0f, 100.0f, 200.0f, 128.0f);
    const ImRect ring = facet::internal::FocusRingRect(control);
    const float offset = facet::HairlineThickness() + facet::PxSnapped(facet::T().metrics.focusRingWidth);
    CHECK(ring.Min.x == control.Min.x - offset);
    CHECK(ring.Max.y == control.Max.y + offset);

    int verticesNone = -1;
    int verticesFull = -1;
    session.SetUi([&] {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const int start = drawList->VtxBuffer.Size;
        facet::internal::DrawFocusRing(drawList, control, 8.0f, 0.0f);
        verticesNone = drawList->VtxBuffer.Size - start;
        facet::internal::DrawFocusRing(drawList, control, 8.0f, 1.0f);
        verticesFull = drawList->VtxBuffer.Size - start;
    });
    session.Frame();
    CHECK(verticesNone == 0);
    CHECK(verticesFull > 0);
}

TEST_CASE("Focus ring: an item shows it only after keyboard input, animated, and clipped by its window")
{
    ThemeGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    facet::SetTheme(facet::Theme::Light(), false);
    ItemState state;
    ImVec4 childClip;
    ImVec4 ringClip;
    session.SetUi([&] {
        ImGui::Button("first");
        ImGui::BeginChild("child", ImVec2(200.0f, 100.0f));
        state = ItemBehavior(ImGui::GetID("item"), ImVec2(80.0f, 24.0f));
        session.Track("item");
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        childClip = drawList->_ClipRectStack.back();
        facet::internal::DrawItemFocusRing(state, 8.0f);
        ringClip = drawList->CmdBuffer.back().ClipRect;
        ImGui::EndChild();
    });
    session.Frames(2);
    // Mouse: focused, no ring.
    session.Click(session.ItemCenter("item"));
    CHECK(state.focused);
    CHECK_FALSE(state.focusVisible);
    CHECK(state.focusAmount == 0.0f);
    // Keyboard: the ring fades in over the fast duration.
    facet::internal::SetKeyboardModality(true);
    session.Frame();
    CHECK(state.focusVisible);
    CHECK(state.focusAmount > 0.0f);
    CHECK(state.focusAmount < 1.0f);
    session.Seconds(facet::T().motion.durationFast + 0.05f);
    CHECK(state.focusAmount == 1.0f);
    // Drawn with the child window's clip rectangle.
    CHECK(ringClip.x == childClip.x);
    CHECK(ringClip.z == childClip.z);
    CHECK(ringClip.w == childClip.w);

    // With reduced motion the ring is there at once.
    facet::Theme theme = facet::Theme::Light();
    theme.motion.reduceMotion = true;
    facet::SetTheme(theme, false);
    session.Click(ImVec2(500.0f, 500.0f));  // focus elsewhere, modality off
    session.Frame();
    session.Click(session.ItemCenter("item"));
    facet::internal::SetKeyboardModality(true);
    session.Frame();
    CHECK(state.focusAmount == 1.0f);
}
