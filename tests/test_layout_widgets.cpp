// Behaviour of the layout family (facet/widgets/layout.h). The core width classes and the labelled
// row layout of facet/core/layout.h are tested in tests/test_layout.cpp.
//
// One test case per behaviour of an implemented widget; docs/writing-a-widget.md describes the
// pattern. The API checks, and the placeholder checks of the widgets that are still planned, are in
// tests/test_api_layout.cpp.

#include <cfloat>
#include <cmath>
#include <cstdio>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "internal/row_layout.h"
#include "internal/state_storage.h"
#include "widgets/layout/layout_internal.h"
#include "support/session.h"
#include "support/structure_stub_check.h"

namespace
{

using namespace facet::test::structure;
namespace fi = facet::internal;

// Restores the theme a test changes.
struct ThemeGuard
{
    facet::Theme saved = facet::T();
    ~ThemeGuard() { facet::SetTheme(saved, false); }
};

// Turns animations off for the guard's lifetime, so values reach their targets at once.
struct ReducedMotion : ThemeGuard
{
    ReducedMotion()
    {
        facet::Theme theme = facet::T();
        theme.motion.reduceMotion = true;
        facet::SetTheme(theme, false);
    }
};

// Restores the UI scale a test changes.
struct ScaleGuard
{
    float saved = facet::UiScale();
    ~ScaleGuard() { facet::SetUiScale(saved); }
};

// A child region `width` pixels wide (and tall enough for a few rows) around `body`.
template <class Body>
void InRegion(const char* id, float width, Body&& body, float height = 0.0f)
{
    const float h = height > 0.0f ? height : facet::Px(facet::T().metrics.controlHeightLarge * 10.0f);
    ImGui::BeginChild(id, ImVec2(width, h));
    body();
    ImGui::EndChild();
}

} // namespace

// ---- SettingsRow --------------------------------------------------------------------------------

TEST_CASE("Settings rows put their controls at one x whatever the label lengths")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    fi::RowLayout shortRow;
    fi::RowLayout longRow;
    session.SetUi([&] {
        facet::BeginSettingsRow("A");
        ImGui::Button("##value", ImVec2(-FLT_MIN, 0.0f));
        session.Track("short");
        facet::EndSettingsRow();
        shortRow = fi::LastSettingsRowLayout();
        facet::BeginSettingsRow("A label much longer than the label column of every settings row");
        ImGui::Button("##value", ImVec2(-FLT_MIN, 0.0f));
        session.Track("long");
        facet::EndSettingsRow();
        longRow = fi::LastSettingsRowLayout();
    });
    session.Frames(2);
    CHECK_FALSE(shortRow.stacked);
    CHECK_FALSE(longRow.stacked);
    CHECK(session.ItemRect("short").Min.x == doctest::Approx(session.ItemRect("long").Min.x));
    CHECK(shortRow.control.Min.x == doctest::Approx(longRow.control.Min.x));
    // The long label is cut at its column and never reaches the control.
    CHECK(longRow.truncated);
    CHECK_FALSE(shortRow.truncated);
    CHECK(longRow.label.Max.x < longRow.control.Min.x);
    // The label column is the token: the control starts there.
    const float column = facet::Px(facet::T().layout.labelColumn);
    CHECK(session.ItemRect("short").Min.x - shortRow.label.Min.x == doctest::Approx(column).epsilon(0.01));
}

TEST_CASE("A settings row stacks its control under the label in a Compact width")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    fi::RowLayout narrow;
    fi::RowLayout wide;
    float narrowWidth = 0.0f;
    session.SetUi([&] {
        const facet::Theme& theme = facet::T();
        InRegion("narrow", facet::Px(theme.layout.labelColumn + theme.layout.minRowControl * 0.5f), [&] {
            narrowWidth = ImGui::GetContentRegionAvail().x;
            facet::BeginSettingsRow("Label", {.description = "A description under the label"});
            ImGui::Button("##value", ImVec2(-FLT_MIN, 0.0f));
            session.Track("narrow");
            facet::EndSettingsRow();
            narrow = fi::LastSettingsRowLayout();
        });
        InRegion("wide", facet::Px(theme.layout.regularFrom + 40.0f), [&] {
            facet::BeginSettingsRow("Label", {.description = "A description under the label"});
            ImGui::Button("##value", ImVec2(-FLT_MIN, 0.0f));
            session.Track("wide");
            facet::EndSettingsRow();
            wide = fi::LastSettingsRowLayout();
        });
    });
    session.Frames(2);
    CHECK(narrow.stacked);
    CHECK_FALSE(wide.stacked);
    // Stacked: the control is under the label and takes the whole width.
    CHECK(session.ItemRect("narrow").Min.y > narrow.label.Max.y);
    CHECK(session.ItemRect("narrow").GetWidth() == doctest::Approx(narrowWidth));
    // Beside: label and control share the line.
    CHECK(wide.label.Min.y < session.ItemRect("wide").Max.y);
    CHECK(wide.label.Max.y > session.ItemRect("wide").Min.y);
}

TEST_CASE("A column of settings rows stacks at one width when one row has a reset button")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    fi::RowLayout plain;
    fi::RowLayout resettable;
    session.SetUi([&] {
        const facet::Theme& theme = facet::T();
        // Wide enough for a plain row beside its label, too narrow once a reset slot is taken off.
        const float slot = facet::ControlHeight(facet::ControlSize::Regular) + facet::Px(theme.spacing.sm);
        const float width = facet::Px(theme.layout.labelColumn + theme.layout.minRowControl) + slot * 0.5f;
        InRegion("column", width, [&] {
            facet::BeginSettingsRow("Plain");
            ImGui::Button("##value", ImVec2(-FLT_MIN, 0.0f));
            session.Track("plain");
            facet::EndSettingsRow();
            plain = fi::LastSettingsRowLayout();
            facet::BeginSettingsRow("Resettable", {.modified = true, .resetButton = true});
            ImGui::Button("##value", ImVec2(-FLT_MIN, 0.0f));
            session.Track("resettable");
            facet::EndSettingsRow();
            resettable = fi::LastSettingsRowLayout();
        });
    });
    session.Frames(3);
    CHECK(resettable.stacked);
    CHECK(plain.stacked == resettable.stacked);
    CHECK(session.ItemRect("plain").Min.x == doctest::Approx(session.ItemRect("resettable").Min.x));
    CHECK(session.ItemRect("plain").Max.x == doctest::Approx(session.ItemRect("resettable").Max.x));
}

TEST_CASE("A settings row lines its label up with the text of a control of any size")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    const facet::ControlSize sizes[] = {facet::ControlSize::Small, facet::ControlSize::Regular, facet::ControlSize::Large};
    fi::RowLayout rows[3];
    float buttonHeight[3] = {};
    float textTop[3] = {};
    session.SetUi([&] {
        for (int i = 0; i < 3; ++i)
        {
            ImGui::PushID(i);
            facet::BeginSettingsRow("Label");
            facet::Button("Apply", {.size = sizes[i]});
            // The button's text sits where CenteredTextY puts it in the button's text style.
            const ImVec2 min = ImGui::GetItemRectMin();
            const ImVec2 max = ImGui::GetItemRectMax();
            buttonHeight[i] = max.y - min.y;
            facet::PushTextStyle(facet::ControlTextStyle(sizes[i]));
            textTop[i] = facet::CenteredTextY(min.y, max.y - min.y);
            facet::PopTextStyle();
            facet::EndSettingsRow();
            rows[i] = fi::LastSettingsRowLayout();
            ImGui::PopID();
        }
    });
    session.Frames(3);
    for (int i = 0; i < 3; ++i)
    {
        CAPTURE(i);
        CHECK(buttonHeight[i] == doctest::Approx(facet::ControlHeight(sizes[i])));
        CHECK(rows[i].label.Min.y == doctest::Approx(textTop[i]));
    }
}

TEST_CASE("Settings rows push their label, so every control can use the same id")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    session.SetUi([&] {
        facet::BeginSettingsRow("First");
        ImGui::Button("##value");
        session.Track("first");
        facet::EndSettingsRow();
        facet::BeginSettingsRow("Second");
        ImGui::Button("##value");
        session.Track("second");
        facet::EndSettingsRow();
    });
    session.Frame();
    CHECK(session.ItemId("first") != 0);
    CHECK(session.ItemId("first") != session.ItemId("second"));
}

TEST_CASE("The reset button of a modified settings row is reported by SettingsRowResetPressed")
{
    ReducedMotion motion;
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool modified = true;
    int resets = 0;
    fi::RowLayout layout;
    session.SetUi([&] {
        facet::BeginSettingsRow("Strength", {.modified = modified, .resetButton = true});
        ImGui::Button("##value", ImVec2(-FLT_MIN, 0.0f));
        session.Track("control");
        facet::EndSettingsRow();
        layout = fi::LastSettingsRowLayout();
        if (facet::SettingsRowResetPressed())
            ++resets;
        // A row after it without a reset button: the query answers for the last row only.
        facet::BeginSettingsRow("Other");
        facet::EndSettingsRow();
        CHECK_FALSE(facet::SettingsRowResetPressed());
    });
    session.Frames(2);
    const float gap = facet::Px(facet::T().spacing.sm);
    const float size = layout.control.GetHeight();
    const ImVec2 reset(layout.control.Max.x + gap + size * 0.5f, layout.control.Min.y + size * 0.5f);
    // The control leaves room for the button at the end of the row.
    CHECK(session.ItemRect("control").Max.x <= layout.control.Max.x + 0.5f);
    session.Click(reset);
    CHECK(resets == 1);
    // Not modified: the button is gone, the slot stays.
    modified = false;
    session.Frames(2);
    session.Click(reset);
    CHECK(resets == 1);
}

TEST_CASE("Settings rows leave every stack balanced, also nested, disabled and outside a frame")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values v;
    const int errorsBefore = fi::UsageErrorCount();
    session.SetUi([&] {
        CHECK_BALANCED({
            facet::BeginSettingsRow("Outer", {.description = "Text", .icon = facet::Icon("layers"), .modified = true,
                                              .resetButton = true});
            facet::BeginSettingsRow("Inner");
            ImGui::TextUnformatted("control");
            facet::EndSettingsRow();
            facet::EndSettingsRow();
        });
        ImGui::BeginDisabled();
        CHECK_BALANCED({
            facet::SettingsRowScope row("Disabled");
            ImGui::Button("##value");
        });
        ImGui::EndDisabled();
        CHECK_BALANCED({
            facet::BeginSettingsRow(nullptr);
            facet::EndSettingsRow();
        });
    });
    session.Frames(2);
    CHECK(fi::UsageErrorCount() == errorsBefore);
    CHECK(fi::OpenScopeCount() == 0);
    // Outside a frame nothing is drawn and the scope still balances.
    facet::BeginSettingsRow("outside");
    facet::EndSettingsRow();
    CHECK(fi::OpenScopeCount() == 0);
    CHECK_FALSE(facet::SettingsRowResetPressed());
}

// ---- Stack --------------------------------------------------------------------------------------

TEST_CASE("A vertical stack puts its gap between the items and fills its width with Stretch")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    const float width = 200.0f;
    session.SetUi([&] {
        facet::BeginStack("column", {.gap = facet::Gap::Loose, .align = facet::LayoutAlign::Stretch, .width = width});
        ImGui::Button("first");
        session.Track("first");
        facet::Button("second", {.width = facet::kFill});
        session.Track("second");
        ImGui::Button("third", ImVec2(-FLT_MIN, 0.0f));
        session.Track("third");
        facet::EndStack();
        session.Track("stack");
        ImGui::Button("after");
        session.Track("after");
    });
    session.Frames(2);
    const float gap = facet::Px(facet::T().spacing.lg);
    CHECK(session.ItemRect("second").Min.y - session.ItemRect("first").Max.y == doctest::Approx(gap));
    CHECK(session.ItemRect("third").Min.y - session.ItemRect("second").Max.y == doctest::Approx(gap));
    // Fields and fill-width items take the stack width, not the window's.
    CHECK(session.ItemRect("second").GetWidth() == doctest::Approx(facet::Px(width)));
    CHECK(session.ItemRect("third").GetWidth() == doctest::Approx(facet::Px(width)));
    // The stack is the last item and covers its items; the window's spacing follows it.
    const ImRect stack = session.ItemRect("stack");
    CHECK(stack.GetWidth() == doctest::Approx(facet::Px(width)));
    CHECK(stack.Max.y == doctest::Approx(session.ItemRect("third").Max.y));
    CHECK(session.ItemRect("after").Min.y - stack.Max.y == doctest::Approx(ImGui::GetStyle().ItemSpacing.y));
}

TEST_CASE("A horizontal stack lines its items up, and a spacer pushes the rest to the end")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    const float width = 400.0f;
    session.SetUi([&] {
        facet::BeginStack("row", {.orientation = facet::Orientation::Horizontal, .align = facet::LayoutAlign::Center,
                                  .width = width});
        facet::Button("small", {.size = facet::ControlSize::Small});
        session.Track("small");
        facet::Button("next", {.size = facet::ControlSize::Small});
        session.Track("next");
        facet::StackSpacer();
        facet::Button("large", {.size = facet::ControlSize::Large});
        session.Track("large");
        facet::EndStack();
        session.Track("stack");
    });
    session.Frames(3);
    const ImRect smallButton = session.ItemRect("small");
    const ImRect next = session.ItemRect("next");
    const ImRect large = session.ItemRect("large");
    const ImRect stack = session.ItemRect("stack");
    const float gap = facet::Px(facet::T().spacing.sm);
    // One line, the gap between neighbours.
    CHECK(next.Min.x - smallButton.Max.x == doctest::Approx(gap));
    CHECK(next.Min.y == doctest::Approx(smallButton.Min.y));
    // The spacer takes the free space: the item after it ends at the stack's end.
    CHECK(large.Max.x == doctest::Approx(stack.Min.x + facet::Px(width)).epsilon(0.001));
    // Center: the small run sits in the middle of the line the large button sets.
    CHECK(stack.GetHeight() == doctest::Approx(large.GetHeight()));
    CHECK((smallButton.Min.y + smallButton.Max.y) * 0.5f == doctest::Approx((large.Min.y + large.Max.y) * 0.5f).epsilon(0.01));
}

TEST_CASE("A horizontal stack that fits its items reports its own size and restores the layout")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    session.SetUi([&] {
        facet::BeginStack("row", {.orientation = facet::Orientation::Horizontal, .gap = facet::Gap::None});
        ImGui::Button("a", ImVec2(40.0f, 20.0f));
        session.Track("a");
        ImGui::Button("b", ImVec2(30.0f, 20.0f));
        session.Track("b");
        facet::EndStack();
        session.Track("stack");
        ImGui::Button("below");
        session.Track("below");
    });
    session.Frames(2);
    CHECK(session.ItemRect("b").Min.x == doctest::Approx(session.ItemRect("a").Max.x));
    CHECK(session.ItemRect("stack").GetWidth() == doctest::Approx(70.0f));
    // The window's own layout is vertical again after the stack.
    CHECK(session.ItemRect("below").Min.y > session.ItemRect("stack").Max.y);
    CHECK(session.ItemRect("below").Min.x == doctest::Approx(session.ItemRect("stack").Min.x));
}

TEST_CASE("Stacks nest, leave every stack balanced and report a stray spacer")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values v;
    int reported = 0;
    session.SetUi([&] {
        CHECK_BALANCED({
            facet::BeginStack("outer", {.orientation = facet::Orientation::Horizontal});
            facet::BeginStack("inner", {.width = 80.0f});
            ImGui::TextUnformatted("one");
            ImGui::TextUnformatted("two");
            facet::EndStack();
            facet::StackSpacer();
            ImGui::TextUnformatted("three");
            facet::EndStack();
        });
        ImGui::BeginDisabled();
        CHECK_BALANCED({
            facet::StackScope stack("disabled", {.gap = facet::Gap::Section});
            ImGui::Button("inside");
        });
        ImGui::EndDisabled();
        CHECK_BALANCED({
            facet::BeginStack("empty");
            facet::EndStack();
        });
        const int before = fi::UsageErrorCount();
        facet::StackSpacer();
        reported = fi::UsageErrorCount() - before;
    });
    session.Frames(2);
    CHECK(reported == 1);
    CHECK(fi::OpenScopeCount() == 0);
}

// ---- Grid ---------------------------------------------------------------------------------------

TEST_CASE("A grid fits as many columns of the minimum width as it can and fills them")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float width = 600.0f;
    ImRect cells[5];
    session.SetUi([&] {
        facet::BeginGrid("grid", {.minCellWidth = 128.0f, .width = width});
        for (int i = 0; i < 5; ++i)
        {
            facet::GridNext();
            ImGui::Button("##cell", ImVec2(-FLT_MIN, 20.0f + 10.0f * static_cast<float>(i)));
            cells[i] = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        }
        facet::EndGrid();
        session.Track("grid");
    });
    session.Frames(2);
    const float gap = facet::Px(facet::T().spacing.sm);
    // (600 + 8) / (128 + 8): four columns.
    const float cell = std::floor((facet::Px(width) - gap * 3.0f) / 4.0f);
    CHECK(cells[0].GetWidth() == doctest::Approx(cell));
    CHECK(cells[1].Min.x - cells[0].Max.x == doctest::Approx(gap));
    CHECK(cells[3].Min.y == doctest::Approx(cells[0].Min.y));
    // The fifth cell starts the next row under the tallest cell of the first.
    CHECK(cells[4].Min.x == doctest::Approx(cells[0].Min.x));
    CHECK(cells[4].Min.y == doctest::Approx(cells[3].Max.y + gap));
    CHECK(session.ItemRect("grid").Max.y == doctest::Approx(cells[4].Max.y));
    // Narrower: fewer, wider columns.
    width = 300.0f;
    session.Frames(2);
    CHECK(cells[1].Min.y == doctest::Approx(cells[0].Min.y));
    CHECK(cells[2].Min.y > cells[0].Max.y);
}

TEST_CASE("A grid cell spans columns and moves to the next row when the span does not fit")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ImRect cells[4];
    session.SetUi([&] {
        facet::BeginGrid("grid", {.minCellWidth = 100.0f, .maxColumns = 3, .width = 500.0f});
        const int spans[] = {2, 2, 1, 9};
        for (int i = 0; i < 4; ++i)
        {
            facet::GridNext({.columnSpan = spans[i]});
            ImGui::Button("##cell", ImVec2(-FLT_MIN, 20.0f));
            cells[i] = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        }
        facet::EndGrid();
    });
    session.Frames(2);
    const float gap = facet::Px(facet::T().spacing.sm);
    const float cell = std::floor((facet::Px(500.0f) - gap * 2.0f) / 3.0f);
    CHECK(cells[0].GetWidth() == doctest::Approx(cell * 2.0f + gap));
    // Two columns do not fit after the first span: the second starts a row.
    CHECK(cells[1].Min.y > cells[0].Max.y);
    CHECK(cells[2].Min.y == doctest::Approx(cells[1].Min.y));
    // A span past the column count is clamped to it.
    CHECK(cells[3].GetWidth() == doctest::Approx(cell * 3.0f + gap * 2.0f));
}

TEST_CASE("A grid holds at a width of a few pixels, with no cell and with one, at every scale")
{
    ScaleGuard scale;
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values v;
    ImRect cells[3];
    int count = 3;
    float width = 3.0f;
    session.SetUi([&] {
        CHECK_BALANCED({
            facet::BeginGrid("grid", {.width = width});
            for (int i = 0; i < count; ++i)
            {
                facet::GridNext();
                ImGui::Button("##cell", ImVec2(-FLT_MIN, 10.0f));
                cells[i] = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
            }
            facet::EndGrid();
            session.Track("grid");
        });
    });
    for (const float uiScale : {0.75f, 1.0f, 2.0f})
    {
        CAPTURE(uiScale);
        facet::SetUiScale(uiScale);
        width = 3.0f;
        count = 3;
        session.Frames(2);
        // One column, one cell per row, none overlapping.
        CHECK(cells[0].GetWidth() >= 1.0f);
        CHECK(cells[1].Min.y > cells[0].Max.y);
        CHECK(cells[2].Min.y > cells[1].Max.y);
        CHECK(cells[0].Min.x == doctest::Approx(cells[1].Min.x));
        count = 1;
        session.Frames(1);
        CHECK(session.ItemRect("grid").GetHeight() == doctest::Approx(10.0f));
        count = 0;
        session.Frames(1);
        CHECK(session.ItemRect("grid").GetHeight() == doctest::Approx(0.0f));
        // The column count follows the width in units: two minimum cells fit at every scale.
        width = facet::T().layout.minRowControl * 2.0f + facet::T().spacing.sm * 1.5f;
        count = 2;
        session.Frames(1);
        CHECK(cells[1].Min.y == doctest::Approx(cells[0].Min.y));
    }
}

TEST_CASE("A stray GridNext is reported and grids nest in their cells")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values v;
    int reported = 0;
    session.SetUi([&] {
        CHECK_BALANCED({
            facet::GridScope grid("outer", {.maxColumns = 2});
            facet::GridNext();
            {
                facet::GridScope inner("inner", {.minCellWidth = 40.0f});
                facet::GridNext();
                ImGui::TextUnformatted("inner cell");
            }
            facet::GridNext();
            ImGui::TextUnformatted("outer cell");
        });
        const int before = fi::UsageErrorCount();
        facet::GridNext();
        reported = fi::UsageErrorCount() - before;
    });
    session.Frames(2);
    CHECK(reported == 1);
}

// ---- Separator ----------------------------------------------------------------------------------

TEST_CASE("A separator is a hairline across the width, or as tall as the line when vertical")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float available = 0.0f;
    session.SetUi([&] {
        available = ImGui::GetContentRegionAvail().x;
        facet::Separator();
        session.Track("horizontal");
        facet::Button("Line", {.size = facet::ControlSize::Large});
        session.Track("button");
        ImGui::SameLine();
        facet::Separator({.orientation = facet::Orientation::Vertical, .opaque = true});
        session.Track("vertical");
        ImGui::SameLine();
        ImGui::Button("next");
        session.Track("next");
    });
    session.Frames(2);
    const float hairline = facet::HairlineThickness();
    CHECK(session.ItemRect("horizontal").GetWidth() == doctest::Approx(available));
    CHECK(session.ItemRect("horizontal").GetHeight() == doctest::Approx(hairline));
    CHECK(session.ItemRect("vertical").GetHeight() == doctest::Approx(session.ItemRect("button").GetHeight()));
    CHECK(session.ItemRect("vertical").Min.y == doctest::Approx(session.ItemRect("button").Min.y));
    CHECK(session.ItemRect("next").Min.x > session.ItemRect("vertical").Max.x);
    // Not interactive: no id, no focus.
    CHECK(session.ItemId("horizontal") == 0);
}

TEST_CASE("A separator text is one footnote line, cut to the width, and takes no keyboard focus")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float line = 0.0f;
    session.SetUi([&] {
        facet::PushTextStyle(facet::TextStyle::Footnote);
        line = ImGui::GetTextLineHeight();
        facet::PopTextStyle();
        ImGui::Button("before");
        session.Track("before");
        facet::SeparatorText("Section");
        session.Track("text");
        facet::SeparatorText("Centered section", {.centered = true});
        InRegion("narrow", 40.0f, [&] {
            facet::SeparatorText("A section title much longer than the region");
            session.Track("narrow");
        }, 40.0f);
        ImGui::Button("after");
        session.Track("after");
    });
    session.Frames(2);
    CHECK(session.ItemRect("text").GetHeight() == doctest::Approx(line));
    CHECK(session.ItemRect("narrow").GetWidth() == doctest::Approx(40.0f));
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("after"));
}

// ---- SplitView ----------------------------------------------------------------------------------

namespace
{

// A split view with a button in each pane. Every test uses its own id: the remembered ratios are
// persistent state, which outlives a session in this process.
struct SplitUi
{
    const char* id = "split";
    float ratio = 0.5f;
    facet::SplitViewOptions options{.minFirst = 100.0f, .minSecond = 100.0f, .width = 600.0f, .height = 200.0f};
    bool callNext = true;
    ImRect view;
    int firstPresses = 0;
    int secondPresses = 0;

    void Ui(facet::test::Session& session)
    {
        ImGui::Button("before");
        session.Track("before");
        facet::BeginSplitView(id, &ratio, options);
        if (ImGui::Button("first", ImVec2(-FLT_MIN, 0.0f)))
            ++firstPresses;
        session.Track("first");
        if (callNext)
        {
            facet::SplitViewNext();
            if (ImGui::Button("second", ImVec2(-FLT_MIN, 0.0f)))
                ++secondPresses;
            session.Track("second");
        }
        facet::EndSplitView();
        view = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        ImGui::Button("after");
        session.Track("after");
    }

    float Band() const { return facet::Px(facet::T().layout.dividerBand); }
    float Room() const { return view.GetWidth() - Band(); }
    // The divider's center for the current ratio (side by side, no clamping in play).
    ImVec2 DividerAt(float r) const
    {
        return ImVec2(view.Min.x + r * Room() + Band() * 0.5f, view.GetCenter().y);
    }
};

} // namespace

TEST_CASE("A split view gives its panes the ratio and a drag of the divider writes it within the minimums")
{
    ReducedMotion motion;
    facet::test::Session session;
    REQUIRE(session.Ready());
    SplitUi ui;
    ui.id = "split drag";
    session.SetUi([&] { ui.Ui(session); });
    session.Frames(2);
    CHECK(ui.view.GetWidth() == doctest::Approx(600.0f));
    CHECK(ui.view.GetHeight() == doctest::Approx(200.0f));
    // The first pane ends at the divider.
    CHECK(session.ItemRect("first").Max.x <= ui.DividerAt(0.5f).x);
    CHECK(session.ItemRect("second").Min.x >= ui.DividerAt(0.5f).x);
    const ImVec2 from = ui.DividerAt(0.5f);
    session.Drag({from, ImVec2(from.x + 60.0f, from.y)});
    CHECK(ui.ratio == doctest::Approx(0.5f + 60.0f / ui.Room()).epsilon(0.01));
    // Far past the end: the second pane keeps its minimum.
    session.Drag({ui.DividerAt(ui.ratio), ImVec2(ui.view.Max.x + 100.0f, from.y)});
    CHECK(ui.ratio * ui.Room() == doctest::Approx(ui.Room() - 100.0f).epsilon(0.01));
    // The panes keep working.
    session.Click(session.ItemCenter("second"));
    CHECK(ui.secondPresses == 1);
}

TEST_CASE("A collapsible split view collapses a pane past half its minimum, and Escape cancels a drag")
{
    ReducedMotion motion;
    facet::test::Session session;
    REQUIRE(session.Ready());
    SplitUi ui;
    ui.id = "split collapse";
    ui.options.collapsible = true;
    session.SetUi([&] { ui.Ui(session); });
    session.Frames(2);
    const ImVec2 from = ui.DividerAt(0.5f);
    session.Drag({from, ImVec2(ui.view.Min.x + 20.0f, from.y)});
    CHECK(ui.ratio == 0.0f);
    // The collapsed pane takes no input: its button is not drawn where the band now is.
    session.Drag({ui.DividerAt(0.0f), ImVec2(ui.view.Max.x - 10.0f, from.y)});
    CHECK(ui.ratio == 1.0f);
    // Escape during a drag puts the ratio from its start back.
    ui.ratio = 0.5f;
    session.Frames(2);
    session.MouseMove(ui.DividerAt(0.5f));
    session.MouseDown();
    session.MouseMove(ImVec2(ui.DividerAt(0.5f).x + 80.0f, from.y));
    CHECK(ui.ratio > 0.55f);
    session.KeyPress(ImGuiKey_Escape);
    CHECK(ui.ratio == doctest::Approx(0.5f));
    session.MouseUp();
    CHECK(ui.ratio == doctest::Approx(0.5f));
}

TEST_CASE("The divider of a split view is a Tab stop between the panes and moves with the keys")
{
    ReducedMotion motion;
    facet::test::Session session;
    REQUIRE(session.Ready());
    SplitUi ui;
    ui.id = "split keys";
    ui.options.collapsible = true;
    session.SetUi([&] { ui.Ui(session); });
    session.Frames(2);
    session.Click(session.ItemCenter("first"));
    REQUIRE(ui.firstPresses == 1);
    session.KeyPress(ImGuiKey_Tab);   // shows the focus on the first button
    session.KeyPress(ImGuiKey_Tab);   // to the divider
    CHECK(GImGui->NavId != session.ItemId("first"));
    CHECK(GImGui->NavId != session.ItemId("second"));
    const float step = facet::Px(facet::T().spacing.lg);
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(ui.ratio * ui.Room() == doctest::Approx(0.5f * ui.Room() + step).epsilon(0.01));
    session.KeyPress(ImGuiKey_LeftArrow | ImGuiMod_Shift);
    CHECK(ui.ratio * ui.Room() == doctest::Approx(0.5f * ui.Room() + step - facet::Px(facet::T().spacing.xs)).epsilon(0.01));
    session.KeyPress(ImGuiKey_Home);
    CHECK(ui.ratio * ui.Room() == doctest::Approx(100.0f).epsilon(0.01));
    session.KeyPress(ImGuiKey_End);
    CHECK(ui.ratio * ui.Room() == doctest::Approx(ui.Room() - 100.0f).epsilon(0.01));
    // Enter collapses the first pane, and brings it back to the ratio it had.
    const float before = ui.ratio;
    session.KeyPress(ImGuiKey_Enter);
    CHECK(ui.ratio == 0.0f);
    session.KeyPress(ImGuiKey_Enter);
    CHECK(ui.ratio == doctest::Approx(before));
    // Tab goes on into the second pane.
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("second"));
}

TEST_CASE("A double click on the divider resets the split view to the ratio it first had")
{
    ReducedMotion motion;
    facet::test::Session session;
    REQUIRE(session.Ready());
    SplitUi ui;
    ui.id = "split reset";
    ui.ratio = 0.4f;
    session.SetUi([&] { ui.Ui(session); });
    session.Frames(2);
    const ImVec2 from = ui.DividerAt(0.4f);
    session.Drag({from, ImVec2(from.x + 90.0f, from.y)});
    REQUIRE(ui.ratio > 0.5f);
    session.DoubleClick(ui.DividerAt(ui.ratio));
    CHECK(ui.ratio == doctest::Approx(0.4f));
}

TEST_CASE("A split view keeps its ratio across a resize and holds a parent smaller than both minimums")
{
    ReducedMotion motion;
    facet::test::Session session;
    REQUIRE(session.Ready());
    SplitUi ui;
    ui.id = "split resize";
    ui.ratio = 0.3f;
    session.SetUi([&] { ui.Ui(session); });
    session.Frames(2);
    CHECK(session.ItemRect("second").Min.x - ui.view.Min.x == doctest::Approx(0.3f * ui.Room() + ui.Band()).epsilon(0.02));
    ui.options.width = 760.0f;
    session.Frames(2);
    CHECK(session.ItemRect("second").Min.x - ui.view.Min.x == doctest::Approx(0.3f * ui.Room() + ui.Band()).epsilon(0.02));
    CHECK(ui.ratio == 0.3f);
    // Smaller than both minimums: the panes share the room by their minimums and nothing overlaps.
    ui.options.width = 120.0f;
    ui.options.orientation = facet::Orientation::Vertical;
    ui.options.height = 120.0f;
    session.Frames(2);
    CHECK(session.ItemRect("first").Max.y <= session.ItemRect("second").Min.y);
    CHECK(session.ItemRect("second").Max.y <= ui.view.Max.y + 0.5f);
    const ImVec2 band(ui.view.GetCenter().x, ui.view.Min.y + (ui.view.GetHeight() - ui.Band()) * 0.5f + ui.Band() * 0.5f);
    session.Drag({band, ImVec2(band.x, band.y + 30.0f)});
    CHECK(ui.ratio == 0.3f);
}

TEST_CASE("A split view stacks its panes in a Compact width")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    SplitUi ui;
    ui.id = "split compact";
    ui.options.width = facet::T().layout.regularFrom * 0.75f;
    ui.options.height = 300.0f;
    session.SetUi([&] { ui.Ui(session); });
    session.Frames(2);
    CHECK(session.ItemRect("second").Min.y > session.ItemRect("first").Max.y);
    CHECK(session.ItemRect("second").Min.x == doctest::Approx(session.ItemRect("first").Min.x));
}

TEST_CASE("A split view writes its saved ratio back once in a later run")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    SplitUi ui;
    ui.id = "split restart";
    ImGuiID id = 0;
    session.SetUi([&] { id = ImGui::GetID(ui.id); });
    session.Frame();
    // What an ini file of an earlier run holds.
    char record[64];
    std::snprintf(record, sizeof(record), "SplitView,0x%08X", id);
    fi::StateStore& store = fi::GlobalStateStore();
    REQUIRE(store.BeginRecord(record));
    store.RecordLine("ratio=0.7000");
    session.SetUi([&] { ui.Ui(session); });
    session.Frame();
    CHECK(ui.ratio == doctest::Approx(0.7f));
    // Only once: the caller's later writes stand.
    ui.ratio = 0.2f;
    session.Frames(2);
    CHECK(ui.ratio == doctest::Approx(0.2f));
}

TEST_CASE("Split views leave every stack balanced, nested, disabled, with one pane and outside a frame")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values v;
    const int errorsBefore = fi::UsageErrorCount();
    float inner = 0.5f;
    session.SetUi([&] {
        CHECK_BALANCED({
            facet::BeginSplitView("outer", &v.ratio, {.height = 200.0f});
            ImGui::TextUnformatted("first");
            facet::SplitViewNext();
            facet::BeginSplitView("inner", &inner, {.orientation = facet::Orientation::Vertical});
            ImGui::TextUnformatted("inner first");
            facet::SplitViewNext();
            ImGui::TextUnformatted("inner second");
            facet::EndSplitView();
            facet::EndSplitView();
        });
        ImGui::BeginDisabled();
        CHECK_BALANCED({
            facet::SplitViewScope split("disabled", &v.ratio, {.height = 100.0f});
            ImGui::Button("inside");
        });
        ImGui::EndDisabled();
        CHECK_BALANCED({
            facet::BeginSplitView("null", nullptr, {.height = 100.0f});
            facet::EndSplitView();
        });
    });
    session.Frames(2);
    CHECK(fi::UsageErrorCount() == errorsBefore);
    CHECK(fi::OpenScopeCount() == 0);
    facet::BeginSplitView("outside", &inner);
    facet::SplitViewNext();
    facet::EndSplitView();
    CHECK(fi::OpenScopeCount() == 0);
}

// ---- DependentGroup -----------------------------------------------------------------------------

namespace
{

// A parent toggle, a dependent group with a button, and a button after it.
struct DependentUi
{
    bool parent = false;
    facet::DependentGroupOptions options;
    bool shown = false;
    int presses = 0;

    void Ui(facet::test::Session& session)
    {
        facet::Toggle("Parent", &parent);
        session.Track("parent");
        shown = facet::BeginDependentGroup("group", parent, options);
        if (shown)
        {
            if (facet::Button("Child"))
                ++presses;
            session.Track("child");
            facet::EndDependentGroup();
        }
        facet::Button("After");
        session.Track("after");
    }
};

} // namespace

TEST_CASE("A dependent group indents its content and disables it while the parent is off")
{
    ReducedMotion motion;
    facet::test::Session session;
    REQUIRE(session.Ready());
    DependentUi ui;
    session.SetUi([&] { ui.Ui(session); });
    session.Frames(2);
    REQUIRE(ui.shown);
    const float indent = facet::Px(facet::T().spacing.lg);
    CHECK(session.ItemRect("child").Min.x - session.ItemRect("parent").Min.x == doctest::Approx(indent));
    session.Click(session.ItemCenter("child"));
    CHECK(ui.presses == 0);
    // The keyboard goes from the parent straight past the disabled content.
    session.Click(session.ItemCenter("parent"));
    REQUIRE(ui.parent);
    session.Click(session.ItemCenter("parent"));
    REQUIRE_FALSE(ui.parent);
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("after"));
    // On: the content takes the mouse and the keyboard.
    session.KeyPress(ImGuiKey_Tab | ImGuiMod_Shift);
    session.KeyPress(ImGuiKey_Space);
    REQUIRE(ui.parent);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("child"));
    session.KeyPress(ImGuiKey_Enter);
    CHECK(ui.presses == 1);
    // Without indent the content starts with the parent.
    ui.options.indent = false;
    session.Frames(2);
    CHECK(session.ItemRect("child").Min.x == doctest::Approx(session.ItemRect("parent").Min.x));
}

TEST_CASE("A dependent group in Hide mode collapses with an animated height and then skips its content")
{
    ThemeGuard guard;
    facet::Theme theme = facet::T();
    theme.motion.reduceMotion = false;
    theme.motion.scale = 1.0f;
    facet::SetTheme(theme, false);
    facet::test::Session session;
    REQUIRE(session.Ready());
    DependentUi ui;
    ui.parent = true;
    ui.options.mode = facet::DependentMode::Hide;
    session.SetUi([&] { ui.Ui(session); });
    session.Frames(30);
    REQUIRE(ui.shown);
    const float open = session.ItemRect("after").Min.y;
    ui.parent = false;
    session.Frames(2);
    // On the way: still submitted, and "After" moves up.
    CHECK(ui.shown);
    const float closing = session.ItemRect("after").Min.y;
    CHECK(closing < open);
    session.Frames(60);
    CHECK_FALSE(ui.shown);
    CHECK(session.ItemRect("after").Min.y < closing);
    // Back on: it opens again.
    ui.parent = true;
    session.Frames(60);
    CHECK(ui.shown);
    CHECK(session.ItemRect("after").Min.y == doctest::Approx(open));
}

TEST_CASE("Dependent groups leave every stack balanced, nested, disabled and outside a frame")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values v;
    const int errorsBefore = fi::UsageErrorCount();
    session.SetUi([&] {
        CHECK_BALANCED({
            if (facet::BeginDependentGroup("outer", false))
            {
                if (facet::BeginDependentGroup("inner", true, {.mode = facet::DependentMode::Hide}))
                {
                    facet::BeginSettingsRow("Row");
                    ImGui::Button("##value");
                    facet::EndSettingsRow();
                    facet::EndDependentGroup();
                }
                facet::EndDependentGroup();
            }
        });
        CHECK_BALANCED({
            facet::DependentGroupScope group("scoped", true, {.indent = false});
            ImGui::TextUnformatted("content");
        });
        CHECK_BALANCED(CHECK_FALSE(facet::BeginDependentGroup("hidden", false, {.mode = facet::DependentMode::Hide})));
    });
    session.Frames(2);
    CHECK(fi::UsageErrorCount() == errorsBefore);
    CHECK(fi::OpenScopeCount() == 0);
    CHECK_FALSE(facet::BeginDependentGroup("outside", true));
    CHECK(fi::OpenScopeCount() == 0);
}

// ---- Flow ---------------------------------------------------------------------------------------

namespace
{

// A flow of `count` fixed-size buttons; records where each landed and which ones FlowNext let in.
struct FlowUi
{
    static constexpr int kMax = 8;
    int count = 5;
    float itemWidth = 100.0f;
    facet::FlowOptions options{.width = 330.0f};
    ImRect items[kMax];
    bool shown[kMax] = {};
    ImRect flow;

    void Ui()
    {
        facet::BeginFlow("flow", options);
        for (int i = 0; i < count; ++i)
        {
            shown[i] = facet::FlowNext();
            if (!shown[i])
                continue;
            ImGui::PushID(i);
            ImGui::Button("##item", ImVec2(itemWidth, 20.0f));
            items[i] = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
            ImGui::PopID();
        }
        facet::EndFlow();
        flow = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    }
};

} // namespace

TEST_CASE("A flow wraps its items onto new lines with the gap between items and lines")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    FlowUi ui;
    session.SetUi([&] { ui.Ui(); });
    session.Frames(3);
    const float gap = facet::Px(facet::T().spacing.sm);
    for (int i = 0; i < ui.count; ++i)
        CHECK(ui.shown[i]);
    CHECK(ui.items[1].Min.x - ui.items[0].Max.x == doctest::Approx(gap));
    CHECK(ui.items[2].Min.y == doctest::Approx(ui.items[0].Min.y));
    // 3 x 100 + 2 gaps fit 330; the fourth wraps.
    CHECK(ui.items[3].Min.x == doctest::Approx(ui.items[0].Min.x));
    CHECK(ui.items[3].Min.y == doctest::Approx(ui.items[0].Max.y + gap));
    CHECK(ui.flow.GetWidth() == doctest::Approx(330.0f));
    CHECK(ui.flow.Max.y == doctest::Approx(ui.items[4].Max.y));
}

TEST_CASE("A flow with a line limit counts the items that do not fit in a chip")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    FlowUi ui;
    ui.options.maxLines = 1;
    session.SetUi([&] { ui.Ui(); });
    session.Frames(4);
    // The third item would fit alone, but not with the chip after it.
    CHECK(ui.shown[0]);
    CHECK(ui.shown[1]);
    CHECK_FALSE(ui.shown[2]);
    CHECK_FALSE(ui.shown[3]);
    CHECK_FALSE(ui.shown[4]);
    // One line, and the chip stays inside the width.
    CHECK(ui.flow.GetHeight() < 40.0f);
    CHECK(ui.flow.Max.x <= ui.flow.Min.x + 330.0f + 0.5f);
    // Room for everything: nothing is hidden.
    ui.options.maxLines = 0;
    session.Frames(2);
    CHECK(ui.shown[4]);
}

TEST_CASE("A flow places its lines by align and spreads them with Stretch")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    FlowUi ui;
    ui.count = 2;
    ui.options.align = facet::LayoutAlign::Center;
    session.SetUi([&] { ui.Ui(); });
    session.Frames(3);
    const float gap = facet::Px(facet::T().spacing.sm);
    const float used = 200.0f + gap;
    CHECK(ui.items[0].Min.x - ui.flow.Min.x == doctest::Approx((330.0f - used) * 0.5f));
    ui.options.align = facet::LayoutAlign::End;
    session.Frames(2);
    CHECK(ui.items[1].Max.x == doctest::Approx(ui.flow.Min.x + 330.0f));
    // Stretch spreads every line but the last.
    ui.count = 5;
    ui.options.align = facet::LayoutAlign::Stretch;
    session.Frames(3);
    CHECK(ui.items[0].Min.x == doctest::Approx(ui.flow.Min.x));
    CHECK(ui.items[2].Max.x == doctest::Approx(ui.flow.Min.x + 330.0f));
    CHECK(ui.items[4].Min.x - ui.items[3].Max.x == doctest::Approx(gap));
}

TEST_CASE("A flow holds at a width of a few pixels, with no item and with one, at every scale")
{
    ScaleGuard scale;
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values v;
    FlowUi ui;
    ui.options.width = 3.0f;
    session.SetUi([&] { CHECK_BALANCED(ui.Ui()); });
    for (const float uiScale : {0.75f, 1.0f, 2.0f})
    {
        CAPTURE(uiScale);
        facet::SetUiScale(uiScale);
        ui.count = 3;
        session.Frames(3);
        // One item per line, none overlapping.
        CHECK(ui.items[1].Min.y > ui.items[0].Max.y);
        CHECK(ui.items[2].Min.y > ui.items[1].Max.y);
        ui.count = 1;
        session.Frames(2);
        CHECK(ui.flow.GetHeight() == doctest::Approx(20.0f));
        ui.count = 0;
        session.Frames(2);
        CHECK(ui.flow.GetHeight() == doctest::Approx(0.0f));
    }
}

TEST_CASE("Flows nest, and a stray FlowNext is reported")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values v;
    int reported = 0;
    session.SetUi([&] {
        CHECK_BALANCED({
            facet::FlowScope outer("outer", {.maxLines = 2});
            if (facet::FlowNext())
            {
                facet::FlowScope inner("inner", {.width = 120.0f});
                if (facet::FlowNext())
                    ImGui::TextUnformatted("inner item");
            }
            if (facet::FlowNext())
                ImGui::TextUnformatted("outer item");
        });
        const int before = fi::UsageErrorCount();
        CHECK_FALSE(facet::FlowNext());
        reported = fi::UsageErrorCount() - before;
    });
    session.Frames(2);
    CHECK(reported == 1);
}
