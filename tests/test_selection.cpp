// Behaviour of the selection family (facet/widgets/selection.h).
//
// One test case per behaviour of an implemented widget; docs/writing-a-widget.md describes the
// pattern. The API checks, and the placeholder checks of the widgets that are still planned, are in
// tests/test_api_selection.cpp.

#include <cmath>
#include <string>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "internal/focus.h"
#include "internal/popup_host.h"
#include "support/session.h"

namespace
{

// Vertices of `color` the window draw list received since `from`.
int CountColor(int from, ImU32 color)
{
    const ImDrawList* drawList = ImGui::GetWindowDrawList();
    int count = 0;
    for (int i = from; i < drawList->VtxBuffer.Size; ++i)
        count += drawList->VtxBuffer[i].col == color ? 1 : 0;
    return count;
}

// Restores the theme a test changes.
struct ThemeGuard
{
    facet::Theme saved = facet::T();
    ~ThemeGuard() { facet::SetTheme(saved, false); }
};

// Focuses the tracked item `name` from the stock button tracked as "before" with Tab.
void TabFrom(facet::test::Session& session, int tabs)
{
    session.Click(session.ItemCenter("before"));
    for (int i = 0; i < tabs; ++i)
        session.KeyPress(ImGuiKey_Tab);
}

} // namespace

// --- Toggle -----------------------------------------------------------------------------------

TEST_CASE("A toggle flips on a click, Space and Enter and reports one write each")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool value = false;
    int writes = 0;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        if (facet::Toggle("Wi-Fi", &value))
            ++writes;
        session.Track("toggle");
    });
    session.Frame();
    session.Click(session.ItemCenter("toggle"));
    CHECK(value);
    CHECK(writes == 1);
    TabFrom(session, 2);
    CHECK(ImGui::GetFocusID() == session.ItemId("toggle"));
    session.KeyPress(ImGuiKey_Space);
    CHECK_FALSE(value);
    session.KeyPress(ImGuiKey_Enter);
    CHECK(value);
    CHECK(writes == 3);
}

TEST_CASE("Dragging the toggle knob past the middle sets the side it is let go on")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool value = false;
    int writes = 0;
    session.SetUi([&] {
        if (facet::Toggle("##drag", &value))
            ++writes;
        session.Track("toggle");
    });
    session.Frame();
    const ImRect r = session.ItemRect("toggle");
    // From the knob at the left end to the right end: on, with one write on release.
    session.Drag({ImVec2(r.Min.x + 4.0f, r.GetCenter().y), ImVec2(r.Max.x - 2.0f, r.GetCenter().y)}, 6);
    session.Frame();
    CHECK(value);
    CHECK(writes == 1);
    // Back, but let go before the middle: it stays on.
    session.Drag({ImVec2(r.Max.x - 4.0f, r.GetCenter().y), ImVec2(r.Max.x - r.GetWidth() * 0.3f, r.GetCenter().y)}, 6);
    session.Frame();
    CHECK(value);
    // Released outside the toggle past the middle: off.
    session.Drag({ImVec2(r.Max.x - 4.0f, r.GetCenter().y), ImVec2(r.Min.x - 40.0f, r.GetCenter().y)}, 6);
    session.Frame();
    CHECK_FALSE(value);
}

TEST_CASE("A read-only, disabled or loading toggle never writes, a null value is read-only")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool readOnly = true;
    bool disabled = true;
    bool loading = true;
    int writes = 0;
    session.SetUi([&] {
        writes += facet::Toggle("Read only", &readOnly, {.readOnly = true}) ? 1 : 0;
        session.Track("readOnly");
        ImGui::BeginDisabled();
        writes += facet::Toggle("Disabled", &disabled) ? 1 : 0;
        session.Track("disabled");
        ImGui::EndDisabled();
        writes += facet::Toggle("Loading", &loading, {.loading = true}) ? 1 : 0;
        session.Track("loading");
        writes += facet::Toggle("No value", nullptr) ? 1 : 0;
        session.Track("null");
    });
    session.Frame();
    for (const char* name : {"readOnly", "disabled", "loading", "null"})
        session.Click(session.ItemCenter(name));
    CHECK(readOnly);
    CHECK(disabled);
    CHECK(loading);
    CHECK(writes == 0);
}

TEST_CASE("A mixed toggle shows its knob in the middle until the first edit")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool value = false;
    bool written = false;
    session.SetUi([&] {
        written = facet::Toggle("Sync", &value, {.mixed = true});
        session.Track("toggle");
    });
    session.Frame();
    session.Click(session.ItemCenter("toggle"));
    // The first edit writes a definite value (on), even though the caller still passes mixed.
    CHECK(value);
    session.Click(session.ItemCenter("toggle"));
    CHECK_FALSE(value);
}

TEST_CASE("A toggle fits label and switch, puts the switch at the end when wider and cuts a long label")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool value = false;
    bool cutTip = false;
    float labelWidth = 0.0f;
    session.SetUi([&] {
        labelWidth = facet::MeasureText(facet::TextStyle::Body, "Wi-Fi").x;
        facet::Toggle("Wi-Fi", &value);
        session.Track("fit");
        facet::Toggle("Wi-Fi##wide", &value, {.width = 300.0f});
        session.Track("wide");
        facet::Toggle("A label far longer than the room it gets##cut", &value, {.width = 80.0f});
        session.Track("cut");
        cutTip = facet::internal::IsPopupLevelOpen(facet::internal::PopupTooltipLevel(ImGui::GetItemID()));
        facet::Toggle("Small##small", &value, {.size = facet::ControlSize::Small});
        session.Track("small");
    });
    session.Frame();
    const facet::Theme& t = facet::T();
    CHECK(session.ItemRect("fit").GetWidth() ==
          doctest::Approx(std::ceil(labelWidth + std::round(facet::Px(t.spacing.sm)) + facet::PxSnapped(t.metrics.toggleWidth))));
    CHECK(session.ItemRect("fit").GetHeight() == doctest::Approx(facet::ControlHeight(facet::ControlSize::Regular)));
    CHECK(session.ItemRect("wide").GetWidth() == doctest::Approx(facet::Px(300.0f)));
    CHECK(session.ItemRect("small").GetHeight() == doctest::Approx(facet::ControlHeight(facet::ControlSize::Small)));
    session.MouseMove(session.ItemCenter("cut"));
    session.Seconds(t.overlay.tooltipDelay + 0.2f);
    CHECK(cutTip);
}

TEST_CASE("A toggle with an error draws it under the switch and the group covers it")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool value = false;
    float plain = 0.0f;
    float withError = 0.0f;
    ImGuiID id = 0;
    ImGuiID last = 0;
    session.SetUi([&] {
        facet::Toggle("Plain", &value);
        plain = ImGui::GetItemRectSize().y;
        id = ImGui::GetID("Checked");
        facet::Toggle("Checked", &value, {.error = "Placeholder error"});
        withError = ImGui::GetItemRectSize().y;
        last = ImGui::GetItemID();
    });
    session.Frame();
    session.Frame();
    CHECK(withError > plain);
    CHECK(last == id);
}

TEST_CASE("The toggle focus ring shows after keyboard input only")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ThemeGuard guard;
    facet::SetTheme(facet::Theme::Light(), false);
    const ImU32 ring = facet::ToU32(facet::internal::FocusRingColor());
    bool value = false;
    int ringVertices = 0;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        const int from = ImGui::GetWindowDrawList()->VtxBuffer.Size;
        facet::Toggle("Bluetooth", &value);
        session.Track("toggle");
        ringVertices = CountColor(from, ring);
    });
    session.Frame();
    session.Click(session.ItemCenter("toggle"));
    session.Frames(10);
    // The click turned it on: the on track has a focusRing hairline of its own, so turn it off.
    session.Click(session.ItemCenter("toggle"));
    session.Frames(10);
    CHECK(ringVertices == 0);
    TabFrom(session, 2);
    session.Frames(10);
    CHECK(ringVertices > 0);
}

TEST_CASE("A toggle outside a frame returns false")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool value = false;
    CHECK_FALSE(facet::Toggle("Toggle", &value));
    CHECK_FALSE(value);
}
