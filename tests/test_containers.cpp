// Behaviour of the containers family (facet/widgets/containers.h).
//
// One test case per behaviour of an implemented widget; docs/writing-a-widget.md describes the
// pattern. The API checks, and the placeholder checks of the widgets that are still planned, are in
// tests/test_api_containers.cpp.

#include <cfloat>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "internal/focus.h"
#include "internal/popup_host.h"
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
