// Behaviour of the layout family (facet/widgets/layout.h). The core width classes and the labelled
// row layout of facet/core/layout.h are tested in tests/test_layout.cpp.
//
// One test case per behaviour of an implemented widget; docs/writing-a-widget.md describes the
// pattern. The API checks, and the placeholder checks of the widgets that are still planned, are in
// tests/test_api_layout.cpp.

#include <cfloat>
#include <cmath>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "internal/row_layout.h"
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
