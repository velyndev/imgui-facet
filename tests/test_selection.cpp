// Behaviour of the selection family (facet/widgets/selection.h).
//
// One test case per behaviour of an implemented widget; docs/writing-a-widget.md describes the
// pattern. The API checks, and the placeholder checks of the widgets that are still planned, are in
// tests/test_api_selection.cpp.

#include <cfloat>
#include <cmath>
#include <string>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "internal/control.h"
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

// --- SegmentedControl -------------------------------------------------------------------------

namespace
{

// The bounding box of the vertices of `color` the window draw list received since `from`: for a
// filled shape, its opaque inner part (anti-aliasing puts it half a pixel inside the edge).
ImRect ColorBounds(int from, ImU32 color)
{
    const ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImRect bounds(FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX);
    for (int i = from; i < drawList->VtxBuffer.Size; ++i)
        if (drawList->VtxBuffer[i].col == color)
            bounds.Add(drawList->VtxBuffer[i].pos);
    return bounds;
}

// A filled edge on a whole pixel: its opaque vertices lie on the pixel, or half a pixel inside it
// where anti-aliasing adds a fringe.
bool IsWhole(float pixels)
{
    const float twice = pixels * 2.0f;
    return std::fabs(twice - std::round(twice)) < 0.02f;
}

} // namespace

TEST_CASE("A segmented control is one Tab stop; Left and Right stop at the ends and skip disabled segments")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    static const facet::ChoiceItem kItems[] = {{"Day"}, {"Week", {}, nullptr, nullptr, true}, {"Month"}, {"Year"}};
    int index = 0;
    int writes = 0;
    ImGuiID ids[4] = {};
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        for (int i = 0; i < 4; ++i)
            ids[i] = EntryIdOf("Range", i);
        if (facet::SegmentedControl("Range", &index, kItems))
            ++writes;
        session.Track("segments");
        ImGui::Button("after");
        session.Track("after");
    });
    session.Frame();
    TabFrom(session, 2);
    CHECK(ImGui::GetFocusID() == ids[0]);
    session.KeyPress(ImGuiKey_LeftArrow);  // stops at the first
    CHECK(index == 0);
    session.KeyPress(ImGuiKey_RightArrow);  // skips the disabled Week
    CHECK(index == 2);
    CHECK(ImGui::GetFocusID() == ids[2]);
    session.KeyPress(ImGuiKey_RightArrow);
    session.KeyPress(ImGuiKey_RightArrow);  // stops at the last
    CHECK(index == 3);
    session.KeyPress(ImGuiKey_Home);
    CHECK(index == 0);
    session.KeyPress(ImGuiKey_End);
    CHECK(index == 3);
    CHECK(writes == 4);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("after"));
    session.KeyPress(ImGuiKey_Tab | ImGuiMod_Shift);
    CHECK(ImGui::GetFocusID() == ids[3]);
}

TEST_CASE("A press selects a segment and a drag across the segments moves the selection")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    static const char* const kItems[] = {"One", "Two", "Three", "Four"};
    int index = 0;
    session.SetUi([&] {
        facet::SegmentedControl("##segments", &index, kItems, {.width = 400.0f});
        session.Track("segments");
    });
    session.Frame();
    const ImRect r = session.ItemRect("segments");
    const float y = r.GetCenter().y;
    const float quarter = r.GetWidth() * 0.25f;
    session.Click(ImVec2(r.Min.x + quarter * 1.5f, y));
    CHECK(index == 1);
    session.Drag({ImVec2(r.Min.x + quarter * 0.5f, y), ImVec2(r.Min.x + quarter * 3.5f, y)}, 6);
    CHECK(index == 3);
}

TEST_CASE("Segments fit their content by default, share a set width equally and fill a short row")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    static const char* const kItems[] = {"A", "Longer label"};
    static const facet::ChoiceItem kIcons[] = {{"Grid", facet::Icon("grid")}, {"List", facet::Icon("list")}};
    int index = 0;
    float available = 0.0f;
    session.SetUi([&] {
        available = ImGui::GetContentRegionAvail().x;
        facet::SegmentedControl("##fit", &index, kItems);
        session.Track("fit");
        facet::SegmentedControl("##fill", &index, kItems, {.width = facet::kFill});
        session.Track("fill");
        facet::SegmentedControl("##icons", &index, kIcons, {.iconOnly = true});
        session.Track("icons");
        facet::SegmentedControl("##large", &index, kItems, {.size = facet::ControlSize::Large});
        session.Track("large");
    });
    session.Frame();
    const facet::internal::ControlMetrics m = facet::internal::GetControlMetrics(facet::ControlSize::Regular);
    CHECK(session.ItemRect("fit").GetWidth() < available);
    CHECK(session.ItemRect("fill").GetWidth() == doctest::Approx(available));
    CHECK(session.ItemRect("icons").GetWidth() == doctest::Approx(m.height * 2.0f));
    CHECK(session.ItemRect("large").GetHeight() == doctest::Approx(facet::ControlHeight(facet::ControlSize::Large)));
}

TEST_CASE("The selection pill springs to the new segment, rests on whole pixels and jumps with reduced motion")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ThemeGuard guard;
    facet::SetTheme(facet::Theme::Light(), false);
    static const char* const kItems[] = {"One", "Two", "Three", "Four"};
    int index = 0;
    float width = 401.0f;  // an odd width, so the segment edges need rounding
    ImRect pill;
    session.SetUi([&] {
        const int from = ImGui::GetWindowDrawList()->VtxBuffer.Size;
        facet::SegmentedControl("##segments", &index, kItems, {.width = width});
        pill = ColorBounds(from, facet::ToU32(facet::T().colors.accentSubtle));
        session.Track("segments");
    });
    session.Frame();
    const ImRect start = pill;
    REQUIRE(start.Min.x < start.Max.x);
    index = 3;
    session.Frame();
    session.Frame();
    // On its way: past the start, short of the end.
    CHECK(pill.Min.x > start.Min.x);
    session.Seconds(1.5f);
    const ImRect r = session.ItemRect("segments");
    CHECK(pill.Max.x > r.Max.x - r.GetWidth() * 0.25f - 1.0f);
    CHECK(IsWhole(pill.Min.x));
    CHECK(IsWhole(pill.Max.x));
    CHECK(IsWhole(pill.Min.y));
    // A width change: the pill follows its segment and comes to rest on whole pixels again.
    const ImRect before = pill;
    width = 300.0f;
    session.Frame();
    CHECK(pill.Max.x > session.ItemRect("segments").Max.x);  // still on its way, no jump
    session.Seconds(1.5f);
    CHECK(pill.Max.x < before.Max.x);
    CHECK(IsWhole(pill.Min.x));
    // Reduced motion: at the new segment in the next frame.
    facet::Theme reduced = facet::T();
    reduced.motion.reduceMotion = true;
    facet::SetTheme(reduced, false);
    const ImRect rested = pill;
    index = 0;
    session.Frame();
    session.Frame();
    CHECK(pill.Min.x < rested.Min.x - 100.0f);
    CHECK(IsWhole(pill.Min.x));
}

TEST_CASE("A read-only or mixed segmented control shows no write until an edit, read-only never writes")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    static const char* const kItems[] = {"One", "Two", "Three"};
    int fixed = 0;
    int mixed = 1;
    int writes = 0;
    session.SetUi([&] {
        writes += facet::SegmentedControl("##ro", &fixed, kItems, {.width = 300.0f, .readOnly = true}) ? 1 : 0;
        session.Track("ro");
        writes += facet::SegmentedControl("##mixed", &mixed, kItems, {.width = 300.0f, .mixed = true}) ? 1 : 0;
        session.Track("mixed");
    });
    session.Frame();
    const ImRect ro = session.ItemRect("ro");
    session.Click(ImVec2(ro.Max.x - 10.0f, ro.GetCenter().y));
    CHECK(fixed == 0);
    CHECK(writes == 0);
    // Choosing the segment that is already set still counts as the edit that ends the mixed look.
    const ImRect mr = session.ItemRect("mixed");
    session.Click(mr.GetCenter());
    CHECK(mixed == 1);
    CHECK(writes == 1);
}

// --- Chip -------------------------------------------------------------------------------------

TEST_CASE("A chip activates on its body; the remove button and Delete report a removal instead")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int presses = 0;
    int removes = 0;
    bool selected = false;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        if (facet::Chip("Placeholder tag", {.selected = selected, .removable = true}))
        {
            ++presses;
            selected = !selected;
        }
        session.Track("chip");
        if (facet::ChipRemovePressed())
            ++removes;
    });
    session.Frame();
    const ImRect r = session.ItemRect("chip");
    session.Click(ImVec2(r.Min.x + r.GetWidth() * 0.3f, r.GetCenter().y));
    CHECK(presses == 1);
    CHECK(selected);
    CHECK(removes == 0);
    // The remove button sits at the end of the pill.
    session.MouseMove(ImVec2(r.Max.x - r.GetHeight() * 0.5f, r.GetCenter().y));
    session.Click(ImVec2(r.Max.x - r.GetHeight() * 0.5f, r.GetCenter().y));
    CHECK(removes == 1);
    CHECK(presses == 1);
    TabFrom(session, 2);
    CHECK(ImGui::GetFocusID() == session.ItemId("chip"));
    session.KeyPress(ImGuiKey_Space);
    CHECK(presses == 2);
    session.KeyPress(ImGuiKey_Delete);
    session.KeyPress(ImGuiKey_Backspace);
    CHECK(removes == 3);
    CHECK_FALSE(facet::ChipRemovePressed());  // outside the frame of the press
}

TEST_CASE("A selected chip shows the selection tint, a disabled chip does nothing")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ThemeGuard guard;
    facet::SetTheme(facet::Theme::Light(), false);
    const ImU32 tint = facet::ToU32(facet::T().colors.accentSubtle);
    int selectedTint = 0;
    int plainTint = -1;
    int presses = 0;
    float plainWidth = 0.0f;
    float withExtras = 0.0f;
    session.SetUi([&] {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        int from = drawList->VtxBuffer.Size;
        facet::Chip("Selected", {.selected = true});
        selectedTint = CountColor(from, tint);
        from = drawList->VtxBuffer.Size;
        facet::Chip("Plain");
        plainTint = CountColor(from, tint);
        plainWidth = ImGui::GetItemRectSize().x;
        facet::Chip("Plain##extras", {.icon = facet::Icon("star"), .color = facet::ToU32(facet::T().colors.chart[0])});
        withExtras = ImGui::GetItemRectSize().x;
        ImGui::BeginDisabled();
        presses += facet::Chip("Disabled", {.removable = true}) ? 1 : 0;
        session.Track("disabled");
        ImGui::EndDisabled();
    });
    session.Frames(10);
    CHECK(selectedTint > 0);
    CHECK(plainTint == 0);
    CHECK(withExtras > plainWidth);
    session.Click(session.ItemCenter("disabled"));
    CHECK(presses == 0);
}

TEST_CASE("A toggle outside a frame returns false")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool value = false;
    CHECK_FALSE(facet::Toggle("Toggle", &value));
    CHECK_FALSE(value);
}
