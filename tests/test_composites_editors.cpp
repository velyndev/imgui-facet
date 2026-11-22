// Behaviour of the editor, property grid, canvas and layer composites (PropertyGrid, CurveEditor,
// BezierEditor, GradientEditor, PreviewCanvas, CanvasElement, LayerList).
//
// One test case per behaviour of an implemented widget; docs/writing-a-widget.md describes the
// pattern. The API checks, and the placeholder checks of the widgets that are still planned, are in
// tests/test_api_composites_editors.cpp.

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "internal/focus.h"
#include "internal/popup_host.h"
#include "internal/state_storage.h"
#include "support/session.h"
#include "widgets/composites/editors_frame.h"
#include "widgets/layout/layout_internal.h"

namespace
{

// Counts the Dear ImGui allocations of a few frames (vector growth, draw lists, windows).
int CountImGuiAllocations(facet::test::Session& session, int frames)
{
    static int allocations = 0;
    allocations = 0;
    ImGuiMemAllocFunc oldAlloc = nullptr;
    ImGuiMemFreeFunc oldFree = nullptr;
    void* oldUser = nullptr;
    ImGui::GetAllocatorFunctions(&oldAlloc, &oldFree, &oldUser);
    ImGui::SetAllocatorFunctions(
        [](size_t size, void*) -> void* {
            ++allocations;
            return std::malloc(size);
        },
        [](void* pointer, void*) { std::free(pointer); });
    session.Frames(frames);
    ImGui::SetAllocatorFunctions(oldAlloc, oldFree, oldUser);
    return allocations;
}

void ShowFocus(ImGuiID id)
{
    ImGui::SetFocusID(id, ImGui::FindWindowByName("test"));
    ImGui::GetCurrentContext()->NavCursorVisible = true;
}

// ---- PropertyGrid ----------------------------------------------------------------------------------

struct GridUi
{
    bool toggle = false;
    float slider = 0.5f;
    float number = 1.0f;
    std::string text = "abc";
    int changes = 0;
    const char* filter = nullptr;
    // Each GridUi names its grid apart: the open and column states are kept like ini records and
    // would otherwise carry from one test case to the next.
    char name[32] = {};
    ImGuiID gridId = 0;
    ImGuiID tooltipId = 0;
    bool nestedOpen = false;
    ImRect enabledLabel;  // the label box of the "Enabled" row
    bool tooltipOpen = false;

    GridUi()
    {
        static int grids = 0;
        ImFormatString(name, sizeof(name), "grid %d", ++grids);
    }

    void Draw(facet::test::Session& session)
    {
        nestedOpen = false;
        facet::BeginPropertyGrid(name, {.filter = filter});
        gridId = ImGui::GetCurrentWindow()->IDStack.back();  // the grid's id is on the id stack inside it
        if (facet::BeginPropertyCategory("General"))
        {
            if (facet::BeginPropertyRow("Enabled", {.tooltip = "Turns the effect on", .keywords = "switch"}))
            {
                tooltipId = ImGui::GetID("Enabled");
                changes += facet::Toggle("##value", &toggle) ? 1 : 0;
                session.Track("toggle");
                facet::EndPropertyRow();
                enabledLabel = facet::internal::LastSettingsRowLayout().label;
            }
            if (facet::BeginPropertyRow("A much longer property label", {.modified = true}))
            {
                changes += facet::Slider("##value", &slider, 0.0f, 1.0f) ? 1 : 0;
                session.Track("slider");
                facet::EndPropertyRow();
            }
            if (facet::BeginPropertyCategory("Nested"))
            {
                nestedOpen = true;
                if (facet::BeginPropertyRow("Count"))
                {
                    changes += facet::NumberInput("##value", &number) ? 1 : 0;
                    session.Track("number");
                    facet::EndPropertyRow();
                }
                facet::EndPropertyCategory();
            }
            facet::EndPropertyCategory();
        }
        if (facet::BeginPropertyCategory("Text", {.defaultOpen = false}))
        {
            if (facet::BeginPropertyRow("Name"))
            {
                changes += facet::TextField("##value", &text) ? 1 : 0;
                session.Track("text");
                facet::EndPropertyRow();
            }
            facet::EndPropertyCategory();
        }
        facet::EndPropertyGrid();
        session.Track("grid");
        // The popup host answers inside the frame.
        tooltipOpen = facet::internal::IsPopupLevelOpen(facet::internal::PopupTooltipLevel(tooltipId));
    }
};

} // namespace

TEST_CASE("Property grid: every row's control starts at one shared column, nested rows included")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    GridUi ui;
    session.SetUi([&] { ui.Draw(session); });
    session.Frames(3);
    const float x = session.ItemRect("toggle").Min.x;
    CHECK(x > session.ItemRect("grid").Min.x);
    CHECK(session.ItemRect("slider").Min.x == doctest::Approx(x));
    CHECK(session.ItemRect("number").Min.x == doctest::Approx(x));
    CHECK(session.ItemRect("number").Min.y > session.ItemRect("slider").Max.y);
    // The default column is the labelColumn token.
    CHECK(facet::internal::PropertyGridColumnUnits(ui.gridId) == doctest::Approx(facet::T().layout.labelColumn));
}

TEST_CASE("Property grid: each control reports one change per edit through the finished widget")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    GridUi ui;
    session.SetUi([&] { ui.Draw(session); });
    session.Frames(3);
    session.Click(session.ItemCenter("toggle"));
    CHECK(ui.toggle);
    CHECK(ui.changes == 1);
    session.KeyPress(ImGuiKey_Tab);  // after a click the first Tab shows the focus
    session.KeyPress(ImGuiKey_Tab);  // the slider
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(ui.slider == doctest::Approx(0.51f));
    CHECK(ui.changes == 2);
}

TEST_CASE("Property grid: the column edge drags, steps by keyboard, cancels with Escape and resets")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    GridUi ui;
    session.SetUi([&] { ui.Draw(session); });
    session.Frames(3);
    const facet::internal::PropertyGridDividerRecord divider = facet::internal::LastPropertyGridDivider();
    REQUIRE(divider.id != 0);
    const float start = facet::internal::PropertyGridColumnUnits(ui.gridId);
    const float x0 = session.ItemRect("toggle").Min.x;

    // A drag 40 units to the right moves the column and every control with it.
    const ImVec2 from(divider.band.GetCenter().x, session.ItemCenter("toggle").y);
    session.Drag({from, ImVec2(from.x + facet::Px(40.0f), from.y)}, 4);
    session.Frames(2);
    CHECK(facet::internal::PropertyGridColumnUnits(ui.gridId) == doctest::Approx(start + 40.0f).epsilon(0.02));
    CHECK(session.ItemRect("toggle").Min.x == doctest::Approx(x0 + facet::Px(40.0f)).epsilon(0.02));
    CHECK(session.ItemRect("slider").Min.x == doctest::Approx(session.ItemRect("toggle").Min.x));

    // Escape during a drag puts the column back where the drag started.
    const float dragged = facet::internal::PropertyGridColumnUnits(ui.gridId);
    const ImVec2 edge(facet::internal::LastPropertyGridDivider().band.GetCenter().x, from.y);
    session.MouseMove(edge);
    session.MouseDown();
    session.MouseMove(ImVec2(edge.x + facet::Px(40.0f), edge.y));
    session.KeyPress(ImGuiKey_Escape);
    session.MouseUp();
    session.Frames(2);
    CHECK(facet::internal::PropertyGridColumnUnits(ui.gridId) == doctest::Approx(dragged));

    // The keyboard: Right steps the focused edge by spacing.lg, Home takes the narrowest column.
    ShowFocus(divider.id);
    session.Frames(1);
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(facet::internal::PropertyGridColumnUnits(ui.gridId) == doctest::Approx(dragged + facet::T().spacing.lg));
    session.KeyPress(ImGuiKey_Home);
    CHECK(facet::internal::PropertyGridColumnUnits(ui.gridId) ==
          doctest::Approx(facet::T().composites.propertyLabelMinWidth));

    // A double click returns to the default column.
    session.DoubleClick(ImVec2(facet::internal::LastPropertyGridDivider().band.GetCenter().x, from.y));
    session.Frames(2);
    CHECK(facet::internal::PropertyGridColumnUnits(ui.gridId) == doctest::Approx(start));
}

TEST_CASE("Property grid: a narrow grid stacks its rows and has no column edge")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    GridUi ui;
    session.SetUi([&] {
        ImGui::BeginChild("narrow", ImVec2(facet::Px(160.0f), facet::Px(400.0f)));
        ui.Draw(session);
        ImGui::EndChild();
    });
    session.Frames(3);
    CHECK(facet::internal::LastPropertyGridDivider().id == 0);
    // The control sits under its label, at the left edge of the rows.
    CHECK(session.ItemRect("toggle").Min.x < session.ItemRect("grid").Min.x + facet::Px(40.0f));
    CHECK(session.ItemRect("slider").Max.x <= session.ItemRect("grid").Max.x + 0.5f);
}

TEST_CASE("Property grid: the filter shows matching rows and their categories, and says when none match")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    GridUi ui;
    session.SetUi([&] { ui.Draw(session); });
    session.Frames(3);
    CHECK(facet::internal::LastPropertyGridRowCount() == 3);  // "Text" starts closed

    ui.filter = "name";  // a row of the closed category: it shows while the filter holds
    session.Frames(3);
    CHECK(facet::internal::LastPropertyGridRowCount() == 1);
    CHECK(session.ItemRect("text").GetWidth() > 0.0f);

    ui.filter = "switch";  // a keyword
    session.Frames(3);
    CHECK(facet::internal::LastPropertyGridRowCount() == 1);
    CHECK(session.ItemRect("toggle").GetWidth() > 0.0f);

    ui.filter = "zzzz";  // no hit: the empty state in the grid
    session.Frames(3);
    CHECK(facet::internal::LastPropertyGridRowCount() == 0);
    CHECK(session.ItemRect("grid").GetHeight() > 0.0f);

    ui.filter = nullptr;  // back: "Text" closes again (after its animation), as the user left it
    session.Frames(30);
    CHECK(facet::internal::LastPropertyGridRowCount() == 3);
}

TEST_CASE("Property grid: a category header toggles with a click, Enter, Space, Left and Right")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    GridUi ui;
    session.SetUi([&] { ui.Draw(session); });
    session.Frames(3);
    CHECK(ui.nestedOpen);
    const ImRect number = session.ItemRect("number");
    // The "Nested" header sits right above its row.
    const ImVec2 header(number.Min.x - facet::Px(4.0f),
                        number.Min.y - ImGui::GetStyle().ItemSpacing.y - facet::ControlHeight(facet::ControlSize::Regular) * 0.5f);
    session.Click(header);
    session.Frames(30);
    CHECK_FALSE(ui.nestedOpen);
    ImGui::GetCurrentContext()->NavCursorVisible = true;
    session.KeyPress(ImGuiKey_RightArrow);
    session.Frames(30);
    CHECK(ui.nestedOpen);
    session.KeyPress(ImGuiKey_LeftArrow);
    session.Frames(30);
    CHECK_FALSE(ui.nestedOpen);
    session.KeyPress(ImGuiKey_Space);
    session.Frames(30);
    CHECK(ui.nestedOpen);
    session.KeyPress(ImGuiKey_Enter);
    session.Frames(30);
    CHECK_FALSE(ui.nestedOpen);
}

TEST_CASE("Property grid: a row's tooltip shows on hover of its label")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    GridUi ui;
    session.SetUi([&] { ui.Draw(session); });
    session.Frames(3);
    REQUIRE(ui.enabledLabel.GetWidth() > 0.0f);
    session.MouseMove(ui.enabledLabel.GetCenter());
    session.Seconds(1.5f);
    CHECK(ui.tooltipOpen);
    session.MouseMove(ImVec2(ui.enabledLabel.Max.x + facet::Px(300.0f), ui.enabledLabel.Max.y + facet::Px(200.0f)));
    session.Seconds(0.5f);
    CHECK_FALSE(ui.tooltipOpen);
}

TEST_CASE("Property grid: a shown grid allocates nothing per frame")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    GridUi ui;
    session.SetUi([&] { ui.Draw(session); });
    session.Frames(10);
    const int states = facet::internal::GlobalStateStore().Count();
    CHECK(CountImGuiAllocations(session, 10) == 0);
    CHECK(facet::internal::GlobalStateStore().Count() == states);
}

// ---- GradientEditor ----------------------------------------------------------------------------------


namespace
{

// Geometry of a gradient editor of the default height whose frame is `r`: the stop markers on their
// row under the bar, and points on the bar.
float GradientBarInset()
{
    return std::round(std::round(facet::Px(facet::T().composites.gradientStopSize)) * 0.5f);
}

ImVec2 GradientStopAt(const ImRect& r, float position)
{
    const float inset = GradientBarInset();
    const float row = std::round(std::max(std::round(facet::Px(facet::T().composites.gradientStopSize)),
                                          facet::Px(facet::T().metrics.minHitTarget)));
    return ImVec2(r.Min.x + inset + position * (r.GetWidth() - inset * 2.0f), r.Max.y - row * 0.5f);
}

ImVec2 GradientBarAt(const ImRect& r, float position)
{
    const float inset = GradientBarInset();
    return ImVec2(r.Min.x + inset + position * (r.GetWidth() - inset * 2.0f), r.Min.y + facet::Px(8.0f));
}

struct GradientUi
{
    facet::GradientStop stops[4] = {{0.0f, {0.0f, 0.0f, 0.0f, 1.0f}}, {1.0f, {1.0f, 1.0f, 1.0f, 1.0f}}};
    int count = 2;
    int writes = 0;
    facet::GradientEditorOptions options;
    bool popoverOpen = false;
    ImGuiID id = 0;

    void Draw(facet::test::Session& session)
    {
        id = ImGui::GetID("Gradient");
        writes += facet::GradientEditor("Gradient", stops, &count, options) ? 1 : 0;
        session.Track("gradient");
        popoverOpen = facet::internal::IsPopupLevelOpen(ImHashStr("##stop", 0, id));
    }
};

bool GradientSorted(const facet::GradientStop* stops, int count)
{
    for (int i = 1; i < count; ++i)
        if (stops[i - 1].position > stops[i].position)
            return false;
    return true;
}

} // namespace

TEST_CASE("Gradient editor: a click on the bar adds a stop with the color shown there and focuses it")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    GradientUi ui;
    session.SetUi([&] { ui.Draw(session); });
    session.Frames(2);
    const ImRect r = session.ItemRect("gradient");
    session.Click(GradientBarAt(r, 0.5f));
    REQUIRE(ui.count == 3);
    CHECK(ui.writes == 1);
    CHECK(GradientSorted(ui.stops, ui.count));
    CHECK(ui.stops[1].position == doctest::Approx(0.5f).epsilon(0.02));
    // The bar interpolates in gamma-encoded sRGB: half way from black to white is 0.5 per channel.
    CHECK(ui.stops[1].color.x == doctest::Approx(0.5f).epsilon(0.03));
    CHECK(ui.stops[1].color.w == doctest::Approx(1.0f));
    // The new stop has the keyboard focus: Delete removes it again.
    session.KeyPress(ImGuiKey_Delete);
    CHECK(ui.count == 2);
}

TEST_CASE("Gradient editor: a stop dragged past another passes it and keeps its drag")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    GradientUi ui;
    ui.stops[1] = {0.5f, {1.0f, 0.0f, 0.0f, 1.0f}};
    ui.stops[2] = {1.0f, {1.0f, 1.0f, 1.0f, 1.0f}};
    ui.count = 3;
    session.SetUi([&] { ui.Draw(session); });
    session.Frames(2);
    const ImRect r = session.ItemRect("gradient");
    // The black stop at 0 goes to 0.75: past the red one, which becomes the first stop.
    session.Drag({GradientStopAt(r, 0.0f), GradientStopAt(r, 0.3f), GradientStopAt(r, 0.6f), GradientStopAt(r, 0.75f)}, 4);
    session.Frames(2);
    REQUIRE(ui.count == 3);
    CHECK(GradientSorted(ui.stops, ui.count));
    CHECK(ui.stops[0].color.x == doctest::Approx(1.0f));  // red first
    CHECK(ui.stops[1].color.x == doctest::Approx(0.0f));  // the dragged black one, still with its drag
    CHECK(ui.stops[1].position == doctest::Approx(0.75f).epsilon(0.02));
}

TEST_CASE("Gradient editor: keys move, add and remove stops, never the last one")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    GradientUi ui;
    session.SetUi([&] { ui.Draw(session); });
    session.Frames(2);
    const ImRect r = session.ItemRect("gradient");
    // Focus the first stop by keyboard: a click on it, then Tab shows the focus.
    session.MouseMove(GradientStopAt(r, 0.0f));
    session.MouseDown();
    session.MouseUp();
    session.KeyPress(ImGuiKey_Escape);  // closes the popover the click opened; the focus returns to the stop
    session.Frames(2);
    facet::internal::SetKeyboardModality(true);
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(ui.stops[0].position == doctest::Approx(0.01f));
    session.KeyPress(ImGuiKey_Insert);  // half way to the next stop
    REQUIRE(ui.count == 3);
    CHECK(ui.stops[1].position == doctest::Approx(0.505f).epsilon(0.01));
    CHECK(GradientSorted(ui.stops, ui.count));
    // Storage full: Insert adds nothing.
    session.KeyPress(ImGuiKey_Insert);
    CHECK(ui.count == 4);
    session.KeyPress(ImGuiKey_Insert);
    CHECK(ui.count == 4);
    // Delete removes the focused stop, down to one.
    for (int i = 0; i < 6; ++i)
        session.KeyPress(ImGuiKey_Delete);
    CHECK(ui.count == 1);
}

TEST_CASE("Gradient editor: a stop dragged far off the bar is removed on release")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    GradientUi ui;
    ui.stops[1] = {0.5f, {1.0f, 0.0f, 0.0f, 1.0f}};
    ui.stops[2] = {1.0f, {1.0f, 1.0f, 1.0f, 1.0f}};
    ui.count = 3;
    session.SetUi([&] { ui.Draw(session); });
    session.Frames(2);
    const ImRect r = session.ItemRect("gradient");
    const ImVec2 from = GradientStopAt(r, 0.5f);
    session.Drag({from, ImVec2(from.x, r.Max.y + facet::Px(facet::T().composites.stopRemoveDistance) + facet::Px(20.0f))}, 4);
    session.Frames(2);
    CHECK(ui.count == 2);
    CHECK(ui.stops[0].color.x == doctest::Approx(0.0f));
    CHECK(ui.stops[1].color.x == doctest::Approx(1.0f));
    CHECK(ui.stops[1].color.y == doctest::Approx(1.0f));
}

TEST_CASE("Gradient editor: a click on a stop opens its color popover and Escape closes it")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    GradientUi ui;
    session.SetUi([&] { ui.Draw(session); });
    session.Frames(2);
    const ImRect r = session.ItemRect("gradient");
    session.Click(GradientStopAt(r, 1.0f));
    session.Frames(2);
    CHECK(ui.popoverOpen);
    CHECK(ui.count == 2);
    CHECK(ui.stops[1].position == doctest::Approx(1.0f));
    session.KeyPress(ImGuiKey_Escape);
    session.Frames(10);
    CHECK_FALSE(ui.popoverOpen);
    CHECK(ui.writes == 0);
}

TEST_CASE("Gradient editor: read-only shows the stops and writes nothing")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    GradientUi ui;
    ui.options.readOnly = true;
    session.SetUi([&] { ui.Draw(session); });
    session.Frames(2);
    const ImRect r = session.ItemRect("gradient");
    session.Click(GradientBarAt(r, 0.5f));
    session.Drag({GradientStopAt(r, 0.0f), GradientStopAt(r, 0.5f)}, 4);
    CHECK(ui.count == 2);
    CHECK(ui.stops[0].position == 0.0f);
    CHECK(ui.writes == 0);
}

TEST_CASE("Gradient editor: the vector form grows its vector when a stop is added")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::vector<facet::GradientStop> stops = {{0.0f, {0.0f, 0.0f, 0.0f, 1.0f}}, {1.0f, {1.0f, 1.0f, 1.0f, 1.0f}}};
    stops.shrink_to_fit();
    session.SetUi([&] {
        facet::GradientEditor("Gradient", &stops);
        session.Track("gradient");
    });
    session.Frames(2);
    session.Click(GradientBarAt(session.ItemRect("gradient"), 0.25f));
    REQUIRE(stops.size() == 3);
    CHECK(stops[1].position == doctest::Approx(0.25f).epsilon(0.03));
    CHECK(stops[1].color.x == doctest::Approx(0.25f).epsilon(0.05));
}

TEST_CASE("Gradient editor: a shown editor allocates nothing per frame")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    GradientUi ui;
    ui.options.alpha = true;
    session.SetUi([&] { ui.Draw(session); });
    session.Frames(10);
    session.MouseMove(GradientStopAt(session.ItemRect("gradient"), 0.0f));  // a hovered stop
    session.Frames(10);
    const int states = facet::internal::GlobalStateStore().Count();
    CHECK(CountImGuiAllocations(session, 10) == 0);
    CHECK(facet::internal::GlobalStateStore().Count() == states);
}
