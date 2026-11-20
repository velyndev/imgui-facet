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

// --- Checkbox ---------------------------------------------------------------------------------

TEST_CASE("A checkbox flips on a click on box or label, Space and Enter")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool value = false;
    int writes = 0;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        if (facet::Checkbox("Show hidden files", &value))
            ++writes;
        session.Track("check");
    });
    session.Frame();
    const ImRect r = session.ItemRect("check");
    session.Click(ImVec2(r.Max.x - 4.0f, r.GetCenter().y));  // on the label
    CHECK(value);
    session.Click(ImVec2(r.Min.x + 4.0f, r.GetCenter().y));  // on the box
    CHECK_FALSE(value);
    TabFrom(session, 2);
    CHECK(ImGui::GetFocusID() == session.ItemId("check"));
    session.KeyPress(ImGuiKey_Space);
    CHECK(value);
    session.KeyPress(ImGuiKey_Enter);
    CHECK_FALSE(value);
    CHECK(writes == 4);
}

TEST_CASE("A tri-state checkbox goes from Off and Mixed to On and from On to Off")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    facet::CheckState state = facet::CheckState::Mixed;
    session.SetUi([&] {
        facet::Checkbox("Select all", &state);
        session.Track("check");
    });
    session.Frame();
    session.Click(session.ItemCenter("check"));
    CHECK(state == facet::CheckState::On);
    session.Click(session.ItemCenter("check"));
    CHECK(state == facet::CheckState::Off);
    session.Click(session.ItemCenter("check"));
    CHECK(state == facet::CheckState::On);
}

TEST_CASE("Dragging from one checkbox across others paints the first box's new value")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool values[4] = {false, true, false, false};
    int writes = 0;
    session.SetUi([&] {
        for (int i = 0; i < 4; ++i)
        {
            ImGui::PushID(i);
            writes += facet::Checkbox("Item", &values[i]) ? 1 : 0;
            session.Track(i == 0 ? "c0" : i == 1 ? "c1" : i == 2 ? "c2" : "c3");
            ImGui::PopID();
        }
    });
    session.Frame();
    // Down on the first box, over the second and third, up on the third: all three on.
    session.Drag({session.ItemCenter("c0"), session.ItemCenter("c1"), session.ItemCenter("c2")}, 4);
    session.Frame();
    CHECK(values[0]);
    CHECK(values[1]);
    CHECK(values[2]);
    CHECK_FALSE(values[3]);
    CHECK(writes == 2);  // the first and the third changed; the second was on already
    // From an on box: paints off, and the release on another box does not flip the first back.
    session.Drag({session.ItemCenter("c2"), session.ItemCenter("c1"), session.ItemCenter("c2")}, 4);
    session.Frame();
    CHECK_FALSE(values[2]);
    CHECK_FALSE(values[1]);
    CHECK(values[0]);
}

TEST_CASE("A read-only, disabled or null checkbox never writes and a disabled one is no Tab stop")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool readOnly = false;
    bool disabled = false;
    int writes = 0;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        writes += facet::Checkbox("Read only", &readOnly, {.readOnly = true}) ? 1 : 0;
        session.Track("readOnly");
        ImGui::BeginDisabled();
        writes += facet::Checkbox("Disabled", &disabled) ? 1 : 0;
        session.Track("disabled");
        ImGui::EndDisabled();
        writes += facet::Checkbox("Null", static_cast<bool*>(nullptr)) ? 1 : 0;
        session.Track("null");
    });
    session.Frame();
    session.Click(session.ItemCenter("readOnly"));
    session.Click(session.ItemCenter("disabled"));
    session.Click(session.ItemCenter("null"));
    CHECK(writes == 0);
    CHECK_FALSE(readOnly);
    CHECK_FALSE(disabled);
    TabFrom(session, 2);
    CHECK(ImGui::GetFocusID() == session.ItemId("readOnly"));  // read-only stays focusable
    session.KeyPress(ImGuiKey_Space);
    CHECK_FALSE(readOnly);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("null"));  // the disabled box is passed over
}

TEST_CASE("Checked and mixed boxes fill with the accent and an unchecked one does not")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ThemeGuard guard;
    facet::SetTheme(facet::Theme::Light(), false);
    const ImU32 accent = facet::ToU32(facet::T().colors.accent);
    const ImU32 glyph = facet::ToU32(facet::T().colors.onAccent);
    int off = -1;
    int on = 0;
    int mixed = 0;
    int mixedGlyph = 0;
    session.SetUi([&] {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        bool no = false;
        bool yes = true;
        facet::CheckState partly = facet::CheckState::Mixed;
        int from = drawList->VtxBuffer.Size;
        facet::Checkbox("Off", &no);
        off = CountColor(from, accent);
        from = drawList->VtxBuffer.Size;
        facet::Checkbox("On", &yes);
        on = CountColor(from, accent);
        from = drawList->VtxBuffer.Size;
        facet::Checkbox("Mixed", &partly);
        mixed = CountColor(from, accent);
        mixedGlyph = CountColor(from, glyph);
    });
    session.Frames(3);
    CHECK(off == 0);
    CHECK(on > 0);
    CHECK(mixed > 0);
    CHECK(mixedGlyph > 0);
}

TEST_CASE("A checkbox narrower than its label cuts it and shows the label on hover")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool value = false;
    bool tip = false;
    session.SetUi([&] {
        ImGui::SetNextItemWidth(70.0f);
        facet::Checkbox("Remember the window position between sessions", &value);
        session.Track("check");
        tip = facet::internal::IsPopupLevelOpen(facet::internal::PopupTooltipLevel(ImGui::GetItemID()));
        facet::Checkbox("Large##large", &value, {.size = facet::ControlSize::Large});
        session.Track("large");
    });
    session.Frame();
    CHECK(session.ItemRect("check").GetWidth() == doctest::Approx(70.0f));
    CHECK(session.ItemRect("large").GetHeight() == doctest::Approx(facet::ControlHeight(facet::ControlSize::Large)));
    session.MouseMove(session.ItemCenter("check"));
    session.Seconds(facet::T().overlay.tooltipDelay + 0.2f);
    CHECK(tip);
}

TEST_CASE("A checkbox outside a frame returns false")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool value = false;
    facet::CheckState state = facet::CheckState::Off;
    CHECK_FALSE(facet::Checkbox("Check", &value));
    CHECK_FALSE(facet::Checkbox("Check", &state));
}

// --- RadioGroup -------------------------------------------------------------------------------

namespace
{

// Probes the parts of a group by the id they use: entry `index` of the widget `label`.
ImGuiID EntryIdOf(const char* label, int index)
{
    const ImGuiID widget = ImGui::GetID(label);
    return ImHashData(&index, sizeof(index), widget);
}

} // namespace

TEST_CASE("A radio group is one Tab stop, arrows move and write and wrap, a disabled option is skipped")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    static const facet::ChoiceItem kItems[] = {{"First"}, {"Second", {}, nullptr, nullptr, true}, {"Third"}, {"Fourth"}};
    int index = 2;
    int writes = 0;
    ImGuiID ids[4] = {};
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        for (int i = 0; i < 4; ++i)
            ids[i] = EntryIdOf("Choice", i);
        if (facet::RadioGroup("Choice", &index, kItems))
            ++writes;
        session.Track("group");
        ImGui::Button("after");
        session.Track("after");
    });
    session.Frame();
    TabFrom(session, 2);
    CHECK(ImGui::GetFocusID() == ids[2]);  // the selected option is the Tab stop
    session.KeyPress(ImGuiKey_DownArrow);
    CHECK(index == 3);
    CHECK(ImGui::GetFocusID() == ids[3]);
    session.KeyPress(ImGuiKey_DownArrow);  // wraps to the first
    CHECK(index == 0);
    session.KeyPress(ImGuiKey_RightArrow);  // the disabled second is skipped
    CHECK(index == 2);
    session.KeyPress(ImGuiKey_UpArrow);
    CHECK(index == 0);
    session.KeyPress(ImGuiKey_End);
    CHECK(index == 3);
    session.KeyPress(ImGuiKey_Home);
    CHECK(index == 0);
    CHECK(writes == 6);
    session.KeyPress(ImGuiKey_Tab);  // one Tab leaves the group
    CHECK(ImGui::GetFocusID() == session.ItemId("after"));
    session.KeyPress(ImGuiKey_Tab | ImGuiMod_Shift);  // and one comes back, on the selected option
    CHECK(ImGui::GetFocusID() == ids[0]);
}

TEST_CASE("A click selects a radio option, a disabled one is not selectable")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    static const facet::ChoiceItem kItems[] = {{"First"}, {"Second", {}, "Placeholder detail"}, {"Third", {}, nullptr, nullptr, true}};
    int index = -1;
    float top = 0.0f;
    session.SetUi([&] {
        top = ImGui::GetCursorScreenPos().y;
        facet::RadioGroup("##choice", &index, kItems);
        session.Track("group");
    });
    session.Frame();
    const ImRect r = session.ItemRect("group");
    const float row = facet::ControlHeight(facet::ControlSize::Regular);
    session.Click(ImVec2(r.Min.x + 6.0f, top + row * 1.5f));
    CHECK(index == 1);
    // The second option has a detail line, so the third starts below it.
    session.Click(ImVec2(r.Min.x + 6.0f, r.Max.y - 4.0f));
    CHECK(index == 1);
}

TEST_CASE("A read-only radio group moves the focus with the arrows but never the selection")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    static const char* const kItems[] = {"First", "Second", "Third"};
    int index = 0;
    ImGuiID second = 0;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        second = EntryIdOf("Choice", 1);
        facet::RadioGroup("Choice", &index, kItems, {.readOnly = true});
    });
    session.Frame();
    TabFrom(session, 2);
    session.KeyPress(ImGuiKey_DownArrow);
    CHECK(ImGui::GetFocusID() == second);
    session.KeyPress(ImGuiKey_Space);
    CHECK(index == 0);
}

TEST_CASE("A horizontal radio group wraps into rows that never leave its width")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    static const char* const kItems[] = {"First option", "Second option", "Third option", "Fourth option"};
    int index = 0;
    float wideHeight = 0.0f;
    float narrowHeight = 0.0f;
    float narrowWidth = 0.0f;
    float available = 0.0f;
    session.SetUi([&] {
        facet::RadioGroup("##wide", &index, kItems, {.orientation = facet::Orientation::Horizontal});
        wideHeight = ImGui::GetItemRectSize().y;
        ImGui::BeginChild("narrow", ImVec2(facet::Px(160.0f), facet::Px(300.0f)));
        available = ImGui::GetContentRegionAvail().x;
        facet::RadioGroup("##narrow", &index, kItems, {.orientation = facet::Orientation::Horizontal});
        narrowHeight = ImGui::GetItemRectSize().y;
        narrowWidth = ImGui::GetItemRectSize().x;
        ImGui::EndChild();
    });
    session.Frame();
    CHECK(wideHeight == doctest::Approx(facet::ControlHeight(facet::ControlSize::Regular)));
    CHECK(narrowHeight > wideHeight * 2.0f);
    CHECK(narrowWidth <= available + 0.5f);
}

TEST_CASE("A toggle outside a frame returns false")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool value = false;
    CHECK_FALSE(facet::Toggle("Toggle", &value));
    CHECK_FALSE(value);
}
