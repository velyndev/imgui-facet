// Selection pill (src/internal/selection_pill.h).

#include <cmath>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "internal/selection_pill.h"
#include "support/session.h"

using facet::internal::SelectionPill;

namespace
{

struct MotionGuard
{
    MotionGuard()
    {
        facet::SetFollowSystemMotion(false);
        facet::SetTheme(facet::Theme::Light(), false);
    }
    ~MotionGuard()
    {
        facet::SetFollowSystemMotion(true);
        facet::SetTheme(facet::Theme::Light(), false);
    }
};

constexpr ImU32 kPillColor = IM_COL32(1, 2, 3, 255);
constexpr ImU32 kLabelColor = IM_COL32(4, 5, 6, 255);

// Three entries in a row, 60 x 24 pixels, 10 apart, relative to `origin`.
ImRect Entry(ImVec2 origin, int index)
{
    const float x = origin.x + index * 70.0f;
    return ImRect(x, origin.y, x + 60.0f, origin.y + 24.0f);
}

struct PillProbe
{
    int selected = 0;
    bool target = true;
    ImVec2 origin = ImVec2(100.0f, 100.0f);
    ImRect drawn;
    bool moving = false;
    int pillVertex = -1;   // first index (in draw order) of a vertex in the pill color
    int labelVertex = -1;  // first index (in draw order) of a vertex in the label color

    void Ui()
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        SelectionPill pill;
        pill.Begin(ImGui::GetID("group"), drawList, origin);
        for (int i = 0; i < 3; ++i)
        {
            const ImRect entry = Entry(origin, i);
            if (target && i == selected)
                pill.Target(entry);
            drawList->AddRectFilled(ImVec2(entry.Min.x + 20.0f, entry.Min.y + 8.0f),
                                    ImVec2(entry.Min.x + 40.0f, entry.Min.y + 16.0f), kLabelColor);
        }
        drawn = pill.End(kPillColor, 6.0f);
        moving = pill.Moving();
        // Draw order is the order of the indices after the channels were merged.
        pillVertex = -1;
        labelVertex = -1;
        int order = 0;
        for (const ImDrawCmd& cmd : drawList->CmdBuffer)
        {
            for (unsigned int i = 0; i < cmd.ElemCount; ++i, ++order)
            {
                const ImDrawVert& vertex = drawList->VtxBuffer[static_cast<int>(cmd.VtxOffset + drawList->IdxBuffer[static_cast<int>(cmd.IdxOffset + i)])];
                if (pillVertex < 0 && vertex.col == kPillColor)
                    pillVertex = order;
                if (labelVertex < 0 && vertex.col == kLabelColor)
                    labelVertex = order;
            }
        }
    }
};

bool Same(const ImRect& a, const ImRect& b)
{
    return std::fabs(a.Min.x - b.Min.x) < 0.01f && std::fabs(a.Max.x - b.Max.x) < 0.01f &&
           std::fabs(a.Min.y - b.Min.y) < 0.01f && std::fabs(a.Max.y - b.Max.y) < 0.01f;
}

} // namespace

TEST_CASE("Selection pill: starts on its target and draws behind the entries")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    PillProbe probe;
    session.SetUi([&] { probe.Ui(); });
    session.Frame();
    CHECK(Same(probe.drawn, Entry(probe.origin, 0)));
    CHECK_FALSE(probe.moving);
    REQUIRE(probe.pillVertex >= 0);
    REQUIRE(probe.labelVertex >= 0);
    CHECK(probe.pillVertex < probe.labelVertex);
}

TEST_CASE("Selection pill: slides to the new entry, stretching on the way, and settles exactly")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    PillProbe probe;
    session.SetUi([&] { probe.Ui(); });
    session.Frames(2);
    probe.selected = 2;
    float widest = 0.0f;
    bool between = false;
    for (int frame = 0; frame < 120; ++frame)
    {
        session.Frame();
        widest = std::max(widest, probe.drawn.GetWidth());
        const float x = probe.drawn.Min.x;
        between = between || (x > Entry(probe.origin, 0).Min.x + 1.0f && x < Entry(probe.origin, 2).Min.x - 1.0f);
    }
    CHECK(between);
    CHECK(widest > 60.0f + 5.0f);  // the leading edge ran ahead
    CHECK(Same(probe.drawn, Entry(probe.origin, 2)));
    CHECK_FALSE(probe.moving);
}

TEST_CASE("Selection pill: the same motion at any frame rate")
{
    MotionGuard guard;
    float x[2] = {};
    const float rates[2] = {1.0f / 30.0f, 1.0f / 120.0f};
    for (int i = 0; i < 2; ++i)
    {
        facet::test::Session session({.deltaTime = rates[i]});
        REQUIRE(session.Ready());
        PillProbe probe;
        session.SetUi([&] { probe.Ui(); });
        session.Frames(2);
        probe.selected = 1;
        session.Frames(static_cast<int>(std::lround(0.2f / rates[i])));
        x[i] = probe.drawn.Min.x;
    }
    CHECK(x[0] == doctest::Approx(x[1]).epsilon(0.001));
}

TEST_CASE("Selection pill: scrolling moves it with its entry, reduced motion snaps it")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    PillProbe probe;
    session.SetUi([&] { probe.Ui(); });
    session.Frames(2);
    // The group scrolls: origin and entries move together, the pill follows at once.
    probe.origin = ImVec2(100.0f, 40.0f);
    session.Frame();
    CHECK(Same(probe.drawn, Entry(probe.origin, 0)));

    facet::Theme theme = facet::Theme::Light();
    theme.motion.reduceMotion = true;
    facet::SetTheme(theme, false);
    probe.selected = 2;
    session.Frame();
    CHECK(Same(probe.drawn, Entry(probe.origin, 2)));
}

TEST_CASE("Selection pill: no target draws nothing, and a returning target starts in place")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    PillProbe probe;
    session.SetUi([&] { probe.Ui(); });
    session.Frames(2);
    probe.target = false;
    session.Frame();
    CHECK(probe.pillVertex < 0);
    CHECK(probe.drawn.GetWidth() == 0.0f);
    probe.target = true;
    probe.selected = 2;
    session.Frame();
    CHECK(Same(probe.drawn, Entry(probe.origin, 2)));
}

TEST_CASE("Selection pill: the selected label is semibold, the others keep their weight")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ImFont* selectedFont = nullptr;
    ImFont* otherFont = nullptr;
    ImFont* headlineFont = nullptr;
    float selectedSize = 0.0f;
    session.SetUi([&] {
        facet::internal::PushPillLabelStyle(facet::TextStyle::Body, true);
        selectedFont = ImGui::GetFont();
        selectedSize = ImGui::GetFontSize();
        facet::PopTextStyle();
        facet::internal::PushPillLabelStyle(facet::TextStyle::Body, false);
        otherFont = ImGui::GetFont();
        facet::PopTextStyle();
        // A style that is already semibold stays so either way.
        facet::internal::PushPillLabelStyle(facet::TextStyle::Headline, false);
        headlineFont = ImGui::GetFont();
        facet::PopTextStyle();
    });
    session.Frame();
    REQUIRE(facet::WeightFont(facet::FontWeight::Semibold) != nullptr);
    CHECK(selectedFont == facet::WeightFont(facet::FontWeight::Semibold));
    CHECK(otherFont == facet::WeightFont(facet::FontWeight::Regular));
    CHECK(headlineFont == facet::WeightFont(facet::FontWeight::Semibold));
    CHECK(selectedFont != otherFont);
    CHECK(selectedSize == doctest::Approx(facet::Px(facet::T().type[facet::TextStyle::Body].size)));
}