// Multi-handle model (src/internal/multi_handle.h): constraints, picking, draw order, and the
// items of a widget with several handles.

#include <array>
#include <cmath>
#include <limits>
#include <vector>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "internal/multi_handle.h"
#include "support/session.h"

using facet::internal::CollisionMode;
using facet::internal::ConstrainHandles;
using facet::internal::EffectiveGap;
using facet::internal::HandleConstraints;
using facet::internal::HandleDrawOrder;
using facet::internal::InsertHandle;
using facet::internal::MoveHandle;
using facet::internal::MoveSpan;
using facet::internal::PickHandle;
using facet::internal::PickHandle2D;
using facet::internal::RemoveHandle;

namespace
{

HandleConstraints Range(double gap, CollisionMode mode = CollisionMode::Clamp)
{
    HandleConstraints c;
    c.min = 0.0;
    c.max = 100.0;
    c.minGap = gap;
    c.collision = mode;
    return c;
}

} // namespace

// ---- Constraints ------------------------------------------------------------------------------

TEST_CASE("Multi-handle: clamp mode stops a handle one gap before its neighbour")
{
    std::array<double, 2> v = {20.0, 60.0};
    const HandleConstraints c = Range(10.0);
    CHECK(MoveHandle(v, 0, 55.0, c));
    CHECK(v[0] == 50.0);
    CHECK(v[1] == 60.0);
    CHECK_FALSE(MoveHandle(v, 0, 90.0, c));  // already at the limit
    CHECK_FALSE(MoveHandle(v, 1, 30.0, c));
    CHECK(v[1] == 60.0);  // cannot pass 50 + 10
    CHECK(MoveHandle(v, 1, 200.0, c));
    CHECK(v[1] == 100.0);       // the range end
    CHECK(MoveHandle(v, 0, -5.0, c));
    CHECK(v[0] == 0.0);
    // Never crossing, whatever the target.
    for (double target = -50.0; target <= 150.0; target += 7.0)
    {
        MoveHandle(v, static_cast<int>(target) % 2 == 0 ? 0 : 1, target, c);
        CHECK(v[1] - v[0] >= 10.0 - 1e-12);
        CHECK(v[0] >= 0.0);
        CHECK(v[1] <= 100.0);
    }
}

TEST_CASE("Multi-handle: push mode moves the neighbours along until the end")
{
    std::array<double, 3> v = {10.0, 30.0, 50.0};
    const HandleConstraints c = Range(5.0, CollisionMode::Push);
    CHECK(MoveHandle(v, 0, 40.0, c));
    CHECK(v[0] == 40.0);
    CHECK(v[1] == 45.0);
    CHECK(v[2] == 50.0);
    CHECK(MoveHandle(v, 0, 48.0, c));
    CHECK(v[1] == 53.0);
    CHECK(v[2] == 58.0);
    // At the end the whole run stops: the pushed handles keep the gap.
    CHECK(MoveHandle(v, 0, 1000.0, c));
    CHECK(v[0] == 90.0);
    CHECK(v[1] == 95.0);
    CHECK(v[2] == 100.0);
    // Pushing down from the top handle.
    CHECK(MoveHandle(v, 2, -1000.0, c));
    CHECK(v[0] == 0.0);
    CHECK(v[1] == 5.0);
    CHECK(v[2] == 10.0);
}

TEST_CASE("Multi-handle: the minimum gap is clamped to what the range can hold")
{
    CHECK(EffectiveGap(Range(10.0), 2) == 10.0);
    CHECK(EffectiveGap(Range(80.0), 3) == 50.0);
    CHECK(EffectiveGap(Range(-3.0), 3) == 0.0);
    CHECK(EffectiveGap(Range(std::numeric_limits<double>::quiet_NaN()), 2) == 0.0);
    CHECK(EffectiveGap(Range(10.0), 1) == 0.0);
    // A gap of the whole range pins two handles to the ends.
    std::array<double, 2> v = {40.0, 60.0};
    ConstrainHandles(v, Range(500.0));
    CHECK(v[0] == 0.0);
    CHECK(v[1] == 100.0);
    CHECK_FALSE(MoveHandle(v, 0, 50.0, Range(500.0)));
}

TEST_CASE("Multi-handle: invalid values are repaired with the least movement")
{
    std::array<double, 3> v = {70.0, std::numeric_limits<double>::quiet_NaN(), -20.0};
    ConstrainHandles(v, Range(10.0));
    CHECK(v[0] <= v[1] - 10.0);
    CHECK(v[1] <= v[2] - 10.0);
    CHECK(v[0] >= 0.0);
    CHECK(v[2] <= 100.0);
    std::array<double, 2> fine = {30.0, 70.0};
    ConstrainHandles(fine, Range(10.0));
    CHECK(fine[0] == 30.0);
    CHECK(fine[1] == 70.0);
}

TEST_CASE("Multi-handle: mirrored handles stay symmetric around the center")
{
    HandleConstraints c = Range(10.0);
    c.mirror = true;
    c.center = 50.0;
    std::array<double, 2> v = {30.0, 70.0};
    CHECK(MoveHandle(v, 0, 20.0, c));
    CHECK(v[0] == 20.0);
    CHECK(v[1] == 80.0);
    CHECK(MoveHandle(v, 1, 52.0, c));
    CHECK(v[1] == 55.0);  // half the gap from the center
    CHECK(v[0] == 45.0);
    CHECK(MoveHandle(v, 0, -40.0, c));
    CHECK(v[0] == 0.0);
    CHECK(v[1] == 100.0);
    // An off-center center limits the pair by the nearer end.
    c.center = 80.0;
    CHECK(MoveHandle(v, 1, 200.0, c));
    CHECK(v[1] == 100.0);
    CHECK(v[0] == 60.0);
    // The span cannot be dragged in mirror mode.
    CHECK_FALSE(MoveSpan(v, 0, 1, 5.0, c));
}

TEST_CASE("Multi-handle: a span keeps its distances and stops at the limits")
{
    std::array<double, 2> v = {20.0, 50.0};
    const HandleConstraints c = Range(5.0);
    CHECK(MoveSpan(v, 0, 1, 10.0, c));
    CHECK(v[0] == 30.0);
    CHECK(v[1] == 60.0);
    CHECK(MoveSpan(v, 0, 1, 500.0, c));
    CHECK(v[1] == 100.0);
    CHECK(v[0] == 70.0);
    CHECK_FALSE(MoveSpan(v, 0, 1, 1.0, c));
    CHECK(MoveSpan(v, 0, 1, -500.0, c));
    CHECK(v[0] == 0.0);
    CHECK(v[1] == 30.0);
    // A run inside three handles stops one gap before the outside neighbour.
    std::array<double, 3> three = {10.0, 40.0, 60.0};
    CHECK(MoveSpan(three, 0, 1, 50.0, c));
    CHECK(three[1] == 55.0);
    CHECK(three[0] == 25.0);
    CHECK(three[2] == 60.0);
}

TEST_CASE("Multi-handle: handles are added and removed in order")
{
    std::array<double, 5> storage = {10.0, 50.0, 90.0, 0.0, 0.0};
    const HandleConstraints c = Range(5.0);
    CHECK(InsertHandle(storage, 3, 30.0, c) == 1);
    CHECK(storage[0] == 10.0);
    CHECK(storage[1] == 30.0);
    CHECK(storage[2] == 50.0);
    CHECK(storage[3] == 90.0);
    CHECK(InsertHandle(storage, 4, 52.0, c) == -1);  // closer than the gap
    CHECK(InsertHandle(storage, 4, 100.0, c) == 4);
    CHECK(InsertHandle(storage, 5, 70.0, c) == -1);  // full
    CHECK(RemoveHandle(storage, 5, 1) == 4);
    CHECK(storage[1] == 50.0);
    CHECK(RemoveHandle(storage, 4, 9) == 4);
    std::array<double, 1> one = {5.0};
    CHECK(RemoveHandle(one, 1, 0, 1) == 1);  // keeps the minimum count
}

// ---- Picking ----------------------------------------------------------------------------------

TEST_CASE("Multi-handle: picking takes the nearest handle")
{
    const float positions[] = {20.0f, 80.0f, 150.0f};
    CHECK(PickHandle(positions, 0.0f, 0.0f, 200.0f) == 0);
    CHECK(PickHandle(positions, 60.0f, 0.0f, 200.0f) == 1);
    CHECK(PickHandle(positions, 140.0f, 0.0f, 200.0f) == 2);
    CHECK(PickHandle({}, 10.0f, 0.0f, 200.0f) == -1);
}

TEST_CASE("Multi-handle: overlapping handles give the press to the one that can move")
{
    // Both at the end of the track: only the first can still move.
    const float atEnd[] = {200.0f, 200.0f};
    CHECK(PickHandle(atEnd, 200.0f, 0.0f, 200.0f) == 0);
    CHECK(PickHandle(atEnd, 190.0f, 0.0f, 200.0f) == 0);
    // Both at the start: the last one.
    const float atStart[] = {0.0f, 0.0f};
    CHECK(PickHandle(atStart, 0.0f, 0.0f, 200.0f) == 1);
    CHECK(PickHandle(atStart, 10.0f, 0.0f, 200.0f) == 1);
    // In the middle: the direction of the pointer decides, then the focused handle.
    const float middle[] = {100.0f, 100.0f, 100.0f};
    CHECK(PickHandle(middle, 120.0f, 0.0f, 200.0f) == 2);
    CHECK(PickHandle(middle, 80.0f, 0.0f, 200.0f) == 0);
    CHECK(PickHandle(middle, 100.0f, 0.0f, 200.0f, 1) == 1);
    CHECK(PickHandle(middle, 100.0f, 0.0f, 200.0f) == 2);

    // The draw order puts the same handle on top.
    int order[2] = {};
    const std::array<double, 2> endValues = {100.0, 100.0};
    HandleDrawOrder(endValues, Range(0.0), -1, order);
    CHECK(order[1] == 0);
    const std::array<double, 2> startValues = {0.0, 0.0};
    HandleDrawOrder(startValues, Range(0.0), -1, order);
    CHECK(order[1] == 1);
    const std::array<double, 2> apart = {30.0, 60.0};
    HandleDrawOrder(apart, Range(0.0), 0, order);
    CHECK(order[0] == 1);
    CHECK(order[1] == 0);  // the active or focused handle on top
}

TEST_CASE("Multi-handle: picking on a plane for two-dimensional pads")
{
    const ImVec2 corners[] = {ImVec2(10.0f, 10.0f), ImVec2(90.0f, 10.0f), ImVec2(10.0f, 90.0f), ImVec2(90.0f, 90.0f)};
    CHECK(PickHandle2D(corners, ImVec2(80.0f, 85.0f), 20.0f) == 3);
    CHECK(PickHandle2D(corners, ImVec2(50.0f, 50.0f), 20.0f) == -1);
    const ImVec2 same[] = {ImVec2(40.0f, 40.0f), ImVec2(40.0f, 40.0f)};
    CHECK(PickHandle2D(same, ImVec2(41.0f, 40.0f), 10.0f) == 1);  // the later one is on top
}

// ---- Behaviour --------------------------------------------------------------------------------

namespace
{

// A range slider made of the blocks: 300 x 24 pixel widget, knob centres from its left to its
// right edge, values 0 to 100.
struct TestRange
{
    double values[3] = {20.0, 60.0, 90.0};
    int count = 2;
    facet::internal::MultiHandleOptions options;
    facet::internal::MultiHandleResult last;
    facet::internal::ItemState handles[3];
    int commits = 0;
    int cancels = 0;
    bool widgetHovered = false;
    ImGuiID lastItemId = 0;
    ImGuiID widgetId = 0;

    TestRange()
    {
        options.drag.mapping = {.min = 0.0, .max = 100.0};
        options.constraints.minGap = 10.0;
        options.handleSize = 16.0f;
    }

    void Ui(facet::test::Session& session)
    {
        const ImGuiID id = ImGui::GetID("range");
        widgetId = id;
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const ImRect rect(pos, ImVec2(pos.x + 300.0f, pos.y + 24.0f));
        ImGui::BeginGroup();
        last = facet::internal::MultiHandleBehavior(id, rect, rect, std::span<double>(values, static_cast<size_t>(count)),
                                                    std::span<facet::internal::ItemState>(handles, static_cast<size_t>(count)),
                                                    options);
        ImGui::EndGroup();
        facet::internal::FinishWidgetGroup(id);
        commits += facet::IsItemEditCommitted() ? 1 : 0;
        cancels += facet::IsItemEditCanceled() ? 1 : 0;
        widgetHovered = ImGui::IsItemHovered();
        lastItemId = ImGui::GetItemID();
        session.Track("range");
    }

    static ImVec2 At(const facet::test::Session& session, double value)
    {
        const ImRect r = session.ItemRect("range");
        return ImVec2(r.Min.x + static_cast<float>(value / 100.0) * r.GetWidth(), r.GetCenter().y);
    }
};

void Install(facet::test::Session& session, TestRange& range)
{
    session.SetUi([&] { range.Ui(session); });
    session.Frames(2);
}

} // namespace

TEST_CASE("Multi-handle behaviour: dragging a handle moves only that handle, clamped")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestRange range;
    Install(session, range);
    session.Drag({TestRange::At(session, 20.0), TestRange::At(session, 40.0)});
    CHECK(range.values[0] == doctest::Approx(40.0));
    CHECK(range.values[1] == 60.0);
    CHECK(range.commits == 1);
    // Into the other handle: stops one gap before it.
    session.Drag({TestRange::At(session, 40.0), TestRange::At(session, 95.0)});
    CHECK(range.values[0] == doctest::Approx(50.0));
    CHECK(range.values[1] == 60.0);
}

TEST_CASE("Multi-handle behaviour: a handle held by its neighbour edits nothing")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestRange range;
    range.values[0] = 50.0;
    range.values[1] = 60.0;
    Install(session, range);
    int changedFrames = 0;
    session.SetUi([&] {
        range.Ui(session);
        changedFrames += range.last.changed ? 1 : 0;
    });
    session.Drag({TestRange::At(session, 50.0), TestRange::At(session, 80.0)});
    CHECK(range.values[0] == 50.0);
    CHECK(changedFrames == 0);
    CHECK(range.commits == 0);
    // The arrow key against the neighbour neither.
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(range.values[0] == 50.0);
    CHECK(range.commits == 0);
}

TEST_CASE("Multi-handle behaviour: stacked handles separate in the direction of the drag")
{
    for (const float side : {-3.0f, 3.0f})
    {
        for (const double target : {10.0, 90.0})
        {
            facet::test::Session session;
            REQUIRE(session.Ready());
            TestRange range;
            range.values[0] = 50.0;
            range.values[1] = 50.0;
            range.options.constraints.minGap = 0.0;
            range.options.spanDrag = false;
            Install(session, range);
            const ImVec2 stack = TestRange::At(session, 50.0);
            INFO("press offset " << side << ", drag to " << target);
            session.Drag({ImVec2(stack.x + side, stack.y), TestRange::At(session, target + side / 3.0)});
            if (target < 50.0)
            {
                CHECK(range.values[0] == doctest::Approx(target).epsilon(0.02));
                CHECK(range.values[1] == 50.0);
            }
            else
            {
                CHECK(range.values[0] == 50.0);
                CHECK(range.values[1] == doctest::Approx(target).epsilon(0.02));
            }
            CHECK(range.commits == 1);
        }
    }
}

TEST_CASE("Multi-handle behaviour: a click on stacked handles focuses one without moving them")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestRange range;
    range.values[0] = 50.0;
    range.values[1] = 50.0;
    range.options.constraints.minGap = 0.0;
    Install(session, range);
    session.Click(TestRange::At(session, 50.0));
    CHECK(range.values[0] == 50.0);
    CHECK(range.values[1] == 50.0);
    CHECK((ImGui::GetFocusID() == range.handles[0].id || ImGui::GetFocusID() == range.handles[1].id));
    CHECK(range.commits == 0);
}

TEST_CASE("Multi-handle behaviour: push mode pushes the other handle")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestRange range;
    range.options.constraints.collision = CollisionMode::Push;
    Install(session, range);
    session.Drag({TestRange::At(session, 20.0), TestRange::At(session, 80.0)});
    CHECK(range.values[0] == doctest::Approx(80.0));
    CHECK(range.values[1] == doctest::Approx(90.0));
    session.Drag({TestRange::At(session, 80.0), TestRange::At(session, 100.0)});
    CHECK(range.values[0] == doctest::Approx(90.0));
    CHECK(range.values[1] == 100.0);
}

TEST_CASE("Multi-handle behaviour: a track click moves the nearest handle")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestRange range;
    range.options.spanDrag = false;
    Install(session, range);
    session.Click(TestRange::At(session, 70.0));
    CHECK(range.values[0] == 20.0);
    CHECK(range.values[1] == doctest::Approx(70.0));
    CHECK(ImGui::GetFocusID() == range.handles[1].id);
    session.Click(TestRange::At(session, 5.0));
    CHECK(range.values[0] == doctest::Approx(5.0));
    CHECK(range.commits == 2);
}

TEST_CASE("Multi-handle behaviour: dragging the span moves both and keeps the distance")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestRange range;
    Install(session, range);
    session.Drag({TestRange::At(session, 40.0), TestRange::At(session, 50.0)});
    CHECK(range.values[0] == doctest::Approx(30.0));
    CHECK(range.values[1] == doctest::Approx(70.0));
    // To the end and beyond: the span stops with its width kept.
    session.Drag({TestRange::At(session, 50.0), TestRange::At(session, 150.0)});
    CHECK(range.values[1] == 100.0);
    CHECK(range.values[0] == doctest::Approx(60.0));
    CHECK(range.commits == 2);
}

TEST_CASE("Multi-handle behaviour: a span drag on a logarithmic track keeps the minimum gap")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestRange range;
    range.options.drag.mapping = {.min = 1.0, .max = 1000.0, .scale = facet::ValueScale::Logarithmic};
    range.options.constraints.minGap = 150.0;
    range.values[0] = 100.0;
    range.values[1] = 300.0;
    Install(session, range);
    const ImRect r = session.ItemRect("range");
    const auto at = [&](double value) {
        const double t = std::log(value) / std::log(1000.0);
        return ImVec2(r.Min.x + static_cast<float>(t) * r.GetWidth(), r.GetCenter().y);
    };
    // Grab the span between the handles and drag far toward min: the gap shrinks in value units
    // as the span moves down, so the span stops where it reaches 150.
    const ImVec2 middle = at(std::sqrt(100.0 * 300.0));
    double smallestWritten = 1e9;  // the smallest gap the caller ever received with changed set
    session.SetUi([&] {
        range.Ui(session);
        if (range.last.changed)
            smallestWritten = std::min(smallestWritten, range.values[1] - range.values[0]);
    });
    session.Drag({middle, ImVec2(middle.x - 120.0f, middle.y)});
    CHECK(smallestWritten >= 150.0 - 1e-6);
    CHECK(range.values[1] - range.values[0] >= 150.0 - 1e-6);
    CHECK(range.values[1] - range.values[0] == doctest::Approx(150.0).epsilon(0.01));
    CHECK(range.values[0] < 100.0);
    CHECK(range.values[1] / range.values[0] == doctest::Approx(3.0).epsilon(0.001));  // the same span on screen
    CHECK(range.commits == 1);
}

TEST_CASE("Multi-handle behaviour: Escape restores every handle")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestRange range;
    range.options.constraints.collision = CollisionMode::Push;
    Install(session, range);
    session.MouseMove(TestRange::At(session, 20.0));
    session.MouseDown();
    session.MouseMove(TestRange::At(session, 85.0));
    CHECK(range.values[1] == doctest::Approx(95.0));
    session.KeyPress(ImGuiKey_Escape);
    CHECK(range.values[0] == 20.0);
    CHECK(range.values[1] == 60.0);
    CHECK(range.cancels == 1);
    session.MouseUp();
    CHECK(range.commits == 0);

    // The span too.
    session.MouseMove(TestRange::At(session, 40.0));
    session.MouseDown();
    session.MouseMove(TestRange::At(session, 30.0));
    CHECK(range.values[0] == doctest::Approx(10.0));
    session.KeyPress(ImGuiKey_Escape);
    CHECK(range.values[0] == 20.0);
    CHECK(range.values[1] == 60.0);
    session.MouseUp();
    CHECK(range.cancels == 2);
}

TEST_CASE("Multi-handle behaviour: each handle is a Tab stop moved by the arrows")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestRange range;
    range.options.drag.mapping.step = 1.0;
    Install(session, range);
    session.KeyPress(ImGuiKey_Tab);
    // Tab from the window's first focus lands on a handle; find which.
    const ImGuiID first = range.handles[0].id;
    const ImGuiID second = range.handles[1].id;
    if (ImGui::GetFocusID() != first)
        session.KeyPress(ImGuiKey_Tab | ImGuiMod_Shift);
    REQUIRE(ImGui::GetFocusID() == first);
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(range.values[0] == 21.0);
    session.KeyPress(ImGuiKey_End);
    CHECK(range.values[0] == 50.0);  // clamped by the gap
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == second);
    session.KeyPress(ImGuiKey_LeftArrow);
    CHECK(range.values[1] == 60.0);  // clamped by the gap as well
    session.KeyPress(ImGuiKey_End);
    CHECK(range.values[1] == 100.0);
    CHECK(range.commits >= 3);
    // The span and the track are never focused.
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() != range.last.span.id);
    CHECK(ImGui::GetFocusID() != range.last.track.id);
}

TEST_CASE("Multi-handle behaviour: the item queries describe the whole widget")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestRange range;
    Install(session, range);
    session.MouseMove(TestRange::At(session, 50.0));
    CHECK(range.widgetHovered);
    CHECK(range.lastItemId == range.widgetId);
    // While a handle is dragged, the last item is that handle, so the activity queries hold.
    session.MouseMove(TestRange::At(session, 60.0));
    session.MouseDown();
    CHECK(range.lastItemId == range.handles[1].id);
    session.MouseUp();
    session.Frame();
    CHECK(range.lastItemId == range.widgetId);
}

TEST_CASE("Multi-handle behaviour: N handles, removal by key")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestRange range;
    range.count = 3;
    range.options.removeKeys = true;
    range.options.spanDrag = false;
    Install(session, range);
    session.Click(TestRange::At(session, 90.0));
    REQUIRE(ImGui::GetFocusID() == range.handles[2].id);
    int removed = -1;
    session.SetUi([&] {
        range.Ui(session);
        if (range.last.removeRequested >= 0)
            removed = range.last.removeRequested;
    });
    session.KeyPress(ImGuiKey_Delete);
    CHECK(removed == 2);
}
