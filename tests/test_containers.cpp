// Behaviour of the containers family (facet/widgets/containers.h).
//
// One test case per behaviour of an implemented widget; docs/writing-a-widget.md describes the
// pattern. The API checks, and the placeholder checks of the widgets that are still planned, are in
// tests/test_api_containers.cpp.

#include <cfloat>
#include <string>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "internal/focus.h"
#include "internal/list_view.h"
#include "internal/overflow.h"
#include "internal/popup_host.h"
#include "widgets/containers/container_layout.h"
#include "support/session.h"
#include "support/structure_stub_check.h"

namespace
{

using namespace facet::test::structure;

// Restores the theme a test changes.
struct ThemeGuard
{
    facet::Theme saved = facet::T();
    ~ThemeGuard() { facet::SetTheme(saved, false); }
};

// Turns animations off for the guard's lifetime, so opening and closing happen at once.
struct ReducedMotion : ThemeGuard
{
    ReducedMotion()
    {
        facet::Theme theme = facet::T();
        theme.motion.reduceMotion = true;
        facet::SetTheme(theme, false);
    }
};

int CountColor(const ImDrawList* drawList, int from, ImU32 color)
{
    int count = 0;
    for (int i = from; i < drawList->VtxBuffer.Size; ++i)
        count += drawList->VtxBuffer[i].col == color ? 1 : 0;
    return count;
}

} // namespace

// ---- Card ---------------------------------------------------------------------------------------

TEST_CASE("A card frames its content inside its width and padding and is the last item")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float available = 0.0f;
    session.SetUi([&] {
        available = ImGui::GetContentRegionAvail().x;
        facet::BeginCard("Title", {.icon = facet::Icon("layers")});
        ImGui::Button("inside", ImVec2(-FLT_MIN, 0.0f));
        session.Track("inside");
        facet::EndCard();
        session.Track("card");
        ImGui::Button("after");
        session.Track("after");
    });
    session.Frame();
    const ImRect card = session.ItemRect("card");
    const ImRect button = session.ItemRect("inside");
    const float padding = facet::Px(facet::T().spacing.lg);
    CHECK(card.GetWidth() == doctest::Approx(available));
    CHECK(button.Min.x == doctest::Approx(card.Min.x + padding));
    CHECK(button.Max.x == doctest::Approx(card.Max.x - padding));
    CHECK(button.Min.y > card.Min.y + padding);  // the header line is above it
    CHECK(card.Max.y >= button.Max.y + padding - 1.0f);
    CHECK(session.ItemRect("after").Min.y >= card.Max.y);
}

TEST_CASE("A card draws its bgSecondary surface under the content")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int fillVertices = 0;
    int fillIndex = -1;
    int contentIndex = -1;
    session.SetUi([&] {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImU32 fill = facet::ToU32(facet::T().colors.bgSecondary);
        const ImU32 text = ImGui::GetColorU32(ImGuiCol_Text);
        const int before = drawList->VtxBuffer.Size;
        const int indexBefore = drawList->IdxBuffer.Size;
        facet::BeginCard("##plain");
        ImGui::TextUnformatted("content");
        facet::EndCard();
        fillVertices = CountColor(drawList, before, fill);
        // The draw order is the order of the merged index buffer (the vertices keep the order they
        // were written in): the surface's triangles come first, so the content is drawn over it.
        fillIndex = -1;
        contentIndex = -1;
        int position = 0;
        for (const ImDrawCmd& cmd : drawList->CmdBuffer)
        {
            for (unsigned int i = 0; i < cmd.ElemCount; ++i, ++position)
            {
                if (static_cast<int>(cmd.IdxOffset + i) < indexBefore)
                    continue;
                const ImU32 col = drawList->VtxBuffer[cmd.VtxOffset + drawList->IdxBuffer[cmd.IdxOffset + i]].col;
                if (fillIndex < 0 && col == fill)
                    fillIndex = position;
                if (contentIndex < 0 && col == text)
                    contentIndex = position;
            }
        }
    });
    session.Frame();
    CHECK(fillVertices > 0);
    CHECK(fillIndex >= 0);
    CHECK(contentIndex > fillIndex);
}

TEST_CASE("A card height is a minimum and a card without a label has no header line")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    session.SetUi([&] {
        facet::BeginCard("##tall", {.height = 200.0f});
        ImGui::TextUnformatted("short");
        facet::EndCard();
        session.Track("tall");
        facet::BeginCard("##plain", {.width = 150.0f});
        ImGui::TextUnformatted("text");
        session.Track("plain.text");
        facet::EndCard();
        session.Track("plain");
    });
    session.Frame();
    CHECK(session.ItemRect("tall").GetHeight() == doctest::Approx(facet::Px(200.0f)).epsilon(0.01));
    CHECK(session.ItemRect("plain").GetWidth() == doctest::Approx(facet::Px(150.0f)));
    const float padding = facet::Px(facet::T().spacing.lg);
    CHECK(session.ItemRect("plain.text").Min.y == doctest::Approx(session.ItemRect("plain").Min.y + padding));
}

TEST_CASE("Cards leave every stack as they found it: nested, skipped, narrow and outside a frame")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values v;
    session.SetUi([&] {
        CHECK_BALANCED({
            facet::CardScope outer("Outer");
            facet::CardScope inner("Inner");
            ImGui::TextUnformatted("nested");
        });
        CHECK_BALANCED({
            facet::BeginCard("Narrow", {.width = 1.0f});
            ImGui::TextUnformatted("text");
            facet::EndCard();
        });
        ImGui::BeginDisabled();
        CHECK_BALANCED({
            facet::BeginCard("Disabled");
            facet::EndCard();
        });
        ImGui::EndDisabled();
    });
    session.Frames(2);
    CHECK(facet::internal::OpenScopeCount() == 0);
    const int before = facet::internal::UsageErrorCount();
    facet::BeginCard("outside a frame");
    facet::EndCard();
    CHECK(facet::internal::OpenScopeCount() == 0);
    CHECK(facet::internal::UsageErrorCount() == before);
}

TEST_CASE("In-flow containers in a collapsed window submit nothing and stay balanced")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values v;
    bool open = true;
    int checked = 0;
    session.SetUi([&] {
        ImGui::SetNextWindowCollapsed(true);
        // A new window lays out its items once to measure them, collapsed or not; after that
        // Begin returns false and the window skips its items.
        if (ImGui::Begin("collapsed"))
        {
            ImGui::End();
            return;
        }
        ++checked;
        CHECK_BALANCED({
            facet::BeginCard("Card");
            facet::EndCard();
        });
        CHECK_BALANCED(CHECK_FALSE(facet::BeginGroupBox("Group")));
        CHECK_BALANCED(CHECK_FALSE(facet::BeginDisclosure("Disclosure", &open)));
        CHECK_BALANCED({
            facet::BeginAccordion("Accordion");
            CHECK_FALSE(facet::BeginAccordionSection("Section", {.defaultOpen = true}));
            facet::EndAccordion();
        });
        CHECK_BALANCED(CHECK_FALSE(facet::BeginScrollArea("Scroll")));
        ImGui::End();
    });
    session.Frames(3);
    CHECK(checked >= 2);
    CHECK(open);
    CHECK(facet::internal::OpenScopeCount() == 0);
}

TEST_CASE("An End of a container without its Begin is reported and changes nothing")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values v;
    int reported = 0;
    session.SetUi([&] {
        const int before = facet::internal::UsageErrorCount();
        const Snapshot start = Snapshot::Take(v);
        facet::EndCard();
        facet::EndGroupBox();
        facet::EndDisclosure();
        facet::EndAccordionSection();
        facet::EndAccordion();
        facet::EndScrollArea();
        facet::EndToolbar();
        Snapshot end = Snapshot::Take(v);
        reported = facet::internal::UsageErrorCount() - before;
        end.errors = start.errors;
        CHECK(end == start);
    });
    session.Frame();
    CHECK(reported == 7);
}

// ---- Group box ----------------------------------------------------------------------------------

TEST_CASE("A group box frames its content under its title and description")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool shown = false;
    session.SetUi([&] {
        shown = facet::BeginGroupBox("Network", {.description = "Applies after a restart"});
        if (shown)
        {
            ImGui::Button("inside", ImVec2(-FLT_MIN, 0.0f));
            session.Track("inside");
            facet::EndGroupBox();
        }
        session.Track("box");
    });
    session.Frame();
    CHECK(shown);
    const ImRect box = session.ItemRect("box");
    const ImRect button = session.ItemRect("inside");
    const float padding = facet::Px(facet::T().spacing.md);
    CHECK(button.Max.x == doctest::Approx(box.Max.x - padding));
    CHECK(button.Min.y > box.Min.y + facet::MeasureText(facet::TextStyle::Headline, "Network").y);
    CHECK(box.Max.y >= button.Max.y + padding - 1.0f);
}

TEST_CASE("A collapsible group box toggles from its title by click and keyboard")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    const ReducedMotion motion;
    bool shown = false;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        shown = facet::BeginGroupBox("Advanced", {.collapsible = true, .defaultOpen = false});
        if (!shown)
            session.Track("title");  // the title row is the last item while the box is closed
        if (shown)
        {
            ImGui::TextUnformatted("content");
            facet::EndGroupBox();
        }
    });
    session.Frame();
    CHECK_FALSE(shown);
    session.Click(session.ItemCenter("title"));
    CHECK(shown);
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Space);
    CHECK_FALSE(shown);
    session.KeyPress(ImGuiKey_Enter);
    CHECK(shown);
}

TEST_CASE("Group boxes leave every stack as they found it, collapsed or open")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values v;
    session.SetUi([&] {
        CHECK_BALANCED(CHECK_FALSE(facet::BeginGroupBox("Closed", {.collapsible = true, .defaultOpen = false})));
        CHECK_BALANCED({
            facet::GroupBoxScope outer("Outer");
            CHECK(static_cast<bool>(outer));
            facet::GroupBoxScope inner("Inner", {.collapsible = true});
            CHECK(static_cast<bool>(inner));
        });
        ImGui::BeginDisabled();
        CHECK_BALANCED({ facet::GroupBoxScope box("Disabled", {.collapsible = true, .width = 1.0f}); });
        ImGui::EndDisabled();
    });
    session.Frames(2);
    CHECK(facet::internal::OpenScopeCount() == 0);
}

// ---- Disclosure ---------------------------------------------------------------------------------

TEST_CASE("A disclosure writes the caller's flag on click, Space and Enter")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    const ReducedMotion motion;
    bool open = false;
    bool shown = false;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        shown = facet::BeginDisclosure("Details", &open, {.summary = "3 settings"});
        if (!shown)
            session.Track("toggle");
        if (shown)
        {
            ImGui::TextUnformatted("content");
            session.Track("content");
            facet::EndDisclosure();
        }
    });
    session.Frame();
    CHECK_FALSE(shown);
    session.Click(session.ItemCenter("toggle"));
    CHECK(open);
    CHECK(shown);
    // The Row style lines the content up with the label, after the chevron.
    CHECK(session.ItemRect("content").Min.x > session.ItemRect("before").Min.x);
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Space);
    CHECK_FALSE(open);
    session.KeyPress(ImGuiKey_Enter);
    CHECK(open);
}

TEST_CASE("A disclosure without a flag keeps its open state under its id")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    const ReducedMotion motion;
    bool shown = false;
    ImRect link;
    session.SetUi([&] {
        shown = facet::BeginDisclosure("Show more", nullptr, {.style = facet::DisclosureStyle::Link, .defaultOpen = true});
        link = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        if (shown)
        {
            ImGui::TextUnformatted("content");
            facet::EndDisclosure();
        }
    });
    session.Frame();
    CHECK(shown);
    // The link is as wide as its content, not the whole row.
    CHECK(link.GetWidth() < ImGui::GetMainViewport()->Size.x * 0.5f);
    session.Click(link.GetCenter());
    CHECK_FALSE(shown);
    session.Frames(3);
    CHECK_FALSE(shown);
}

TEST_CASE("A disclosure opens with an animated height and at once with reduced motion")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool open = false;
    session.SetUi([&] {
        if (facet::BeginDisclosure("Details", &open))
        {
            for (int i = 0; i < 6; ++i)
            {
                ImGui::PushID(i);
                ImGui::Button("row", ImVec2(100.0f, 30.0f));
                ImGui::PopID();
            }
            facet::EndDisclosure();
        }
        ImGui::Button("after");
        session.Track("after");
    });
    session.Frame();
    const float closedY = session.ItemRect("after").Min.y;
    open = true;
    session.Frames(4);
    const float midY = session.ItemRect("after").Min.y;
    session.Seconds(1.0f);
    const float openY = session.ItemRect("after").Min.y;
    CHECK(midY > closedY);
    CHECK(midY < openY);

    const ReducedMotion motion;
    open = false;
    session.Frames(2);
    CHECK(session.ItemRect("after").Min.y == doctest::Approx(closedY));
    open = true;
    session.Frames(2);
    CHECK(session.ItemRect("after").Min.y == doctest::Approx(openY));
}

TEST_CASE("A closing disclosure keeps its content out of Tab while it animates")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool open = true;
    ImRect toggle;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        const bool shown = facet::BeginDisclosure("Details", &open);
        toggle = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        if (shown)
        {
            ImGui::Button("inside");
            facet::EndDisclosure();
        }
        ImGui::Button("after");
        session.Track("after");
    });
    session.Frame();
    session.Click(toggle.GetCenter());
    CHECK_FALSE(open);
    // Tab from the toggle skips the content that is closing.
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("after"));
}

TEST_CASE("Disclosures leave every stack as they found it, open, closing and closed")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values v;
    bool open = true;
    bool closed = false;
    session.SetUi([&] {
        CHECK_BALANCED(CHECK_FALSE(facet::BeginDisclosure("Closed", &closed)));
        CHECK_BALANCED({
            facet::DisclosureScope outer("Outer", &open);
            facet::DisclosureScope inner("Inner", nullptr, {.defaultOpen = true});
            ImGui::TextUnformatted("inside");
        });
        ImGui::BeginDisabled();
        CHECK_BALANCED({ facet::DisclosureScope disabled("Disabled", nullptr, {.style = facet::DisclosureStyle::Link}); });
        ImGui::EndDisabled();
    });
    session.Frames(2);
    open = false;
    session.Frames(2);  // closing: still submitted, clipped
    session.Seconds(1.0f);
    CHECK(facet::internal::OpenScopeCount() == 0);
}

// ---- Accordion ----------------------------------------------------------------------------------

TEST_CASE("A single-open accordion closes the other sections when one opens")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    const ReducedMotion motion;
    bool shown[3] = {};
    ImRect headers[3];
    session.SetUi([&] {
        facet::BeginAccordion("acc", {.singleOpen = true});
        const char* labels[3] = {"One", "Two", "Three"};
        for (int i = 0; i < 3; ++i)
        {
            shown[i] = facet::BeginAccordionSection(labels[i], {.defaultOpen = i == 0});
            headers[i] = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
            if (shown[i])
            {
                ImGui::TextUnformatted("content");
                facet::EndAccordionSection();
            }
        }
        facet::EndAccordion();
    });
    session.Frames(2);
    CHECK(shown[0]);
    CHECK_FALSE(shown[1]);
    session.Click(headers[2].GetCenter());
    session.Frame();
    CHECK_FALSE(shown[0]);
    CHECK(shown[2]);
}

TEST_CASE("Up and Down move between accordion headers past the open content")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    const ReducedMotion motion;
    ImGuiID headers[3] = {};
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        facet::BeginAccordion("acc");
        const char* labels[3] = {"One", "Two", "Three"};
        for (int i = 0; i < 3; ++i)
        {
            const bool shown = facet::BeginAccordionSection(labels[i], {.defaultOpen = true});
            headers[i] = ImGui::GetItemID();
            if (shown)
            {
                ImGui::PushID(i);
                ImGui::Button("inside");
                ImGui::PopID();
                facet::EndAccordionSection();
            }
        }
        facet::EndAccordion();
    });
    session.Frames(2);
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == headers[0]);
    session.KeyPress(ImGuiKey_DownArrow);
    CHECK(ImGui::GetFocusID() == headers[1]);
    session.KeyPress(ImGuiKey_End);
    CHECK(ImGui::GetFocusID() == headers[2]);
    session.KeyPress(ImGuiKey_UpArrow);
    CHECK(ImGui::GetFocusID() == headers[1]);
    session.KeyPress(ImGuiKey_Home);
    CHECK(ImGui::GetFocusID() == headers[0]);
}

TEST_CASE("ImGui::SetNextItemOpen opens and closes an accordion section")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    const ReducedMotion motion;
    int request = -1;
    bool shown = false;
    session.SetUi([&] {
        facet::BeginAccordion("acc");
        if (request >= 0)
            ImGui::SetNextItemOpen(request == 1);
        shown = facet::BeginAccordionSection("Section");
        if (shown)
            facet::EndAccordionSection();
        facet::EndAccordion();
    });
    session.Frame();
    CHECK_FALSE(shown);
    request = 1;
    session.Frames(2);
    CHECK(shown);
    request = 0;
    session.Frames(2);
    CHECK_FALSE(shown);
}

TEST_CASE("Accordions leave every stack as they found it")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values v;
    session.SetUi([&] {
        CHECK_BALANCED({
            facet::AccordionScope accordion("acc");
            {
                facet::AccordionSectionScope open("Open", {.defaultOpen = true});
                facet::AccordionScope nested("nested");
                facet::AccordionSectionScope inner("Inner", {.defaultOpen = true});
            }
            facet::AccordionSectionScope closed("Closed");
            CHECK_FALSE(static_cast<bool>(closed));
        });
        CHECK_BALANCED({ facet::AccordionScope accordion("empty", {.width = 1.0f}); });
        // A section outside an accordion still works on its own.
        CHECK_BALANCED({ facet::AccordionSectionScope alone("Alone", {.defaultOpen = true}); });
    });
    session.Frames(2);
    CHECK(facet::internal::OpenScopeCount() == 0);
}

// ---- Scroll area --------------------------------------------------------------------------------

TEST_CASE("A scroll area scrolls its content with the wheel")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float scroll = 0.0f;
    session.SetUi([&] {
        if (facet::BeginScrollArea("area", {.height = 120.0f}))
        {
            for (int i = 0; i < 40; ++i)
            {
                ImGui::PushID(i);
                ImGui::Button("row", ImVec2(-FLT_MIN, 0.0f));
                ImGui::PopID();
            }
            scroll = ImGui::GetScrollY();
            facet::EndScrollArea();
        }
        session.Track("area");
    });
    session.Frames(2);
    CHECK(session.ItemRect("area").GetHeight() == doctest::Approx(facet::Px(120.0f)));
    CHECK(scroll == 0.0f);
    session.Wheel(session.ItemCenter("area"), -2.0f);
    session.Seconds(1.0f);
    CHECK(scroll > 0.0f);
}

TEST_CASE("Scroll areas leave every stack as they found it")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values v;
    int frame = 0;
    session.SetUi([&] {
        ++frame;
        CHECK_BALANCED({
            facet::ScrollAreaScope outer("outer", {.height = 100.0f});
            CHECK(static_cast<bool>(outer));
            if (outer)
            {
                facet::ScrollAreaScope inner("inner", {.height = 40.0f});
                ImGui::TextUnformatted("text");
            }
        });
        ImGui::BeginDisabled();
        CHECK_BALANCED({ facet::ScrollAreaScope area("disabled", {.horizontal = true, .height = 40.0f}); });
        ImGui::EndDisabled();
        // Far below the visible part of the window: out of sight.
        ImGui::Dummy(ImVec2(10.0f, 2000.0f));
        CHECK_BALANCED({
            facet::ScrollAreaScope hidden("hidden", {.height = 40.0f});
            // A new child window lays out its items once to measure them, like any new window.
            if (frame > 1)
                CHECK_FALSE(static_cast<bool>(hidden));
        });
    });
    session.Frames(3);
    CHECK(facet::internal::OpenScopeCount() == 0);
}

// ---- Toolbar ------------------------------------------------------------------------------------

namespace
{

// A toolbar of four buttons A, B, C, D with priorities 0, 2, 0, 1 and a separator after B; C shows
// its label, so it is wider than the square icon buttons. Records which buttons showed and how
// often each was activated.
struct ToolbarProbe
{
    const char* labels[4] = {"A", "B", "C", "D"};
    int priorities[4] = {0, 2, 0, 1};
    bool iconOnly[4] = {true, true, false, true};
    bool shown[4] = {};
    int presses[4] = {};
    float width = 0.0f;  // units; 0 fills
    int separatorVertices = 0;
    ImRect bar;
    ImGuiID moreId = 0;
    bool menuOpen = false;

    void Draw(facet::test::Session& session)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const int before = drawList->VtxBuffer.Size;
        facet::BeginToolbar("tools", {.width = width});
        for (int i = 0; i < 4; ++i)
        {
            if (facet::ToolbarButton(labels[i], facet::Icon("plus"), {.iconOnly = iconOnly[i], .priority = priorities[i]}))
                ++presses[i];
            shown[i] = ImGui::GetItemID() == ImGui::GetID(labels[i]);
            if (shown[i])
                session.Track(labels[i]);
            if (i == 1)
                facet::ToolbarSeparator();
        }
        moreId = ImGui::GetID("##more");
        menuOpen = facet::internal::IsPopupLevelOpen(ImGui::GetID("##moremenu"));
        facet::EndToolbar();
        bar = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        separatorVertices = CountColor(drawList, before, facet::ToU32(facet::T().colors.separator));
    }
};

} // namespace

TEST_CASE("A toolbar button activates on click, Space and Enter, and the arrows move between buttons")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ToolbarProbe probe;
    session.SetUi([&] { probe.Draw(session); });
    session.Frames(2);
    for (int i = 0; i < 4; ++i)
        CHECK(probe.shown[i]);
    CHECK(probe.separatorVertices > 0);
    // Icon-only buttons are square at the control height; C shows its label too.
    CHECK(session.ItemRect("A").GetWidth() == doctest::Approx(facet::ControlHeight(facet::ControlSize::Regular)));
    CHECK(session.ItemRect("C").GetWidth() > session.ItemRect("A").GetWidth());
    session.Click(session.ItemCenter("B"));
    CHECK(probe.presses[1] == 1);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("B"));
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(ImGui::GetFocusID() == session.ItemId("C"));
    session.KeyPress(ImGuiKey_Space);
    session.KeyPress(ImGuiKey_Enter);
    CHECK(probe.presses[2] == 2);
    session.KeyPress(ImGuiKey_LeftArrow);
    CHECK(ImGui::GetFocusID() == session.ItemId("B"));
}

TEST_CASE("A narrow toolbar moves the lowest priority into the more menu, the last of equals first")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ToolbarProbe probe;
    session.SetUi([&] { probe.Draw(session); });
    session.Frames(2);
    const float scale = facet::Px(1.0f);
    const float button = session.ItemRect("A").GetWidth() / scale;  // units
    const float wide = session.ItemRect("C").GetWidth() / scale;
    const float gap = facet::T().spacing.xs;
    const float separator = facet::T().spacing.sm;
    // Short of the whole row by half of what C is wider than the more button: C (priority 0, the
    // last of the two lowest) goes first and the more button takes its place.
    probe.width = button * 3.0f + wide + separator + gap * 4.0f - (wide - button) * 0.5f;
    session.Frames(3);
    CHECK(probe.shown[0]);
    CHECK(probe.shown[1]);
    CHECK_FALSE(probe.shown[2]);
    CHECK(probe.shown[3]);
    CHECK(probe.separatorVertices > 0);
    // Room for B, the separator, D and the more button: A (priority 0) goes next, D (priority 1)
    // stays, and the separator keeps dividing B from D.
    probe.width = button * 3.0f + separator + gap * 3.0f;
    session.Frames(3);
    CHECK_FALSE(probe.shown[0]);
    CHECK(probe.shown[1]);
    CHECK_FALSE(probe.shown[2]);
    CHECK(probe.shown[3]);
    CHECK(probe.separatorVertices > 0);
    CHECK(probe.bar.GetWidth() == doctest::Approx(facet::Px(probe.width)));
    // Nothing leaves the bar.
    CHECK(session.ItemRect("D").Max.x <= probe.bar.Max.x + 0.5f);
    CHECK(session.ItemRect("B").Min.x >= probe.bar.Min.x - 0.5f);
    // Room for one button and the more button: only B, the highest priority, stays, and the
    // separator goes, because nothing is left on its other side.
    probe.width = button * 2.0f + gap;
    session.Frames(3);
    CHECK(probe.shown[1]);
    CHECK_FALSE(probe.shown[0]);
    CHECK_FALSE(probe.shown[2]);
    CHECK_FALSE(probe.shown[3]);
    CHECK(probe.separatorVertices == 0);
    // Wide again: everything comes back.
    probe.width = 0.0f;
    session.Frames(3);
    for (int i = 0; i < 4; ++i)
        CHECK(probe.shown[i]);
}

TEST_CASE("The rows of a toolbar's more menu activate the hidden buttons")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ToolbarProbe probe;
    session.SetUi([&] { probe.Draw(session); });
    const float button = facet::ControlHeight(facet::ControlSize::Regular) / facet::Px(1.0f);
    probe.width = button * 3.0f + facet::T().spacing.xs * 2.0f;
    session.Frames(3);
    REQUIRE_FALSE(probe.shown[0]);
    REQUIRE_FALSE(probe.shown[2]);
    REQUIRE_FALSE(probe.shown[3]);
    const float h = facet::ControlHeight(facet::ControlSize::Regular);
    const ImVec2 more(probe.bar.Max.x - h * 0.5f, probe.bar.GetCenter().y);
    // By mouse: the menu lists A, C and D in toolbar order.
    session.Click(more);
    session.Frames(2);
    CHECK(probe.menuOpen);
    session.Click(session.ItemCenter("B"));  // a click outside closes the menu and B sees nothing
    session.Frames(2);
    CHECK_FALSE(probe.menuOpen);
    CHECK(probe.presses[1] == 0);
    // By keyboard: B, then Right to the more button, Enter opens the menu on its first row.
    session.Click(session.ItemCenter("B"));
    CHECK(probe.presses[1] == 1);
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(ImGui::GetFocusID() == probe.moreId);
    session.KeyPress(ImGuiKey_Enter);
    session.Frames(2);
    CHECK(probe.menuOpen);
    session.KeyPress(ImGuiKey_Enter);
    session.Frame();
    CHECK(probe.presses[0] == 1);
    CHECK(probe.presses[2] == 0);
    CHECK_FALSE(probe.menuOpen);
    // The focus went back to the more button: the second row is C.
    CHECK(ImGui::GetFocusID() == probe.moreId);
    session.KeyPress(ImGuiKey_Enter);
    session.Frames(2);
    session.KeyPress(ImGuiKey_DownArrow);
    session.KeyPress(ImGuiKey_Enter);
    session.Frame();
    CHECK(probe.presses[2] == 1);
    CHECK(probe.presses[0] == 1);
    // Activations are counted once.
    session.Frames(3);
    CHECK(probe.presses[2] == 1);
}

TEST_CASE("Items after a toolbar spacer sit at the trailing end")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ImRect bar;
    float available = 0.0f;
    session.SetUi([&] {
        available = ImGui::GetContentRegionAvail().x;
        facet::BeginToolbar("tools");
        (void)facet::ToolbarButton("Start", facet::Icon("plus"));
        facet::ToolbarSpacer();
        (void)facet::ToolbarButton("End", facet::Icon("settings"), {.iconOnly = false});
        session.Track("end");
        facet::EndToolbar();
        bar = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    });
    session.Frames(3);
    CHECK(session.ItemRect("end").Max.x == doctest::Approx(bar.Max.x).epsilon(0.001));
    CHECK(bar.GetWidth() == doctest::Approx(available));
}

TEST_CASE("Toolbars leave every stack as they found it")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values v;
    session.SetUi([&] {
        CHECK_BALANCED({
            facet::ToolbarScope outer("outer");
            CHECK_FALSE(facet::ToolbarButton("A", facet::Icon("plus")));
            facet::ToolbarSeparator();
            ImGui::TextUnformatted("text");
            facet::ToolbarSpacer();
            facet::ToolbarScope inner("inner", {.size = facet::ControlSize::Small});
            CHECK_FALSE(facet::ToolbarButton("B", facet::Icon("plus")));
        });
        CHECK_BALANCED({
            facet::ToolbarScope narrow("narrow", {.width = 1.0f});
            CHECK_FALSE(facet::ToolbarButton("A", facet::Icon("plus")));
            CHECK_FALSE(facet::ToolbarButton("B", facet::Icon("plus")));
        });
        ImGui::BeginDisabled();
        CHECK_BALANCED({
            facet::ToolbarScope disabled("disabled");
            CHECK_FALSE(facet::ToolbarButton("A", facet::Icon("plus")));
        });
        ImGui::EndDisabled();
        // Outside a toolbar a tool button is a plain button of its own.
        CHECK_BALANCED(CHECK_FALSE(facet::ToolbarButton("Alone", facet::Icon("plus"))));
        CHECK_BALANCED(facet::ToolbarSeparator());
        CHECK_BALANCED(facet::ToolbarSpacer());
    });
    session.Frames(3);
    CHECK(facet::internal::OpenScopeCount() == 0);
    const int before = facet::internal::UsageErrorCount();
    facet::BeginToolbar("outside a frame");
    CHECK_FALSE(facet::ToolbarButton("A", facet::Icon("plus")));
    facet::EndToolbar();
    CHECK(facet::internal::OpenScopeCount() == 0);
    CHECK(facet::internal::UsageErrorCount() == before);
}

// ---- Shell --------------------------------------------------------------------------------------

namespace
{

// A shell over the session window, with a button inside and a button of the page under it.
struct ShellProbe
{
    const char* label = "Shell";  // one per test: Facet keeps a shell's place for the whole process
    bool open = true;
    facet::ShellOptions options{.initialSize = {300.0f, 200.0f}, .closeOnEscape = true};
    bool shown = false;
    int presses = 0;
    int behind = 0;
    ImRect window;      // the shell window, pixels
    ImGuiID windowId = 0;

    void Draw(facet::test::Session& session)
    {
        // A wide button of the page, under the middle of the display where the shell opens.
        ImGui::SetCursorScreenPos(ImVec2(0.0f, 250.0f));
        if (ImGui::Button("behind", ImVec2(800.0f, 100.0f)))
            ++behind;
        shown = facet::BeginShell(label, &open, options);
        if (shown)
        {
            ImGuiWindow* root = ImGui::GetCurrentWindow()->RootWindow;
            window = root->Rect();
            windowId = root->ID;
            if (ImGui::Button("inside"))
                ++presses;
            session.Track("inside");
            facet::EndShell();
        }
    }
    // The middle of the title bar's free part, pixels.
    ImVec2 Title() const { return ImVec2(window.Min.x + window.GetWidth() * 0.4f, window.Min.y + facet::Px(10.0f)); }
};

float Units(float pixels)
{
    return pixels / facet::Px(1.0f);
}

} // namespace

TEST_CASE("A shell opens centered at its initial size as a window of its own and closes")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ShellProbe probe{.label = "Shell###open"};
    session.SetUi([&] { probe.Draw(session); });
    session.Frames(2);
    REQUIRE(probe.shown);
    CHECK(Units(probe.window.GetWidth()) == doctest::Approx(300.0f));
    CHECK(Units(probe.window.GetHeight()) == doctest::Approx(200.0f));
    CHECK(probe.window.GetCenter().x == doctest::Approx(400.0f).epsilon(0.01));
    CHECK(probe.window.GetCenter().y == doctest::Approx(300.0f).epsilon(0.01));
    // Its own top-level window, not a child of the page it was called from.
    CHECK(probe.windowId != ImGui::FindWindowByName("test")->ID);
    session.Click(session.ItemCenter("inside"));
    CHECK(probe.presses == 1);
    // The close button at the end of the title bar writes the caller's flag.
    const float h = facet::Px(facet::T().containers.titleBarHeight);
    session.Click(ImVec2(probe.window.Max.x - h * 0.5f, probe.window.Min.y + h * 0.5f));
    CHECK_FALSE(probe.open);
    CHECK_FALSE(probe.shown);
    probe.open = true;
    session.Frames(2);
    CHECK(probe.shown);
    // Escape closes it with closeOnEscape.
    session.Click(session.ItemCenter("inside"));
    session.KeyPress(ImGuiKey_Escape);
    CHECK_FALSE(probe.open);
}

TEST_CASE("A click on a shell never reaches the page behind it")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ShellProbe probe{.label = "Shell###behind"};
    session.SetUi([&] { probe.Draw(session); });
    session.Frames(2);
    REQUIRE(probe.shown);
    // The middle of the shell, over the page button: the content area, empty there.
    const ImVec2 middle(probe.window.GetCenter().x, 300.0f);
    REQUIRE(probe.window.Contains(middle));
    session.Click(middle);
    CHECK(probe.behind == 0);
    // A resize band and the title bar over the button are the shell's too.
    session.Click(ImVec2(probe.window.Min.x + 2.0f, 300.0f));
    CHECK(probe.behind == 0);
    // Beside the shell the button works.
    session.Click(ImVec2(20.0f, 300.0f));
    CHECK(probe.behind == 1);
}

TEST_CASE("Dragging a shell's title bar moves it and snaps it to the display edges")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ShellProbe probe{.label = "Shell###drag"};
    session.SetUi([&] { probe.Draw(session); });
    session.Frames(2);
    const ImRect start = probe.window;
    const ImVec2 grab = probe.Title();
    session.Drag({grab, ImVec2(grab.x + 40.0f, grab.y + 30.0f)});
    session.Frame();
    CHECK(probe.window.Min.x == doctest::Approx(start.Min.x + 40.0f));
    CHECK(probe.window.Min.y == doctest::Approx(start.Min.y + 30.0f));
    CHECK(probe.window.GetWidth() == doctest::Approx(start.GetWidth()));
    // Within spacing.lg of the left and top edges it snaps into the corner.
    const float snap = facet::Px(facet::T().spacing.lg);
    const ImVec2 at = probe.Title();
    session.Drag({at, ImVec2(at.x - (probe.window.Min.x - snap * 0.5f), at.y - (probe.window.Min.y - snap * 0.5f))});
    session.Frame();
    CHECK(probe.window.Min.x == doctest::Approx(0.0f));
    CHECK(probe.window.Min.y == doctest::Approx(0.0f));
    // Dragged past the display, it stays inside.
    const ImVec2 away = probe.Title();
    session.Drag({away, ImVec2(away.x + 2000.0f, away.y + 2000.0f)});
    session.Frame();
    CHECK(probe.window.Max.x == doctest::Approx(800.0f));
    CHECK(probe.window.Max.y == doctest::Approx(600.0f));
}

TEST_CASE("A shell resizes from every edge and corner within its minimum and maximum size")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ShellProbe probe{.label = "Shell###resize"};
    probe.options.minSize = {200.0f, 150.0f};
    probe.options.maxSize = {400.0f, 300.0f};
    session.SetUi([&] { probe.Draw(session); });
    session.Frames(2);
    const float band = facet::Px(facet::T().containers.resizeBorder);
    struct Case
    {
        float fx, fy;     // where on the window: 0 left or top, 0.5 middle, 1 right or bottom
        float dx, dy;     // the drag
        float ex, ey;     // the expected change of width and height
        bool movesX, movesY;  // the left or top edge moves
    };
    const Case cases[] = {
        {1.0f, 0.5f, 30.0f, 0.0f, 30.0f, 0.0f, false, false},   // right
        {0.0f, 0.5f, -30.0f, 0.0f, 30.0f, 0.0f, true, false},   // left
        {0.5f, 1.0f, 0.0f, 20.0f, 0.0f, 20.0f, false, false},   // bottom
        {0.5f, 0.0f, 0.0f, -20.0f, 0.0f, 20.0f, false, true},   // top
        {1.0f, 1.0f, -20.0f, -10.0f, -20.0f, -10.0f, false, false},  // bottom-right
        {0.0f, 0.0f, 10.0f, 10.0f, -10.0f, -10.0f, true, true},  // top-left
        {1.0f, 0.0f, 10.0f, 10.0f, 10.0f, -10.0f, false, true},  // top-right
        {0.0f, 1.0f, -10.0f, 10.0f, 10.0f, 10.0f, true, false},  // bottom-left
    };
    for (const Case& c : cases)
    {
        CAPTURE(c.fx);
        CAPTURE(c.fy);
        const ImRect before = probe.window;
        // Half a band inside the edge, so the point lies on the band.
        const float x = before.Min.x + band * 0.5f + (before.GetWidth() - band) * c.fx;
        const float y = before.Min.y + band * 0.5f + (before.GetHeight() - band) * c.fy;
        session.Drag({ImVec2(x, y), ImVec2(x + c.dx, y + c.dy)});
        session.Frame();
        CHECK(probe.window.GetWidth() == doctest::Approx(before.GetWidth() + c.ex));
        CHECK(probe.window.GetHeight() == doctest::Approx(before.GetHeight() + c.ey));
        CHECK(probe.window.Min.x == doctest::Approx(c.movesX ? before.Min.x + c.dx : before.Min.x));
        CHECK(probe.window.Min.y == doctest::Approx(c.movesY ? before.Min.y + c.dy : before.Min.y));
    }
    // The limits: far out stops at the maximum, far in at the minimum.
    ImRect r = probe.window;
    session.Drag({ImVec2(r.Max.x - band * 0.5f, r.Max.y - band * 0.5f), ImVec2(r.Max.x + 600.0f, r.Max.y + 600.0f)});
    session.Frame();
    CHECK(Units(probe.window.GetWidth()) <= 400.0f + 0.01f);
    CHECK(Units(probe.window.GetHeight()) <= 300.0f + 0.01f);
    r = probe.window;
    session.Drag({ImVec2(r.Max.x - band * 0.5f, r.Max.y - band * 0.5f), ImVec2(r.Min.x, r.Min.y)});
    session.Frame();
    CHECK(Units(probe.window.GetWidth()) == doctest::Approx(200.0f));
    CHECK(Units(probe.window.GetHeight()) == doctest::Approx(150.0f));
}

TEST_CASE("A shell keeps its place in units and comes back into the display at a larger scale")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    const float savedScale = facet::UiScale();
    ShellProbe probe{.label = "Shell###scale"};
    session.SetUi([&] { probe.Draw(session); });
    session.Frames(2);
    // Into the bottom-right corner.
    const ImVec2 grab = probe.Title();
    session.Drag({grab, ImVec2(grab.x + 2000.0f, grab.y + 2000.0f)});
    session.Frame();
    REQUIRE(probe.window.Max.x == doctest::Approx(800.0f));
    const float unitsWide = Units(probe.window.GetWidth());
    facet::SetUiScale(savedScale * 2.0f);
    session.Frames(2);
    // Twice as large in pixels, the same in units, and moved back into the display.
    CHECK(Units(probe.window.GetWidth()) == doctest::Approx(unitsWide));
    CHECK(probe.window.Max.x <= 800.0f + 0.5f);
    CHECK(probe.window.Max.y <= 600.0f + 0.5f);
    CHECK(probe.window.Min.x >= -0.5f);
    facet::SetUiScale(savedScale);
    session.Frames(2);
    CHECK(Units(probe.window.GetWidth()) == doctest::Approx(unitsWide));
    CHECK(probe.window.Max.x <= 800.0f + 0.5f);
}

TEST_CASE("A shell's place and size come back after a restart and fit a smaller display")
{
    std::string ini;
    ImRect first;
    {
        facet::test::Session session;
        REQUIRE(session.Ready());
        ShellProbe probe{.label = "Shell###restart"};
        session.SetUi([&] { probe.Draw(session); });
        session.Frames(2);
        const ImVec2 grab = probe.Title();
        session.Drag({grab, ImVec2(grab.x + 120.0f, grab.y + 90.0f)});
        session.Frame();
        first = probe.window;
        ini = ImGui::SaveIniSettingsToMemory();
    }
    // The record holds the place in units; move it in the text, so the next run can only find the
    // new place through the ini file.
    const std::size_t record = ini.find("[Shell,");
    REQUIRE(record != std::string::npos);
    const std::size_t pos = ini.find("pos=", record);
    REQUIRE(pos != std::string::npos);
    ini.replace(pos, ini.find('\n', pos) - pos, "pos=10.00,20.00");
    {
        facet::test::Session session;
        REQUIRE(session.Ready());
        ImGui::LoadIniSettingsFromMemory(ini.c_str());
        ShellProbe probe{.label = "Shell###restart"};
        session.SetUi([&] { probe.Draw(session); });
        session.Frames(2);
        CHECK(probe.window.Min.x == doctest::Approx(facet::Px(10.0f)));
        CHECK(probe.window.Min.y == doctest::Approx(facet::Px(20.0f)));
        CHECK(probe.window.GetWidth() == doctest::Approx(first.GetWidth()));
        CHECK(probe.window.GetHeight() == doctest::Approx(first.GetHeight()));
    }
    {
        // The host now runs at a smaller resolution: the shell is clamped back inside it.
        facet::test::Session session({.displaySize = ImVec2(400.0f, 300.0f)});
        REQUIRE(session.Ready());
        ImGui::LoadIniSettingsFromMemory(ini.c_str());
        ShellProbe probe{.label = "Shell###restart"};
        session.SetUi([&] { probe.Draw(session); });
        session.Frames(2);
        CHECK(probe.window.Max.x <= 400.0f + 0.5f);
        CHECK(probe.window.Max.y <= 300.0f + 0.5f);
        CHECK(probe.window.Min.x >= -0.5f);
        CHECK(probe.window.Min.y >= -0.5f);
    }
}

TEST_CASE("A collapsible shell collapses to its title bar on a double click")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ShellProbe probe{.label = "Shell###collapse"};
    probe.options.collapsible = true;
    probe.options.persist = false;
    session.SetUi([&] { probe.Draw(session); });
    session.Frames(2);
    REQUIRE(probe.shown);
    const ImVec2 title = probe.Title();
    session.DoubleClick(title);
    session.Frame();
    CHECK_FALSE(probe.shown);
    ImGuiWindow* window = ImGui::FindWindowByName(probe.label);
    REQUIRE(window != nullptr);
    CHECK(window->Size.y == doctest::Approx(facet::Px(facet::T().containers.titleBarHeight)));
    session.Seconds(1.0f);  // past the double click time, so the next two clicks count as a pair
    session.DoubleClick(title);
    session.Frame();
    CHECK(probe.shown);
}

TEST_CASE("Shells leave every stack as they found it: open, closed, collapsed, nested and outside a frame")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values v;
    bool closed = false;
    bool open = true;
    session.SetUi([&] {
        CHECK_BALANCED(CHECK_FALSE(facet::BeginShell("Closed", &closed)));
        CHECK_BALANCED({
            facet::ShellScope outer("Outer", nullptr, {.initialSize = {300.0f, 200.0f}});
            if (outer)
            {
                ImGui::TextUnformatted("outer");
                facet::ShellScope inner("Inner", &open, {.initialSize = {200.0f, 150.0f}, .glass = false});
                ImGui::TextUnformatted("inner");
            }
        });
        CHECK_BALANCED({
            facet::ShellScope collapsed("Collapsed", nullptr, {.collapsible = true, .persist = false});
        });
        ImGui::BeginDisabled();
        CHECK_BALANCED({ facet::ShellScope disabled("Disabled", &open, {.minSize = {1.0f, 1.0f}}); });
        ImGui::EndDisabled();
    });
    session.Frames(3);
    CHECK(open);
    CHECK(facet::internal::OpenScopeCount() == 0);
    const int before = facet::internal::UsageErrorCount();
    CHECK_FALSE(facet::BeginShell("outside a frame", nullptr));
    CHECK(facet::internal::UsageErrorCount() == before);
}

// ---- Title bar ----------------------------------------------------------------------------------

TEST_CASE("A title bar puts the caller's items before its window buttons and reports a press")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    facet::WindowButton pressed = facet::WindowButton::None;
    int saves = 0;
    ImRect bar;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        facet::BeginTitleBar("Editor", {.icon = facet::Icon("monitor"), .detail = "Placeholder detail", .windowButtons = true});
        if (facet::Button("Save"))
            ++saves;
        session.Track("save");
        facet::EndTitleBar();
        bar = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        pressed = facet::TitleBarButtonPressed();
        if (pressed != facet::WindowButton::None)
            session.Track("pressed");  // only to keep the frame of the press visible to the test
    });
    session.Frames(3);
    CHECK(bar.GetHeight() == doctest::Approx(facet::Px(facet::T().containers.titleBarHeight)));
    CHECK(bar.GetWidth() == doctest::Approx(ImGui::FindWindowByName("test")->ContentRegionRect.GetWidth()).epsilon(0.02));
    const ImRect save = session.ItemRect("save");
    const float h = facet::ControlHeight(facet::ControlSize::Regular);
    const float gap = facet::Px(facet::T().spacing.xs);
    const float inset = (bar.GetHeight() - h) * 0.5f;
    // The three buttons end at the trailing edge; the items end before them.
    const float buttonsLeft = bar.Max.x - inset - h * 3.0f - gap * 2.0f;
    CHECK(save.Max.x <= buttonsLeft);
    CHECK(save.Max.x >= buttonsLeft - facet::Px(facet::T().spacing.sm) - 1.0f);
    CHECK(save.GetCenter().y == doctest::Approx(bar.GetCenter().y).epsilon(0.02));
    session.Click(session.ItemCenter("save"));
    CHECK(saves == 1);

    // A press on each window button is reported on its frame, by mouse and keyboard.
    const facet::WindowButton expected[3] = {facet::WindowButton::Minimize, facet::WindowButton::Maximize,
                                             facet::WindowButton::Close};
    for (int i = 0; i < 3; ++i)
    {
        facet::WindowButton seen = facet::WindowButton::None;
        ImRect plain;
        session.SetUi([&] {
            facet::BeginTitleBar("Editor", {.windowButtons = true});
            facet::EndTitleBar();
            plain = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
            if (facet::TitleBarButtonPressed() != facet::WindowButton::None)
                seen = facet::TitleBarButtonPressed();
        });
        session.Frame();
        const float left = plain.Max.x - inset - h * 3.0f - gap * 2.0f;
        session.Click(ImVec2(left + h * 0.5f + (h + gap) * static_cast<float>(i), plain.GetCenter().y));
        CHECK(seen == expected[i]);
    }
    facet::WindowButton seen = facet::WindowButton::None;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        facet::BeginTitleBar("Editor", {.windowButtons = true});
        facet::EndTitleBar();
        if (facet::TitleBarButtonPressed() != facet::WindowButton::None)
            seen = facet::TitleBarButtonPressed();
    });
    session.Frame();
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Enter);
    CHECK(seen == facet::WindowButton::Minimize);
}

TEST_CASE("A title bar records its caption, items and buttons for the hit test")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    session.SetUi([&] {
        facet::BeginTitleBar("Editor", {.windowButtons = true});
        ImGui::Button("item");
        facet::EndTitleBar();
    });
    session.Frames(3);
    const facet::internal::TitleBarRegions* regions = nullptr;
    REQUIRE(facet::internal::PreviousTitleBarRegions(&regions) == 1);
    CHECK(regions[0].bar.GetHeight() > 0.0f);
    CHECK(regions[0].items.GetWidth() > 0.0f);
    CHECK(regions[0].items.Max.x <= regions[0].buttons[0].Min.x);
    CHECK(regions[0].buttons[2].Max.x <= regions[0].bar.Max.x);
    CHECK(regions[0].rootWindow == ImGui::FindWindowByName("test")->ID);
}

TEST_CASE("A narrow title bar cuts its title and keeps every part inside")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ImDrawList* drawList = nullptr;
    int from = 0;
    ImRect bar;
    session.SetUi([&] {
        drawList = ImGui::GetWindowDrawList();
        from = drawList->VtxBuffer.Size;
        facet::BeginTitleBar("A title much longer than the bar", {.windowButtons = true, .width = 140.0f});
        facet::EndTitleBar();
        bar = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    });
    session.Frames(2);
    CHECK(bar.GetWidth() == doctest::Approx(facet::Px(140.0f)));
    CHECK(VerticesInside(drawList, from, bar));
}

TEST_CASE("Title bars leave every stack as they found it")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values v;
    session.SetUi([&] {
        CHECK_BALANCED({
            facet::TitleBarScope outer("Outer", {.windowButtons = true, .maximized = true});
            ImGui::TextUnformatted("item");
            facet::TitleBarScope inner("Inner", {.height = 28.0f});
        });
        CHECK_BALANCED({ facet::TitleBarScope narrow("Narrow", {.windowButtons = true, .width = 1.0f}); });
        ImGui::BeginDisabled();
        CHECK_BALANCED({ facet::TitleBarScope disabled("Disabled", {.windowButtons = true}); });
        ImGui::EndDisabled();
        const int before = facet::internal::UsageErrorCount();
        facet::EndTitleBar();
        CHECK(facet::internal::UsageErrorCount() == before + 1);
    });
    session.Frames(2);
    CHECK(facet::internal::OpenScopeCount() == 0);
    const int before = facet::internal::UsageErrorCount();
    facet::BeginTitleBar("outside a frame");
    facet::EndTitleBar();
    CHECK(facet::TitleBarButtonPressed() == facet::WindowButton::None);
    CHECK(facet::internal::OpenScopeCount() == 0);
    CHECK(facet::internal::UsageErrorCount() == before);
}
// ---- Virtual list -------------------------------------------------------------------------------

namespace
{

// A virtual list of `count` rows that counts the rows it submits per frame and remembers the
// area's scroll position.
struct VirtualProbe
{
    int count = 1000000;
    ImGuiSelectionBasicStorage* selection = nullptr;
    facet::VirtualListOptions options{.height = 200.0f};
    bool shown = false;
    int submitted = 0;
    int lastSubmitted = -1;
    int activated = -1;
    float scrollY = 0.0f;
    float scrollMax = 0.0f;
    float contentHeight = 0.0f;

    void Draw(facet::test::Session& session)
    {
        ImGui::Button("before");
        session.Track("before");
        shown = facet::BeginVirtualList("list", count, selection, options);
        submitted = 0;
        if (shown)
        {
            facet::RowRange rows;
            while (facet::NextVisibleRows(&rows))
                for (int row = rows.first; row < rows.last; ++row)
                {
                    if (facet::VirtualListRow(row))
                        activated = row;
                    if (row == 3)
                        session.Track("row3");
                    if (row == 5)
                        session.Track("row5");
                    facet::Text(facet::TextStyle::Body, "Row %d", row);
                    ++submitted;
                    lastSubmitted = row;
                }
            ImGuiWindow* area = ImGui::GetCurrentWindow();
            scrollY = area->Scroll.y;
            scrollMax = area->ScrollMax.y;
            contentHeight = area->ContentSize.y;
            facet::EndVirtualList();
        }
    }
};

} // namespace

TEST_CASE("A virtual list of a million rows submits only the visible rows and reaches the last by keyboard")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    VirtualProbe probe;
    session.SetUi([&] { probe.Draw(session); });
    session.Frames(2);
    REQUIRE(probe.shown);
    const float row = facet::internal::ListRowHeight(facet::ControlSize::Regular);
    const int visible = static_cast<int>(facet::Px(200.0f) / row) + 1;
    CHECK(probe.submitted <= visible + 3);
    // The scroll range covers every row.
    CHECK(probe.contentHeight == doctest::Approx(static_cast<double>(row) * 1000000.0).epsilon(0.001));
    CHECK(probe.scrollMax > 0.0f);
    // Keyboard: Tab into the list, End to the last row.
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_End);
    session.Frames(2);
    CHECK(probe.lastSubmitted == 999999);
    CHECK(probe.scrollY == doctest::Approx(probe.scrollMax));
    CHECK(probe.submitted <= visible + 4);  // the cost stays the same at the end
    session.KeyPress(ImGuiKey_Enter);
    CHECK(probe.activated == 999999);
    session.KeyPress(ImGuiKey_Home);
    session.Frames(2);
    CHECK(probe.scrollY == doctest::Approx(0.0f));
}

TEST_CASE("A virtual list writes the selection on click, Ctrl and Shift, and activates on a double click")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ImGuiSelectionBasicStorage selection;
    VirtualProbe probe;
    probe.count = 50;
    probe.selection = &selection;
    session.SetUi([&] { probe.Draw(session); });
    session.Frames(2);
    session.Click(session.ItemCenter("row3"));
    CHECK(selection.Size == 1);
    CHECK(selection.Contains(3));
    session.KeyDown(ImGuiMod_Ctrl);
    session.Click(session.ItemCenter("row5"));
    session.KeyUp(ImGuiMod_Ctrl);
    CHECK(selection.Size == 2);
    CHECK(selection.Contains(5));
    session.Click(session.ItemCenter("row3"));
    session.KeyDown(ImGuiMod_Shift);
    session.Click(session.ItemCenter("row5"));
    session.KeyUp(ImGuiMod_Shift);
    CHECK(selection.Size == 3);
    CHECK(selection.Contains(4));
    session.DoubleClick(session.ItemCenter("row5"));
    CHECK(probe.activated == 5);
}

TEST_CASE("A virtual list without a selection still activates its rows")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    VirtualProbe probe;
    probe.count = 50;
    session.SetUi([&] { probe.Draw(session); });
    session.Frames(2);
    session.DoubleClick(session.ItemCenter("row3"));
    CHECK(probe.activated == 3);
    session.KeyPress(ImGuiKey_DownArrow);
    session.KeyPress(ImGuiKey_Enter);
    CHECK(probe.activated == 4);
}

TEST_CASE("A virtual list draws its loading, error and empty states instead of rows and scrolls to a row")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values v;
    VirtualProbe probe;
    probe.count = 10000;
    session.SetUi([&] {
        // The next-scroll request goes to the next list, so the probe comes first.
        probe.Draw(session);
        CHECK_BALANCED(CHECK_FALSE(facet::BeginVirtualList("loading", 10, nullptr, {.state = facet::DataState::Loading, .height = 60.0f})));
        CHECK_BALANCED(CHECK_FALSE(facet::BeginVirtualList("error", 10, nullptr, {.state = facet::DataState::Error, .height = 60.0f})));
        CHECK_BALANCED(CHECK_FALSE(facet::BeginVirtualList("empty", 0, nullptr, {.emptyText = "No rows", .height = 60.0f})));
        CHECK_BALANCED(CHECK_FALSE(facet::BeginVirtualList("negative", -5, nullptr, {.height = 60.0f})));
    });
    session.Frames(2);
    CHECK(probe.scrollY == 0.0f);
    facet::SetNextVirtualListScrollTo(5000);
    session.Frames(2);
    CHECK(probe.scrollY > 0.0f);
    CHECK(probe.lastSubmitted >= 5000);
}

TEST_CASE("Virtual lists leave every stack as they found it")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values v;
    ImGuiSelectionBasicStorage selection;
    session.SetUi([&] {
        ImGui::Button("first");  // the item the window's keyboard focus starts on
        CHECK_BALANCED({
            facet::VirtualListScope list("list", 1000, &selection, {.rowHeight = 20.0f, .height = 100.0f});
            if (list)
            {
                facet::RowRange rows;
                while (facet::NextVisibleRows(&rows))
                    for (int row = rows.first; row < rows.last; ++row)
                    {
                        (void)facet::VirtualListRow(row);
                        ImGui::TextUnformatted("row");
                    }
            }
        });
        ImGui::BeginDisabled();
        CHECK_BALANCED({ facet::VirtualListScope list("disabled", 10, nullptr, {.height = 60.0f}); });
        ImGui::EndDisabled();
    });
    session.Frames(2);
    CHECK(selection.Size == 0);
    CHECK(facet::internal::OpenScopeCount() == 0);
    const int before = facet::internal::UsageErrorCount();
    CHECK_FALSE(facet::BeginVirtualList("outside a frame", 10, nullptr));
    CHECK(facet::internal::UsageErrorCount() == before);
}
// ---- Status bar ---------------------------------------------------------------------------------

TEST_CASE("A status bar lays its segments out from the leading edge and hides the lowest priority first")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float width = 0.0f;  // units; 0 fills
    ImRect bar;
    ImRect segments[3];
    session.SetUi([&] {
        facet::BeginStatusBar("status", {.width = width});
        const char* texts[3] = {"First segment", "Second segment", "Third segment"};
        const int priorities[3] = {1, 0, 2};
        for (int i = 0; i < 3; ++i)
        {
            facet::StatusItem(texts[i], {.tone = i == 0 ? facet::Tone::Success : facet::Tone::Neutral,
                                         .priority = priorities[i]});
            segments[i] = GImGui->LastItemData.Rect;  // the previous item's rect while hidden
        }
        facet::EndStatusBar();
        bar = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    });
    session.Frames(2);
    CHECK(bar.GetHeight() == doctest::Approx(facet::Px(facet::T().containers.statusBarHeight)));
    CHECK(segments[0].Min.x == doctest::Approx(bar.Min.x + facet::Px(facet::T().spacing.md)));
    CHECK(segments[1].Min.x > segments[0].Max.x);
    CHECK(segments[2].Min.x > segments[1].Max.x);
    CHECK(segments[2].Max.x <= bar.Max.x);
    // Narrow: the second (priority 0) goes first, then the first (priority 1).
    const float scale = facet::Px(1.0f);
    const float gap = facet::T().spacing.lg;
    const float pad = facet::T().spacing.md;
    const float w0 = segments[0].GetWidth() / scale;
    const float w2 = segments[2].GetWidth() / scale;
    width = pad * 2.0f + w0 + gap + w2 + 1.0f;
    session.Frames(3);
    CHECK(segments[0].Min.x == doctest::Approx(bar.Min.x + facet::Px(pad)));
    CHECK(segments[2].Min.x == doctest::Approx(segments[0].Max.x + facet::Px(gap)));
    width = pad * 2.0f + w2 + 1.0f;
    session.Frames(3);
    CHECK(segments[2].Min.x == doctest::Approx(bar.Min.x + facet::Px(pad)));
    CHECK(segments[2].Max.x <= bar.Max.x);
}

TEST_CASE("A status segment draws its tone as a dot or a colored icon")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int dots = 0;
    int icons = 0;
    int neutral = 0;
    session.SetUi([&] {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImU32 success = facet::ToU32(facet::T().colors.success);
        const ImU32 warning = facet::ToU32(facet::T().colors.warning);
        facet::BeginStatusBar("status");
        int from = drawList->VtxBuffer.Size;
        facet::StatusItem("Saved", {.tone = facet::Tone::Success});
        dots = CountColor(drawList, from, success);
        from = drawList->VtxBuffer.Size;
        facet::StatusItem("Offline", {.icon = facet::Icon("warning"), .tone = facet::Tone::Warning});
        icons = CountColor(drawList, from, warning);
        from = drawList->VtxBuffer.Size;
        facet::StatusItem("Ready");
        neutral = CountColor(drawList, from, success) + CountColor(drawList, from, warning);
        facet::EndStatusBar();
    });
    session.Frames(2);
    CHECK(dots > 0);
    CHECK(icons > 0);
    CHECK(neutral == 0);
}

TEST_CASE("Status bars leave every stack as they found it")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values v;
    session.SetUi([&] {
        CHECK_BALANCED({
            facet::StatusBarScope bar("bar");
            facet::StatusItem("Item");
            ImGui::TextUnformatted("text");
            facet::StatusItem("After", {.icon = facet::Icon("info")});
        });
        CHECK_BALANCED({
            facet::StatusBarScope narrow("narrow", {.width = 1.0f});
            facet::StatusItem("A segment that cannot fit");
        });
        CHECK_BALANCED({ facet::StatusBarScope empty("empty"); });
        ImGui::BeginDisabled();
        CHECK_BALANCED({
            facet::StatusBarScope disabled("disabled");
            facet::StatusItem("Disabled", {.tone = facet::Tone::Destructive});
        });
        ImGui::EndDisabled();
        CHECK_BALANCED(facet::StatusItem("Outside a bar"));
        const int before = facet::internal::UsageErrorCount();
        facet::EndStatusBar();
        CHECK(facet::internal::UsageErrorCount() == before + 1);
    });
    session.Frames(2);
    CHECK(facet::internal::OpenScopeCount() == 0);
    const int before = facet::internal::UsageErrorCount();
    facet::BeginStatusBar("outside a frame");
    facet::StatusItem("Item");
    facet::EndStatusBar();
    CHECK(facet::internal::OpenScopeCount() == 0);
    CHECK(facet::internal::UsageErrorCount() == before);
}
TEST_CASE("The overflow fitter without a button reserves nothing for the hidden items")
{
    const facet::internal::OverflowItem items[3] = {{100.0f, 1}, {100.0f, 0}, {100.0f, 2}};
    bool visible[3] = {};
    // Two items and their gap fit exactly: the middle one (priority 0) hides, nothing is reserved.
    facet::internal::OverflowResult fit =
        facet::internal::FitOverflowWithoutButton(items, 210.0f, 10.0f, 0.0f, nullptr, visible);
    CHECK(fit.hiddenCount == 1);
    CHECK_FALSE(fit.showButton);
    CHECK(fit.usedWidth == doctest::Approx(210.0f));
    CHECK(visible[0]);
    CHECK_FALSE(visible[1]);
    CHECK(visible[2]);
    // The same row with a button needs room for it, so one more item hides.
    fit = facet::internal::FitOverflow(items, 210.0f, 10.0f, 0.0f, 0.0f, nullptr, visible);
    CHECK(fit.hiddenCount == 2);
    // Everything fits: nothing hides.
    fit = facet::internal::FitOverflowWithoutButton(items, 320.0f, 10.0f, 0.0f, nullptr, visible);
    CHECK(fit.hiddenCount == 0);
    CHECK(fit.usedWidth == doctest::Approx(320.0f));
}