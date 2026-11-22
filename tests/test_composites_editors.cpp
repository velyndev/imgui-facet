// Behaviour of the editor, property grid, canvas and layer composites (PropertyGrid, CurveEditor,
// BezierEditor, GradientEditor, PreviewCanvas, CanvasElement, LayerList).
//
// One test case per behaviour of an implemented widget; docs/writing-a-widget.md describes the
// pattern. The API checks are in tests/test_api_composites_editors.cpp.

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
#include "internal/list_view.h"
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

TEST_CASE("Property grid: a grid scrolled above the window keeps its own height")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    GridUi ui;
    float unscrolled = 0.0f;
    float scrolled = 0.0f;
    float scroll = 0.0f;
    session.SetUi([&] {
        ImGui::BeginChild("scrolled", ImVec2(facet::Px(400.0f), facet::Px(200.0f)));
        ImGui::SetScrollY(scroll);
        ImGui::Dummy(ImVec2(10.0f, facet::Px(1200.0f)));
        ui.Draw(session);
        ImGui::Dummy(ImVec2(10.0f, facet::Px(1200.0f)));
        ImGui::EndChild();
    });
    session.Frames(3);
    unscrolled = session.ItemRect("grid").GetHeight();
    scroll = facet::Px(1900.0f);  // the grid lies wholly above the visible part
    session.Frames(3);
    scrolled = session.ItemRect("grid").GetHeight();
    CHECK(unscrolled > 0.0f);
    CHECK(scrolled == doctest::Approx(unscrolled));
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

// ---- CurveEditor -------------------------------------------------------------------------------------

namespace
{

struct CurveUi
{
    ImVec2 points[4] = {{0.0f, 0.0f}, {0.5f, 0.5f}, {1.0f, 1.0f}};
    int count = 3;
    int writes = 0;
    facet::CurveEditorOptions options;
    ImGuiID id = 0;

    void Draw(facet::test::Session& session)
    {
        ImGui::Button("before");
        session.Track("before");
        id = ImGui::GetID("Curve");
        writes += facet::CurveEditor("Curve", points, &count, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), options) ? 1 : 0;
        session.Track("curve");
    }

    ImVec2 At(ImVec2 value) const { return facet::internal::CurveEditorPointPos(id, value); }
};

// Puts the keyboard focus on point `index` with the focus visible.
void FocusCurvePoint(facet::test::Session& session, const CurveUi& ui, int index)
{
    ShowFocus(facet::internal::CurveEditorPointId(ui.id, index));
    facet::internal::SetKeyboardModality(true);
    session.Frames(1);
}

} // namespace

TEST_CASE("Curve editor: Tab goes from point to point and the arrows move the focused one")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    CurveUi ui;
    session.SetUi([&] { ui.Draw(session); });
    session.Frames(2);
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);  // after a click the first Tab shows the focus
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == facet::internal::CurveEditorPointId(ui.id, 0));
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == facet::internal::CurveEditorPointId(ui.id, 1));
    session.KeyPress(ImGuiKey_UpArrow);
    CHECK(ui.points[1].y == doctest::Approx(0.51f));
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(ui.points[1].x == doctest::Approx(0.51f));
    CHECK(ui.writes == 2);
}

TEST_CASE("Curve editor: a point never passes its neighbours and locked ends keep their x")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    CurveUi ui;
    session.SetUi([&] { ui.Draw(session); });
    session.Frames(2);
    FocusCurvePoint(session, ui, 1);
    session.KeyPress(ImGuiKey_End);
    CHECK(ui.points[1].x == doctest::Approx(1.0f));  // up to its right neighbour, not past it
    CHECK(ui.points[2].x == 1.0f);
    // A drag far to the left stops at the left neighbour.
    const ImVec2 from = ui.At(ui.points[1]);
    session.Drag({from, ImVec2(from.x - session.ItemRect("curve").GetWidth() * 1.5f, from.y)}, 4);
    CHECK(ui.points[1].x == doctest::Approx(0.0f));
    CHECK(ui.points[0].x == 0.0f);
    // The locked first point moves up and down only.
    FocusCurvePoint(session, ui, 0);
    session.KeyPress(ImGuiKey_RightArrow);
    session.KeyPress(ImGuiKey_UpArrow);
    CHECK(ui.points[0].x == 0.0f);
    CHECK(ui.points[0].y == doctest::Approx(0.01f));
}

TEST_CASE("Curve editor: a double click adds a point in order and removes one, never below two")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    CurveUi ui;
    session.SetUi([&] { ui.Draw(session); });
    session.Frames(2);
    session.DoubleClick(ui.At(ImVec2(0.25f, 0.75f)));
    REQUIRE(ui.count == 4);
    CHECK(ui.points[1].x == doctest::Approx(0.25f).epsilon(0.03));
    CHECK(ui.points[1].y == doctest::Approx(0.75f).epsilon(0.03));
    CHECK(ImGui::GetFocusID() == facet::internal::CurveEditorPointId(ui.id, 1));
    // The storage is full: no fifth point.
    session.DoubleClick(ui.At(ImVec2(0.75f, 0.25f)));
    CHECK(ui.count == 4);
    // A double click on a point removes it; Delete removes the focused one; the locked ends stay.
    session.DoubleClick(ui.At(ui.points[1]));
    CHECK(ui.count == 3);
    CHECK(ui.points[1].x == 0.5f);
    FocusCurvePoint(session, ui, 1);
    session.KeyPress(ImGuiKey_Delete);
    CHECK(ui.count == 2);
    FocusCurvePoint(session, ui, 0);
    session.KeyPress(ImGuiKey_Delete);
    CHECK(ui.count == 2);
}

TEST_CASE("Curve editor: values outside the range are drawn clamped and written only by an edit")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    CurveUi ui;
    ui.points[1] = {0.5f, 3.0f};
    session.SetUi([&] { ui.Draw(session); });
    session.Frames(3);
    CHECK(ui.points[1].y == 3.0f);
    CHECK(ui.writes == 0);
    const ImRect r = session.ItemRect("curve");
    const ImVec2 shown = ui.At(ui.points[1]);
    CHECK(shown.y >= r.Min.y);
    FocusCurvePoint(session, ui, 1);
    session.KeyPress(ImGuiKey_DownArrow);
    CHECK(ui.points[1].y <= 1.0f);
}

TEST_CASE("Curve editor: the vector form grows its vector when a point is added")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::vector<ImVec2> points = {{0.0f, 0.0f}, {1.0f, 1.0f}};
    points.shrink_to_fit();
    ImGuiID id = 0;
    session.SetUi([&] {
        id = ImGui::GetID("Curve");
        facet::CurveEditor("Curve", &points, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f));
    });
    session.Frames(2);
    session.DoubleClick(facet::internal::CurveEditorPointPos(id, ImVec2(0.5f, 0.2f)));
    REQUIRE(points.size() == 3);
    CHECK(points[1].x == doctest::Approx(0.5f).epsilon(0.03));
}

TEST_CASE("Curve editor: read-only shows the points and writes nothing")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    CurveUi ui;
    ui.options.readOnly = true;
    session.SetUi([&] { ui.Draw(session); });
    session.Frames(2);
    session.DoubleClick(ui.At(ImVec2(0.25f, 0.75f)));
    const ImVec2 from = ui.At(ui.points[1]);
    session.Drag({from, ImVec2(from.x, from.y - facet::Px(40.0f))}, 4);
    FocusCurvePoint(session, ui, 1);
    session.KeyPress(ImGuiKey_UpArrow);
    CHECK(ui.count == 3);
    CHECK(ui.points[1].y == 0.5f);
    CHECK(ui.writes == 0);
}

TEST_CASE("Curve editor: works in a table cell, where the table has split the draw list")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    CurveUi ui;
    session.SetUi([&] {
        if (ImGui::BeginTable("table", 2))
        {
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("cell");
            ImGui::TableNextColumn();
            ui.Draw(session);
            ImGui::EndTable();
        }
    });
    session.Frames(3);
    CHECK(session.ItemRect("curve").GetWidth() > 0.0f);
    const ImVec2 from = ui.At(ui.points[1]);
    session.Drag({from, ImVec2(from.x, from.y - facet::Px(20.0f))}, 4);
    CHECK(ui.points[1].y > 0.5f);
}

TEST_CASE("Curve editor: a shown editor allocates nothing per frame")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    CurveUi ui;
    ui.options.marker = 0.3f;
    session.SetUi([&] { ui.Draw(session); });
    session.Frames(10);
    session.MouseMove(ui.At(ui.points[1]));
    session.Frames(10);
    const int states = facet::internal::GlobalStateStore().Count();
    CHECK(CountImGuiAllocations(session, 10) == 0);
    CHECK(facet::internal::GlobalStateStore().Count() == states);
}

// ---- BezierEditor ------------------------------------------------------------------------------------

namespace
{

// Sets the theme's reduced motion for the life of the guard, and restores the theme.
struct EditorsMotionGuard
{
    facet::Theme saved = facet::T();
    explicit EditorsMotionGuard(bool reduce)
    {
        facet::Theme theme = saved;
        theme.motion.reduceMotion = reduce;
        facet::SetTheme(theme, false);
    }
    ~EditorsMotionGuard() { facet::SetTheme(saved, false); }
    EditorsMotionGuard(const EditorsMotionGuard&) = delete;
    EditorsMotionGuard& operator=(const EditorsMotionGuard&) = delete;
};

struct BezierUi
{
    facet::CubicBezier curve{{0.25f, 0.1f}, {0.25f, 1.0f}};
    int writes = 0;
    facet::BezierEditorOptions options;
    ImGuiID id = 0;
    bool previewing = false;

    void Draw(facet::test::Session& session)
    {
        id = ImGui::GetID("Easing");
        writes += facet::BezierEditor("Easing", &curve, options) ? 1 : 0;
        session.Track("easing");
        previewing = facet::internal::BezierEditorPreviewRunning(id);
    }
};

void FocusBezierPoint(facet::test::Session& session, const BezierUi& ui, int index)
{
    ShowFocus(facet::internal::BezierEditorPointId(ui.id, index));
    facet::internal::SetKeyboardModality(true);
    session.Frames(1);
}

} // namespace

TEST_CASE("Bezier editor: the arrows move a control point, x stays in 0 to 1 and y too without overshoot")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    BezierUi ui;
    session.SetUi([&] { ui.Draw(session); });
    session.Frames(2);
    FocusBezierPoint(session, ui, 0);
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(ui.curve.p1.x == doctest::Approx(0.26f));
    CHECK(ui.writes == 1);
    session.KeyPress(ImGuiKey_Home);
    CHECK(ui.curve.p1.x == 0.0f);
    FocusBezierPoint(session, ui, 1);
    session.HoldKey(ImGuiKey_UpArrow, 1.0f);
    CHECK(ui.curve.p2.y == 1.0f);  // no overshoot
    // A drag far out of the canvas ends at its edges.
    const ImVec2 from = facet::internal::BezierEditorPointPos(ui.id, ui.curve.p2);
    session.Drag({from, ImVec2(from.x + facet::Px(2000.0f), from.y - facet::Px(2000.0f))}, 4);
    CHECK(ui.curve.p2.x == 1.0f);
    CHECK(ui.curve.p2.y == 1.0f);
}

TEST_CASE("Bezier editor: with overshoot a control point leaves 0 to 1")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    BezierUi ui;
    ui.options.allowOvershoot = true;
    session.SetUi([&] { ui.Draw(session); });
    session.Frames(2);
    FocusBezierPoint(session, ui, 1);
    session.KeyPress(ImGuiKey_PageUp);
    CHECK(ui.curve.p2.y > 1.0f);
    FocusBezierPoint(session, ui, 0);
    session.KeyPress(ImGuiKey_PageDown);
    session.KeyPress(ImGuiKey_PageDown);
    CHECK(ui.curve.p1.y < 0.0f);
}

TEST_CASE("Bezier editor: the preview runs once after an edit, not with reduced motion")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    {
        EditorsMotionGuard motion(false);
        BezierUi ui;
        session.SetUi([&] { ui.Draw(session); });
        session.Frames(2);
        CHECK_FALSE(ui.previewing);
        FocusBezierPoint(session, ui, 0);
        session.KeyPress(ImGuiKey_UpArrow);
        CHECK(ui.previewing);
        session.Seconds(facet::T().composites.easingPreviewTime * facet::T().motion.scale + 0.2f);
        CHECK_FALSE(ui.previewing);
    }
    {
        EditorsMotionGuard motion(true);
        BezierUi ui;
        session.SetUi([&] { ui.Draw(session); });
        session.Frames(2);
        FocusBezierPoint(session, ui, 0);
        session.KeyPress(ImGuiKey_UpArrow);
        CHECK(ui.writes == 1);
        CHECK_FALSE(ui.previewing);
    }
}

TEST_CASE("Bezier editor: read-only shows the curve and writes nothing")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    BezierUi ui;
    ui.options.readOnly = true;
    session.SetUi([&] { ui.Draw(session); });
    session.Frames(2);
    FocusBezierPoint(session, ui, 0);
    session.KeyPress(ImGuiKey_RightArrow);
    const ImVec2 from = facet::internal::BezierEditorPointPos(ui.id, ui.curve.p2);
    session.Drag({from, ImVec2(from.x - facet::Px(40.0f), from.y)}, 4);
    CHECK(ui.curve.p1.x == 0.25f);
    CHECK(ui.curve.p2.x == 0.25f);
    CHECK(ui.writes == 0);
}

TEST_CASE("Bezier editor: a shown editor allocates nothing per frame")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    BezierUi ui;
    session.SetUi([&] { ui.Draw(session); });
    session.Frames(10);
    const int states = facet::internal::GlobalStateStore().Count();
    CHECK(CountImGuiAllocations(session, 10) == 0);
    CHECK(facet::internal::GlobalStateStore().Count() == states);
}

// ---- LayerList ---------------------------------------------------------------------------------------

namespace
{

struct LayerUi
{
    facet::LayerItem layers[4] = {{"Layer 1"}, {"Layer 2"}, {"Layer 3"}, {"Layer 4"}};
    int selected = 0;
    facet::LayerListResult last;
    int actions = 0;
    facet::LayerListOptions options;

    void Draw(facet::test::Session& session)
    {
        ImGui::Button("before");
        session.Track("before");
        const facet::LayerListResult result = facet::LayerList("layers", &selected, layers, options);
        session.Track("layers");
        if (result.action != facet::LayerAction::None)
        {
            last = result;
            ++actions;
            if (result.action == facet::LayerAction::Moved)
                facet::ApplyReorderMove(layers, result.move);
        }
    }
};

float LayerRowHeight()
{
    return facet::internal::ListRowHeight(facet::ControlSize::Regular);
}

ImVec2 LayerRowAt(const ImRect& r, int row)
{
    return ImVec2(r.Min.x + facet::Px(60.0f), r.Min.y + LayerRowHeight() * (static_cast<float>(row) + 0.5f));
}

// The visibility (column 0) or lock (column 1) toggle of a row.
ImVec2 LayerToggleAt(const ImRect& r, int row, int column)
{
    const float h = LayerRowHeight();
    return ImVec2(r.Max.x - h * (column == 0 ? 1.5f : 0.5f), r.Min.y + h * (static_cast<float>(row) + 0.5f));
}

} // namespace

TEST_CASE("Layer list: a click selects a row and the arrows move the selection")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    LayerUi ui;
    session.SetUi([&] { ui.Draw(session); });
    session.Frames(2);
    const ImRect r = session.ItemRect("layers");
    session.Click(LayerRowAt(r, 2));
    CHECK(ui.selected == 2);
    CHECK(ui.last.action == facet::LayerAction::Selected);
    facet::internal::SetKeyboardModality(true);
    session.KeyPress(ImGuiKey_DownArrow);
    CHECK(ui.selected == 3);
    session.KeyPress(ImGuiKey_Home);
    CHECK(ui.selected == 0);
}

TEST_CASE("Layer list: the rows are one Tab stop, then the focused row's toggles")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    LayerUi ui;
    ui.selected = 1;
    session.SetUi([&] { ui.Draw(session); });
    session.Frames(2);
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);  // the selected row
    session.KeyPress(ImGuiKey_Tab);  // its visibility toggle
    session.KeyPress(ImGuiKey_Space);
    CHECK_FALSE(ui.layers[1].visible);
    CHECK(ui.last.action == facet::LayerAction::Visibility);
    CHECK(ui.last.index == 1);
    session.KeyPress(ImGuiKey_Tab);  // its lock toggle
    session.KeyPress(ImGuiKey_Enter);
    CHECK(ui.layers[1].locked);
    CHECK(ui.last.action == facet::LayerAction::Lock);
    // The other rows' toggles are no Tab stops: the next Tab leaves the list.
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Space);
    CHECK(ui.layers[1].locked);
    CHECK(ui.layers[2].visible);
    CHECK_FALSE(ui.layers[2].locked);
}

TEST_CASE("Layer list: dragging down the visibility toggles paints them, Alt+click shows one layer only")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    LayerUi ui;
    session.SetUi([&] { ui.Draw(session); });
    session.Frames(2);
    const ImRect r = session.ItemRect("layers");
    session.Drag({LayerToggleAt(r, 0, 0), LayerToggleAt(r, 1, 0), LayerToggleAt(r, 2, 0)}, 4);
    CHECK_FALSE(ui.layers[0].visible);
    CHECK_FALSE(ui.layers[1].visible);
    CHECK_FALSE(ui.layers[2].visible);
    CHECK(ui.layers[3].visible);
    CHECK(ui.selected == 0);  // the toggles leave the selection alone
    session.KeyDown(ImGuiMod_Alt);
    session.Click(LayerToggleAt(r, 2, 0));
    session.KeyUp(ImGuiMod_Alt);
    CHECK(ui.layers[2].visible);
    CHECK_FALSE(ui.layers[0].visible);
    CHECK_FALSE(ui.layers[3].visible);
}

TEST_CASE("Layer list: Alt+Down and a drag move a row and the selection follows")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    LayerUi ui;
    session.SetUi([&] { ui.Draw(session); });
    session.Frames(2);
    const ImRect r = session.ItemRect("layers");
    session.Click(LayerRowAt(r, 0));
    facet::internal::SetKeyboardModality(true);
    session.KeyPress(ImGuiKey_DownArrow | ImGuiMod_Alt);
    CHECK(ui.last.action == facet::LayerAction::Moved);
    CHECK(ui.last.move.from == 0);
    CHECK(ui.last.move.to == 1);
    CHECK(std::string(ui.layers[1].label) == "Layer 1");
    CHECK(ui.selected == 1);
    // A pointer drag of the last row to the top.
    session.Drag({LayerRowAt(r, 3), LayerRowAt(r, 2), LayerRowAt(r, 1), ImVec2(LayerRowAt(r, 0).x, LayerRowAt(r, 0).y - LayerRowHeight() * 0.4f)}, 6);
    session.Frames(2);
    CHECK(std::string(ui.layers[0].label) == "Layer 4");
    CHECK(ui.selected == 2);  // still on "Layer 1"
    CHECK(std::string(ui.layers[ui.selected].label) == "Layer 1");
}

TEST_CASE("Layer list: read-only rows select but toggle and move nothing; the data states show")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    LayerUi ui;
    ui.options.readOnly = true;
    session.SetUi([&] { ui.Draw(session); });
    session.Frames(2);
    const ImRect r = session.ItemRect("layers");
    session.Click(LayerToggleAt(r, 1, 0));
    session.Drag({LayerRowAt(r, 3), LayerRowAt(r, 0)}, 6);
    CHECK(ui.layers[1].visible);
    CHECK(std::string(ui.layers[3].label) == "Layer 4");
    session.Click(LayerRowAt(r, 2));
    CHECK(ui.selected == 2);

    ui.options.readOnly = false;
    ui.options.state = facet::DataState::Loading;
    session.Frames(2);
    const float loading = session.ItemRect("layers").GetHeight();
    CHECK(loading > 0.0f);
    const int actions = ui.actions;
    session.Click(session.ItemCenter("layers"));
    CHECK(ui.actions == actions);
}

TEST_CASE("Layer list: a shown list allocates nothing per frame")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    LayerUi ui;
    session.SetUi([&] { ui.Draw(session); });
    session.Frames(10);
    session.MouseMove(LayerRowAt(session.ItemRect("layers"), 1));
    session.Frames(10);
    const int states = facet::internal::GlobalStateStore().Count();
    CHECK(CountImGuiAllocations(session, 10) == 0);
    CHECK(facet::internal::GlobalStateStore().Count() == states);
}

// ---- PreviewCanvas and CanvasElement -----------------------------------------------------------------

namespace
{

struct CanvasUi
{
    ImVec2 reference = {1000.0f, 500.0f};
    facet::CanvasElementState element{facet::CanvasAnchor::TopLeft, {100.0f, 100.0f}, {200.0f, 100.0f}};
    facet::CanvasElementState other{facet::CanvasAnchor::TopLeft, {600.0f, 300.0f}, {100.0f, 100.0f}};
    facet::CanvasElementOptions options{.resizable = true};
    int writes = 0;
    ImGuiID id = 0;

    void Draw(facet::test::Session& session)
    {
        facet::BeginPreviewCanvas("canvas", reference, {.width = 500.0f});
        id = ImGui::GetID("Element");
        writes += facet::CanvasElement("Element", &element, options) ? 1 : 0;
        session.Track("element");
        facet::CanvasElement("Other", &other, {.snap = false});
        facet::EndPreviewCanvas();
        session.Track("canvas");
    }

    ImRect Part(int part) const { return facet::internal::CanvasElementPartRect(id, part); }
};

} // namespace

TEST_CASE("Preview canvas: the screen edge is drawn in the control boundary color")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    const ImU32 boundary = facet::ToU32(facet::ControlBoundaryColor(facet::T().colors));
    const ImU32 separator = facet::ToU32(facet::T().colors.separator);
    REQUIRE(boundary != separator);
    int boundaryVertices = 0;
    session.SetUi([&] {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const int from = drawList->VtxBuffer.Size;
        facet::BeginPreviewCanvas("edge canvas", {1000.0f, 500.0f}, {.gridSpacing = 100.0f, .width = 500.0f});
        facet::EndPreviewCanvas();
        boundaryVertices = 0;
        for (int i = from; i < drawList->VtxBuffer.Size; ++i)
            boundaryVertices += drawList->VtxBuffer[i].col == boundary ? 1 : 0;
    });
    session.Frames(2);
    // A canvas without elements: only the screen edge uses the boundary color.
    CHECK(boundaryVertices > 0);
}

TEST_CASE("Canvas element: a drag moves it in reference pixels and snaps to the canvas center")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    CanvasUi ui;
    session.SetUi([&] { ui.Draw(session); });
    session.Frames(2);
    const float scale = session.ItemRect("element").GetWidth() / 200.0f;  // pixels per reference pixel
    REQUIRE(scale > 0.0f);
    const ImVec2 from = ui.Part(0).GetCenter();
    // Alt held: no snapping; 40 screen pixels are 40 / scale reference pixels.
    session.KeyDown(ImGuiMod_Alt);
    session.Drag({from, ImVec2(from.x + 40.0f, from.y)}, 4);
    session.KeyUp(ImGuiMod_Alt);
    CHECK(ui.element.offset.x == doctest::Approx(100.0f + 40.0f / scale).epsilon(0.01));
    CHECK(ui.element.offset.y == doctest::Approx(100.0f));
    CHECK(ui.writes > 0);
    // Near the center: the element's center comes onto the canvas center (500, 250).
    const ImVec2 at = ui.Part(0).GetCenter();
    const ImVec2 to(ui.Part(0).GetCenter().x + (500.0f - (ui.element.offset.x + 100.0f)) * scale + 2.0f,
                    ui.Part(0).GetCenter().y + (250.0f - 150.0f) * scale - 2.0f);
    session.Drag({at, to}, 4);
    CHECK(ui.element.offset.x + 100.0f == doctest::Approx(500.0f));
    CHECK(ui.element.offset.y + 50.0f == doctest::Approx(250.0f));
}

TEST_CASE("Canvas element: the arrows nudge it, Shift by ten, and Ctrl resizes it")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    CanvasUi ui;
    session.SetUi([&] { ui.Draw(session); });
    session.Frames(2);
    ShowFocus(ui.id);
    facet::internal::SetKeyboardModality(true);
    session.Frames(1);
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(ui.element.offset.x == 101.0f);
    session.KeyPress(ImGuiKey_DownArrow | ImGuiMod_Shift);
    CHECK(ui.element.offset.y == 110.0f);
    session.KeyPress(ImGuiKey_RightArrow | ImGuiMod_Ctrl);
    CHECK(ui.element.extent.x == 201.0f);
    CHECK(ui.element.offset.x == 101.0f);
}

TEST_CASE("Canvas element: a handle resizes it and the anchor control keeps its place")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    CanvasUi ui;
    session.SetUi([&] { ui.Draw(session); });
    session.Frames(2);
    session.Click(ui.Part(0).GetCenter());  // selects it: the handles and the anchor control show
    session.Frames(1);
    const ImRect handle = ui.Part(5);  // bottom-right
    REQUIRE(handle.GetWidth() > 0.0f);
    const float scale = session.ItemRect("element").GetWidth() / 200.0f;
    session.KeyDown(ImGuiMod_Alt);
    session.Drag({handle.GetCenter(), ImVec2(handle.GetCenter().x + 20.0f, handle.GetCenter().y + 10.0f)}, 4);
    session.KeyUp(ImGuiMod_Alt);
    CHECK(ui.element.extent.x == doctest::Approx(200.0f + 20.0f / scale).epsilon(0.01));
    CHECK(ui.element.extent.y == doctest::Approx(100.0f + 10.0f / scale).epsilon(0.01));
    CHECK(ui.element.offset.x == doctest::Approx(100.0f));
    // The center cell of the anchor control: the anchor changes, the element stays where it is.
    const ImRect cell = ui.Part(9 + static_cast<int>(facet::CanvasAnchor::Center));
    REQUIRE(cell.GetWidth() > 0.0f);
    const ImRect before = ui.Part(0);
    session.Click(cell.GetCenter());
    session.Frames(1);
    CHECK(ui.element.anchor == facet::CanvasAnchor::Center);
    CHECK(ui.Part(0).Min.x == doctest::Approx(before.Min.x).epsilon(0.001));
    CHECK(ui.Part(0).Min.y == doctest::Approx(before.Min.y).epsilon(0.001));
}

TEST_CASE("Canvas element: one larger than the canvas moves, Escape puts a drag back, read-only writes nothing")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    CanvasUi ui;
    ui.element.extent = {1500.0f, 800.0f};
    session.SetUi([&] { ui.Draw(session); });
    session.Frames(2);
    const ImRect canvas = session.ItemRect("canvas");
    CHECK(canvas.GetHeight() < ui.Part(0).GetHeight());  // the element is larger than the canvas
    const ImVec2 from = canvas.GetCenter();
    session.MouseMove(from);
    session.MouseDown();
    session.MouseMove(ImVec2(from.x - 30.0f, from.y));
    session.MouseMove(ImVec2(from.x - 60.0f, from.y));
    session.KeyPress(ImGuiKey_Escape);
    session.MouseUp();
    CHECK(ui.element.offset.x == 100.0f);
    ui.options.readOnly = true;
    session.Drag({from, ImVec2(from.x - 60.0f, from.y)}, 4);
    CHECK(ui.element.offset.x == 100.0f);
}

TEST_CASE("Canvas element: Escape writes back the state the drag began with, exactly")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    CanvasUi ui;
    ui.reference = {1920.0f, 1080.0f};
    // Values the anchor arithmetic does not reproduce to the last bit.
    const facet::CanvasElementState start{facet::CanvasAnchor::Center, {149.556335f, -199.394348f}, {751.422424f, 405.211304f}};
    ui.element = start;
    session.SetUi([&] { ui.Draw(session); });
    session.Frames(2);
    const ImVec2 from = ui.Part(0).GetCenter();
    session.KeyDown(ImGuiMod_Alt);
    session.MouseMove(from);
    session.MouseDown();
    session.MouseMove(ImVec2(from.x - 23.0f, from.y + 7.0f));
    session.MouseMove(ImVec2(from.x - 41.0f, from.y + 13.0f));
    REQUIRE(ui.element.offset.x != start.offset.x);
    session.KeyPress(ImGuiKey_Escape);
    session.MouseUp();
    session.KeyUp(ImGuiMod_Alt);
    CHECK(ui.element.anchor == start.anchor);
    CHECK(ui.element.offset.x == start.offset.x);
    CHECK(ui.element.offset.y == start.offset.y);
    CHECK(ui.element.extent.x == start.extent.x);
    CHECK(ui.element.extent.y == start.extent.y);
}

TEST_CASE("Preview canvas: an empty reference size draws an empty canvas without elements")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    CanvasUi ui;
    ui.reference = {0.0f, 100.0f};
    session.SetUi([&] { ui.Draw(session); });
    session.Frames(2);
    CHECK(session.ItemRect("canvas").GetHeight() > 0.0f);
    CHECK(ui.Part(0).GetWidth() == 0.0f);
}

TEST_CASE("Preview canvas: a shown canvas allocates nothing per frame")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    CanvasUi ui;
    session.SetUi([&] { ui.Draw(session); });
    session.Frames(10);
    session.Click(ui.Part(0).GetCenter());
    session.Frames(10);
    const int states = facet::internal::GlobalStateStore().Count();
    CHECK(CountImGuiAllocations(session, 10) == 0);
    CHECK(facet::internal::GlobalStateStore().Count() == states);
}
