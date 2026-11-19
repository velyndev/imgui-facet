// Drag and drop core (src/internal/drag_drop.h): where drops land and how rows move apart, the
// start rules, cancel, the results, interop with stock Dear ImGui sources and targets, the
// ghost, the keyboard path and edge scrolling, driven by scripted input.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "core/core_internal.h"
#include "internal/drag_drop.h"
#include "internal/item.h"
#include "support/session.h"

namespace fi = facet::internal;
using fi::DragDropEffect;
using fi::DragDropZoneResult;

namespace
{

constexpr const char* kRowType = "test.row";

struct RowPayload
{
    int list = -1;
    int index = -1;
};

// A vertical list of rows built on the item behaviour, each a drag source, with one ordered zone.
struct TestList
{
    int list = 0;
    std::vector<int> keys = {0, 1, 2, 3, 4};
    ImVec2 rowSize = ImVec2(200.0f, 20.0f);
    fi::DragDropZoneOptions options;
    const char* type = kRowType;
    int hidden = -1;           // a row index not submitted (a row that scrolled away)
    int focusKey = -1;         // the row with this key takes the keyboard focus once
    bool forceFirst = false;   // DragDropForceNextSource before row 0's source, every frame
    ImVec2 forceOffset = ImVec2(0.0f, 0.0f);
    bool disabled = false;
    bool readOnly = false;

    std::vector<ImRect> slots;
    ImRect rect;
    ImGuiID zoneId = 0;
    int lifted = -1;           // the row whose source reported a drag this frame
    int liftedFrames = 0;
    DragDropZoneResult drop;
    bool delivered = false;
    RowPayload deliveredPayload;
    int deliveredIndex = -1;
    DragDropEffect deliveredEffect = DragDropEffect::Move;
    bool deliveredKeyboard = false;
    bool everHovered = false;
    bool everRejected = false;
    bool everHoverOpen = false;
    bool active = false;       // DragDropActive at the end of the list, for this frame
    fi::DragDropInfo info;

    void Ui(facet::test::Session& session, const char* name)
    {
        ImGui::PushID(name);
        zoneId = ImGui::GetID("zone");
        slots.assign(keys.size(), ImRect());
        lifted = -1;
        const ImVec2 start = ImGui::GetCursorScreenPos();
        if (disabled)
            ImGui::BeginDisabled();
        for (int i = 0; i < static_cast<int>(keys.size()); ++i)
        {
            if (i == hidden)
            {
                ImGui::Dummy(rowSize);
                slots[i] = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
                continue;
            }
            ImGui::PushID(keys[i]);
            fi::ItemOptions itemOptions;
            itemOptions.readOnly = readOnly;
            const fi::ItemState row = fi::ItemBehavior(ImGui::GetID("row"), rowSize, itemOptions);
            slots[i] = row.rect;
            const std::string probe = std::string(name) + "." + std::to_string(i);
            session.Track(probe);
            if (focusKey == keys[i])
            {
                ImGui::SetFocusID(row.id, ImGui::GetCurrentWindow());
                focusKey = -1;
            }
            const RowPayload payload{list, i};
            if (forceFirst && i == 0)
                fi::DragDropForceNextSource(forceOffset);
            if (fi::DragDropBeginSource(kRowType, &payload, sizeof(payload), {.previewText = "Row"}))
            {
                lifted = i;
                ++liftedFrames;
                fi::DragDropEndSource();
            }
            ImGui::PopID();
        }
        if (disabled)
            ImGui::EndDisabled();
        const ImVec2 end = ImGui::GetCursorScreenPos();
        rect = ImRect(start, ImVec2(start.x + rowSize.x, end.y));
        fi::DragDropZoneOptions zoneOptions = options;
        zoneOptions.slots = slots;
        drop = fi::DragDropZone(zoneId, rect, type, zoneOptions);
        everHovered = everHovered || drop.hovered;
        everRejected = everRejected || drop.rejected;
        everHoverOpen = everHoverOpen || drop.hoverOpen;
        if (drop.delivered)
        {
            delivered = true;
            deliveredIndex = drop.insertIndex;
            deliveredEffect = drop.effect;
            deliveredKeyboard = drop.keyboard;
            REQUIRE(drop.payload != nullptr);
            REQUIRE(drop.payload->DataSize == static_cast<int>(sizeof(RowPayload)));
            deliveredPayload = *static_cast<const RowPayload*>(drop.payload->Data);
        }
        // Queries answer inside a frame only (rule 14), so the list keeps what they said.
        active = fi::DragDropActive(kRowType, &info);
        ImGui::PopID();
    }

    ImVec2 RowCenter(int index) const { return slots[index].GetCenter(); }
    // A point in the gap before row `index` (after the last row for keys.size()).
    ImVec2 GapPoint(int index) const
    {
        if (index >= static_cast<int>(slots.size()))
            return ImVec2(slots.back().GetCenter().x, slots.back().Max.y - 2.0f);
        return ImVec2(slots[index].GetCenter().x, slots[index].Min.y + 3.0f);
    }
};

// Applies a drop on list `to` as the caller of a reorderable list would.
void ApplyDrop(std::vector<int>& from, int source, std::vector<int>& to, int insertIndex, bool sameList)
{
    if (sameList)
    {
        facet::ApplyReorderMove(to, fi::DragDropReorderMove(source, insertIndex));
        return;
    }
    const int key = from[source];
    from.erase(from.begin() + source);
    to.insert(to.begin() + std::min<int>(insertIndex, static_cast<int>(to.size())), key);
}

struct ScaleGuard
{
    float ui = facet::UiScale();
    ~ScaleGuard() { facet::SetUiScale(ui); }
};

struct ThemeGuard
{
    facet::Theme theme = facet::T();
    ~ThemeGuard() { facet::SetTheme(theme, false); }
};

} // namespace

// ---- Ordered zones ------------------------------------------------------------------------------

TEST_CASE("DragDropInsertIndex picks the slot a point goes before")
{
    const ImRect slots[] = {ImRect(0, 0, 100, 20), ImRect(0, 24, 100, 44), ImRect(0, 48, 100, 68)};
    CHECK(fi::DragDropInsertIndex(slots, facet::Orientation::Vertical, ImVec2(50, -5)) == 0);
    CHECK(fi::DragDropInsertIndex(slots, facet::Orientation::Vertical, ImVec2(50, 9)) == 0);
    CHECK(fi::DragDropInsertIndex(slots, facet::Orientation::Vertical, ImVec2(50, 11)) == 1);
    CHECK(fi::DragDropInsertIndex(slots, facet::Orientation::Vertical, ImVec2(50, 57)) == 2);
    CHECK(fi::DragDropInsertIndex(slots, facet::Orientation::Vertical, ImVec2(50, 58)) == 3);
    CHECK(fi::DragDropInsertIndex(slots, facet::Orientation::Vertical, ImVec2(50, 59)) == 3);
    CHECK(fi::DragDropInsertIndex(slots, facet::Orientation::Vertical, ImVec2(50, 500)) == 3);
    const ImRect row[] = {ImRect(0, 0, 40, 20), ImRect(44, 0, 84, 20)};
    CHECK(fi::DragDropInsertIndex(row, facet::Orientation::Horizontal, ImVec2(30, 10)) == 1);
    CHECK(fi::DragDropInsertIndex(row, facet::Orientation::Horizontal, ImVec2(70, 10)) == 2);
    CHECK(fi::DragDropInsertIndex({}, facet::Orientation::Vertical, ImVec2(0, 0)) == -1);
}

TEST_CASE("DragDropReorderMove turns a gap index into the move of a reorderable list")
{
    CHECK(fi::DragDropReorderMove(1, 4).from == 1);
    CHECK(fi::DragDropReorderMove(1, 4).to == 3);
    CHECK(fi::DragDropReorderMove(3, 0).to == 0);
    CHECK_FALSE(fi::DragDropReorderMove(2, 2).IsValid());
    CHECK_FALSE(fi::DragDropReorderMove(2, 3).IsValid());
    CHECK_FALSE(fi::DragDropReorderMove(-1, 3).IsValid());
    CHECK_FALSE(fi::DragDropReorderMove(2, -1).IsValid());
}

// The visible order and positions while a drag hovers (other rows at layout plus gap shift, the
// dragged row in the gap) must be exactly the layout after the caller applies the reported drop,
// for every source and gap, rows of different heights included. That is what makes the list
// show the true result and keeps rows still when the data moves.
TEST_CASE("Every gap shown during a drag is the layout the reported drop gives")
{
    const std::vector<float> heights = {10.0f, 20.0f, 15.0f, 30.0f, 12.0f};
    const float spacing = 4.0f;
    const int count = static_cast<int>(heights.size());
    const auto layout = [&](const std::vector<int>& order) {
        std::vector<float> top(heights.size());
        float y = 100.0f;
        for (const int key : order)
        {
            top[static_cast<std::size_t>(key)] = y;
            y += heights[static_cast<std::size_t>(key)] + spacing;
        }
        return top;
    };
    std::vector<int> base(heights.size());
    for (int i = 0; i < count; ++i)
        base[static_cast<std::size_t>(i)] = i;
    const std::vector<float> before = layout(base);
    std::vector<ImRect> slots;
    for (int i = 0; i < count; ++i)
        slots.emplace_back(0.0f, before[i], 50.0f, before[i] + heights[i]);

    // Within the list.
    for (int source = 0; source < count; ++source)
    {
        for (int insert = 0; insert <= count; ++insert)
        {
            CAPTURE(source);
            CAPTURE(insert);
            std::vector<int> model = base;
            facet::ApplyReorderMove(model, fi::DragDropReorderMove(source, insert));
            const std::vector<float> after = layout(model);
            const float extent = heights[source] + spacing;
            for (int i = 0; i < count; ++i)
            {
                if (i == source)
                    continue;
                CHECK(before[i] + fi::DragDropGapShift(i, source, insert, extent) == doctest::Approx(after[i]));
            }
            const ImRect gap = fi::DragDropGapRect(slots, source, insert, facet::Orientation::Vertical, heights[source]);
            CHECK(gap.Min.y == doctest::Approx(after[source]));
            CHECK(gap.GetHeight() == doctest::Approx(heights[source]));
        }
    }

    // From another list: an item of height 18 lands before `insert`.
    const float incoming = 18.0f;
    for (int insert = 0; insert <= count; ++insert)
    {
        CAPTURE(insert);
        const float extent = incoming + spacing;
        for (int i = 0; i < count; ++i)
        {
            const float expected = before[i] + (i >= insert ? extent : 0.0f);
            CHECK(before[i] + fi::DragDropGapShift(i, -1, insert, extent) == doctest::Approx(expected));
        }
        const ImRect gap = fi::DragDropGapRect(slots, -1, insert, facet::Orientation::Vertical, incoming);
        const float expectedTop = insert < count ? before[insert] : before[count - 1] + heights[count - 1] + spacing;
        CHECK(gap.Min.y == doctest::Approx(expectedTop));
    }
    // Nothing hovers: nothing moves, the dragged row's place stays open.
    CHECK(fi::DragDropGapShift(3, 1, -1, 20.0f) == 0.0f);
}

TEST_CASE("In a wrapped grid a drop lands in the line under the point")
{
    // Three slots per line, two lines and one more slot: a swatch grid along the horizontal axis.
    std::vector<ImRect> slots;
    for (int i = 0; i < 7; ++i)
    {
        const float x = static_cast<float>(i % 3) * 40.0f;
        const float y = static_cast<float>(i / 3) * 40.0f;
        slots.emplace_back(x, y, x + 32.0f, y + 32.0f);
    }
    const auto at = [&](float x, float y) {
        return fi::DragDropInsertIndexWrapped(slots, facet::Orientation::Horizontal, ImVec2(x, y));
    };
    CHECK(at(5.0f, 10.0f) == 0);
    CHECK(at(20.0f, 10.0f) == 1);
    CHECK(at(100.0f, 10.0f) == 3);   // past the end of the first line: before the second line's first
    CHECK(at(5.0f, 50.0f) == 3);
    CHECK(at(60.0f, 50.0f) == 5);
    CHECK(at(20.0f, 90.0f) == 7);    // after the last slot of the last line
    CHECK(at(5.0f, 300.0f) == 6);    // below everything: the nearest line
    CHECK(fi::DragDropInsertIndexWrapped({}, facet::Orientation::Horizontal, ImVec2(0, 0)) == -1);

    // The slots entries are drawn in during a drag are the order the reported move gives.
    for (int source = 0; source < 7; ++source)
    {
        for (int insert = 0; insert <= 7; ++insert)
        {
            CAPTURE(source);
            CAPTURE(insert);
            std::vector<int> model = {0, 1, 2, 3, 4, 5, 6};
            facet::ApplyReorderMove(model, fi::DragDropReorderMove(source, insert));
            std::vector<int> shown(7, -1);
            for (int i = 0; i < 7; ++i)
            {
                if (i == source)
                    continue;
                const int slot = fi::DragDropGapSlot(i, source, insert);
                REQUIRE(slot >= 0);
                REQUIRE(slot < 7);
                shown[static_cast<std::size_t>(slot)] = i;
            }
            const int landing = insert > source ? insert - 1 : insert;
            shown[static_cast<std::size_t>(std::min(landing, 6))] = source;
            CHECK(shown == model);
        }
    }
    // From elsewhere, entries from the gap on move one slot on.
    CHECK(fi::DragDropGapSlot(2, -1, 2) == 3);
    CHECK(fi::DragDropGapSlot(1, -1, 2) == 1);
    CHECK(fi::DragDropGapSlot(4, 1, -1) == 4);
}

TEST_CASE("A zone of several types takes any of them and rejects the others")
{
    for (const char* type : {"test.a", "test.b", "test.c"})
    {
        CAPTURE(type);
        facet::test::Session session;
        REQUIRE(session.Ready());
        bool delivered = false;
        bool rejected = false;
        ImRect zoneRect;
        session.SetUi([&] {
            ImGui::Button("stock source", ImVec2(200.0f, 30.0f));
            session.Track("source");
            if (ImGui::BeginDragDropSource())
            {
                const int value = 1;
                ImGui::SetDragDropPayload(type, &value, sizeof(value));
                ImGui::EndDragDropSource();
            }
            const ImVec2 pos = ImGui::GetCursorScreenPos();
            zoneRect = ImRect(pos, ImVec2(pos.x + 200.0f, pos.y + 80.0f));
            ImGui::Dummy(zoneRect.GetSize());
            const char* const types[] = {"test.a", "test.b"};
            const DragDropZoneResult zone = fi::DragDropZone(ImGui::GetID("zone"), zoneRect, types);
            delivered = delivered || zone.delivered;
            rejected = rejected || zone.rejected;
        });
        session.Frame();
        session.DragAndHold({session.ItemCenter("source"), zoneRect.GetCenter()}, 8, 2);
        const bool accepted = std::string(type) != "test.c";
        CHECK(delivered == accepted);
        CHECK(rejected == !accepted);
    }
}

TEST_CASE("The insertion line and the into part of a tree row")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    const ImRect slots[] = {ImRect(0, 0, 100, 20), ImRect(0, 24, 100, 44)};
    const ImRect line = fi::DragDropInsertLineRect(slots, 1, facet::Orientation::Vertical);
    CHECK(line.GetCenter().y == doctest::Approx(22.0f));
    CHECK(line.GetHeight() == doctest::Approx(facet::Px(facet::T().metrics.focusRingWidth)));
    CHECK(line.Min.x == 0.0f);
    CHECK(line.Max.x == 100.0f);
    CHECK(fi::DragDropInsertLineRect(slots, 2, facet::Orientation::Vertical).GetCenter().y == doctest::Approx(44.0f));
    CHECK(fi::DragDropInsertLineRect({}, 0, facet::Orientation::Vertical).GetWidth() == 0.0f);
    const ImRect into = fi::DragDropIntoRect(ImRect(0, 0, 100, 40));
    CHECK(into.Min.y == 10.0f);
    CHECK(into.Max.y == 30.0f);
}

TEST_CASE("Entry positions spring the same at any frame rate and jump with reduced motion")
{
    const auto run = [](float dt) {
        facet::test::Session session({.deltaTime = dt});
        REQUIRE(session.Ready());
        float target = 0.0f;
        float value = 0.0f;
        session.SetUi([&] { value = fi::DragDropAnimatedPosition(7, 3, target); });
        session.Frame();
        target = 100.0f;
        session.Seconds(0.1f);
        return value;
    };
    const float slow = run(1.0f / 30.0f);
    const float fast = run(1.0f / 120.0f);
    CHECK(slow > 1.0f);
    CHECK(slow < 100.0f);
    CHECK(slow == doctest::Approx(fast).epsilon(0.02));

    ThemeGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    facet::Theme theme = facet::T();
    theme.motion.reduceMotion = true;
    facet::SetTheme(theme, false);
    float target = 0.0f;
    float value = 0.0f;
    session.SetUi([&] { value = fi::DragDropAnimatedPosition(7, 3, target); });
    session.Frame();
    target = 100.0f;
    session.Frame();
    CHECK(value == 100.0f);
}

// ---- Start rules --------------------------------------------------------------------------------

TEST_CASE("A click never starts a drag")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestList list;
    bool everActive = false;
    session.SetUi([&] {
        list.Ui(session, "a");
        everActive = everActive || fi::DragDropActive(nullptr, nullptr);
    });
    session.Frame();
    session.Click(list.RowCenter(2));
    session.DoubleClick(list.RowCenter(1));
    CHECK_FALSE(everActive);
    CHECK(list.liftedFrames == 0);
    CHECK_FALSE(list.delivered);
}

TEST_CASE("A drag starts after the threshold token at scale 0.75, 1 and 2")
{
    ScaleGuard guard;
    for (const float scale : {0.75f, 1.0f, 2.0f})
    {
        CAPTURE(scale);
        facet::SetUiScale(scale);
        facet::test::Session session;
        REQUIRE(session.Ready());
        TestList list;
        session.SetUi([&] { list.Ui(session, "a"); });
        session.Frame();
        const float threshold = facet::Px(facet::T().drag.startThreshold);
        const float saved = ImGui::GetIO().MouseDragThreshold;
        // Dear ImGui floors mouse positions; whole pixels keep the distances exact.
        const ImVec2 at(std::floor(list.RowCenter(1).x), std::floor(list.RowCenter(1).y));
        session.MouseMove(at);
        session.MouseDown();
        session.MouseMove(ImVec2(at.x + threshold - 0.5f, at.y));
        CHECK(list.lifted == -1);
        CHECK_FALSE(list.active);
        session.MouseMove(ImVec2(at.x + threshold + 0.5f, at.y));
        CHECK(list.lifted == 1);
        CHECK(list.active);
        session.MouseUp();
        session.Frames(2);
        // The threshold is set for Dear ImGui's call only.
        CHECK(ImGui::GetIO().MouseDragThreshold == saved);
    }
}

TEST_CASE("No drag starts from a row while a slider or a text field over it holds the mouse")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float value = 0.0f;
    char text[32] = "first second third";
    bool everActive = false;
    ImRect sliderRect;
    ImRect fieldRect;
    session.SetUi([&] {
        const RowPayload payload{0, 0};
        // A row with a slider on it.
        const ImVec2 rowPos = ImGui::GetCursorScreenPos();
        fi::ItemOptions overlap;
        overlap.allowOverlap = true;
        fi::ItemBehavior(ImGui::GetID("row1"), ImVec2(300.0f, 30.0f), overlap);
        if (fi::DragDropBeginSource(kRowType, &payload, sizeof(payload)))
            fi::DragDropEndSource();
        const ImVec2 after = ImGui::GetCursorScreenPos();
        ImGui::SetCursorScreenPos(ImVec2(rowPos.x + 20.0f, rowPos.y + 4.0f));
        ImGui::SetNextItemWidth(200.0f);
        ImGui::SliderFloat("##slider", &value, 0.0f, 1.0f);
        sliderRect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        ImGui::SetCursorScreenPos(after);
        // A row with a text field on it.
        const ImVec2 rowPos2 = ImGui::GetCursorScreenPos();
        fi::ItemBehavior(ImGui::GetID("row2"), ImVec2(300.0f, 30.0f), overlap);
        if (fi::DragDropBeginSource(kRowType, &payload, sizeof(payload)))
            fi::DragDropEndSource();
        ImGui::SetCursorScreenPos(ImVec2(rowPos2.x + 20.0f, rowPos2.y + 4.0f));
        ImGui::SetNextItemWidth(200.0f);
        ImGui::InputText("##field", text, sizeof(text));
        fieldRect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        // A text field made a source is never dragged either.
        const RowPayload fieldPayload{0, 1};
        if (fi::DragDropBeginSource(kRowType, &fieldPayload, sizeof(fieldPayload)))
            fi::DragDropEndSource();
        everActive = everActive || fi::DragDropActive(nullptr, nullptr);
    });
    session.Frames(2);
    const ImVec2 s = sliderRect.GetCenter();
    session.Drag({ImVec2(s.x - 60.0f, s.y), ImVec2(s.x + 60.0f, s.y)}, 6);
    CHECK(value > 0.5f);
    const ImVec2 f = fieldRect.GetCenter();
    session.Drag({ImVec2(fieldRect.Min.x + 6.0f, f.y), ImVec2(f.x + 40.0f, f.y)}, 6);
    CHECK_FALSE(everActive);
}

TEST_CASE("Disabled and read-only rows never start a drag")
{
    for (const bool readOnly : {false, true})
    {
        facet::test::Session session;
        REQUIRE(session.Ready());
        TestList list;
        list.disabled = !readOnly;
        list.readOnly = readOnly;
        session.SetUi([&] { list.Ui(session, "a"); });
        session.Frame();
        session.Drag({list.RowCenter(0), list.RowCenter(3)}, 6);
        CHECK(list.liftedFrames == 0);
        CHECK_FALSE(list.delivered);
    }
}

// ---- Results ------------------------------------------------------------------------------------

TEST_CASE("A drop reports where the row lands and changes nothing by itself")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestList list;
    session.SetUi([&] { list.Ui(session, "a"); });
    session.Frame();
    session.DragAndHold({list.RowCenter(0), list.GapPoint(3)}, 6, 2);
    REQUIRE(list.delivered);
    CHECK(list.deliveredIndex == 3);
    CHECK(list.deliveredPayload.index == 0);
    CHECK(list.deliveredEffect == DragDropEffect::Move);
    CHECK_FALSE(list.deliveredKeyboard);
    CHECK(list.keys == std::vector<int>{0, 1, 2, 3, 4});
    std::vector<int> model = list.keys;
    ApplyDrop(model, 0, model, list.deliveredIndex, true);
    CHECK(model == std::vector<int>{1, 2, 0, 3, 4});
}

TEST_CASE("Two lists exchange a row")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestList left;
    TestList right;
    right.list = 1;
    right.keys = {10, 11, 12};
    session.SetUi([&] {
        ImGui::BeginGroup();
        left.Ui(session, "left");
        ImGui::EndGroup();
        ImGui::SameLine(0.0f, 40.0f);
        ImGui::BeginGroup();
        right.Ui(session, "right");
        ImGui::EndGroup();
    });
    session.Frame();
    session.DragAndHold({left.RowCenter(1), right.GapPoint(2)}, 8, 2);
    REQUIRE(right.delivered);
    CHECK_FALSE(left.delivered);
    CHECK(right.deliveredPayload.list == 0);
    CHECK(right.deliveredPayload.index == 1);
    CHECK(right.deliveredIndex == 2);
    ApplyDrop(left.keys, right.deliveredPayload.index, right.keys, right.deliveredIndex, false);
    CHECK(left.keys == std::vector<int>{0, 2, 3, 4});
    CHECK(right.keys == std::vector<int>{10, 11, 1, 12});
}

TEST_CASE("Ctrl copies and Shift splits only where the zone allows it")
{
    struct Case
    {
        ImGuiKeyChord mod;
        bool allowCopy;
        bool allowSplit;
        DragDropEffect expected;
    };
    const Case cases[] = {
        {ImGuiMod_Ctrl, true, false, DragDropEffect::Copy},  {ImGuiMod_Ctrl, false, false, DragDropEffect::Move},
        {ImGuiMod_Shift, false, true, DragDropEffect::Split}, {ImGuiMod_Shift, true, false, DragDropEffect::Move},
        {0, true, true, DragDropEffect::Move},
    };
    for (const Case& c : cases)
    {
        facet::test::Session session;
        REQUIRE(session.Ready());
        TestList list;
        list.options.allowCopy = c.allowCopy;
        list.options.allowSplit = c.allowSplit;
        session.SetUi([&] { list.Ui(session, "a"); });
        session.Frame();
        session.MouseMove(list.RowCenter(0));
        session.MouseDown();
        session.MouseMove(list.GapPoint(2));
        session.MouseMove(list.GapPoint(3));
        if (c.mod != 0)
            session.KeyDown(c.mod);
        session.Frames(2);
        session.MouseUp();
        if (c.mod != 0)
            session.KeyUp(c.mod);
        REQUIRE(list.delivered);
        CHECK(list.deliveredEffect == c.expected);
    }
}

// ---- Cancel -------------------------------------------------------------------------------------

TEST_CASE("Escape cancels a drag and the release drops nothing")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestList list;
    bool canceled = false;
    session.SetUi([&] {
        list.Ui(session, "a");
        canceled = canceled || fi::DragDropCanceled();
    });
    session.Frame();
    session.MouseMove(list.RowCenter(0));
    session.MouseDown();
    session.MouseMove(list.GapPoint(2));
    session.MouseMove(list.GapPoint(3));
    REQUIRE(list.active);
    session.KeyPress(ImGuiKey_Escape);
    CHECK_FALSE(list.active);
    CHECK(canceled);
    CHECK(list.lifted == -1);
    session.MouseUp();
    session.Frames(2);
    CHECK_FALSE(list.delivered);
}

TEST_CASE("A right click cancels a drag and opens no context menu")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestList list;
    bool canceled = false;
    bool menuOpened = false;
    int stockPresses = 0;
    session.SetUi([&] {
        list.Ui(session, "a");
        if (ImGui::BeginPopupContextWindow("menu"))
        {
            menuOpened = true;
            ImGui::EndPopup();
        }
        canceled = canceled || fi::DragDropCanceled();
        if (ImGui::Button("stock", ImVec2(200.0f, 30.0f)))
            ++stockPresses;
        session.Track("stock");
    });
    session.Frame();
    session.MouseMove(list.RowCenter(0));
    session.MouseDown();
    session.MouseMove(list.GapPoint(2));
    session.MouseMove(session.ItemCenter("stock"));
    REQUIRE(list.active);
    session.MouseDown(ImGuiMouseButton_Right);
    session.MouseUp(ImGuiMouseButton_Right);
    session.Frames(2);
    CHECK(canceled);
    CHECK_FALSE(list.active);
    CHECK_FALSE(menuOpened);
    session.MouseMove(list.GapPoint(3));
    session.MouseUp();
    session.Frames(2);
    CHECK_FALSE(list.delivered);
    CHECK(stockPresses == 0);
}

TEST_CASE("Releasing where no zone takes the drag cancels it")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestList list;
    bool canceled = false;
    session.SetUi([&] {
        list.Ui(session, "a");
        canceled = canceled || fi::DragDropCanceled();
    });
    session.Frame();
    session.DragAndHold({list.RowCenter(0), ImVec2(600.0f, 500.0f)}, 6, 1);
    session.Frames(2);
    CHECK(canceled);
    CHECK_FALSE(list.delivered);
    CHECK_FALSE(list.active);
}

// ---- Zones --------------------------------------------------------------------------------------

TEST_CASE("A zone of another type, or one that refuses, rejects the drag with the not-allowed cursor")
{
    for (const bool refuse : {false, true})
    {
        CAPTURE(refuse);
        facet::test::Session session;
        REQUIRE(session.Ready());
        TestList list;
        DragDropZoneResult other;
        bool everDelivered = false;
        bool everHovered = false;
        ImRect otherRect;
        ImGuiMouseCursor cursor = ImGuiMouseCursor_Arrow;
        session.SetUi([&] {
            list.Ui(session, "a");
            ImGui::Dummy(ImVec2(200.0f, 20.0f));
            const ImVec2 pos = ImGui::GetCursorScreenPos();
            otherRect = ImRect(pos, ImVec2(pos.x + 200.0f, pos.y + 60.0f));
            ImGui::Dummy(otherRect.GetSize());
            fi::DragDropZoneOptions options;
            options.accept = !refuse;
            other = fi::DragDropZone(ImGui::GetID("other"), otherRect, refuse ? kRowType : "test.other", options);
            everDelivered = everDelivered || other.delivered;
            everHovered = everHovered || other.hovered;
        });
        session.Frame();
        session.MouseMove(list.RowCenter(0));
        session.MouseDown();
        session.MouseMove(list.GapPoint(2));
        session.MouseMove(otherRect.GetCenter());
        session.Frames(2);
        CHECK(other.rejected);
        CHECK_FALSE(other.hovered);
        cursor = ImGui::GetMouseCursor();
        CHECK(cursor == ImGuiMouseCursor_NotAllowed);
        session.MouseUp();
        session.Frames(2);
        CHECK_FALSE(everDelivered);
        CHECK_FALSE(everHovered);
        CHECK_FALSE(list.delivered);
    }
}

TEST_CASE("The smaller of two nested zones takes the drag")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestList list;
    DragDropZoneResult outer;
    DragDropZoneResult inner;
    int outerDelivered = 0;
    int innerDelivered = 0;
    bool outerHovered = false;
    ImRect innerRect;
    session.SetUi([&] {
        list.Ui(session, "a");
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const ImRect outerRect(pos, ImVec2(pos.x + 300.0f, pos.y + 120.0f));
        innerRect = ImRect(ImVec2(pos.x + 50.0f, pos.y + 30.0f), ImVec2(pos.x + 150.0f, pos.y + 90.0f));
        ImGui::Dummy(outerRect.GetSize());
        outer = fi::DragDropZone(ImGui::GetID("outer"), outerRect, kRowType);
        inner = fi::DragDropZone(ImGui::GetID("inner"), innerRect, kRowType);
        outerDelivered += outer.delivered ? 1 : 0;
        innerDelivered += inner.delivered ? 1 : 0;
        outerHovered = outerHovered || (outer.hovered && innerRect.Contains(ImGui::GetIO().MousePos));
    });
    session.Frame();
    session.DragAndHold({list.RowCenter(0), innerRect.GetCenter()}, 8, 3);
    CHECK(innerDelivered == 1);
    CHECK(outerDelivered == 0);
    CHECK_FALSE(outerHovered);
}

TEST_CASE("A drag resting on a zone reports hoverOpen after the delay")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestList list;
    DragDropZoneResult node;
    ImRect nodeRect;
    session.SetUi([&] {
        list.Ui(session, "a");
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        nodeRect = ImRect(pos, ImVec2(pos.x + 200.0f, pos.y + 30.0f));
        ImGui::Dummy(nodeRect.GetSize());
        node = fi::DragDropZone(ImGui::GetID("node"), nodeRect, kRowType);
    });
    session.Frame();
    session.MouseMove(list.RowCenter(0));
    session.MouseDown();
    session.MouseMove(list.GapPoint(3));
    session.MouseMove(nodeRect.GetCenter());
    session.Seconds(facet::T().drag.hoverOpenDelay * 0.5f);
    CHECK(node.hovered);
    CHECK_FALSE(node.hoverOpen);
    session.Seconds(facet::T().drag.hoverOpenDelay * 0.6f);
    CHECK(node.hoverOpen);
    session.MouseUp();
    session.Frames(2);
}

// ---- Interop with stock Dear ImGui ---------------------------------------------------------------

TEST_CASE("A stock Dear ImGui source drops on a Facet zone")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestList list;
    session.SetUi([&] {
        ImGui::Button("stock source", ImVec2(200.0f, 30.0f));
        session.Track("source");
        if (ImGui::BeginDragDropSource())
        {
            const RowPayload payload{7, 42};
            ImGui::SetDragDropPayload(kRowType, &payload, sizeof(payload));
            ImGui::TextUnformatted("stock preview");
            ImGui::EndDragDropSource();
        }
        list.Ui(session, "a");
    });
    session.Frame();
    session.DragAndHold({session.ItemCenter("source"), list.GapPoint(2)}, 8, 2);
    REQUIRE(list.delivered);
    CHECK(list.deliveredPayload.list == 7);
    CHECK(list.deliveredPayload.index == 42);
    CHECK(list.deliveredIndex == 2);
}

TEST_CASE("A Facet source drops on a stock Dear ImGui target")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestList list;
    RowPayload received;
    int deliveries = 0;
    session.SetUi([&] {
        list.Ui(session, "a");
        ImGui::Button("stock target", ImVec2(200.0f, 40.0f));
        session.Track("target");
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kRowType))
            {
                received = *static_cast<const RowPayload*>(payload->Data);
                ++deliveries;
            }
            ImGui::EndDragDropTarget();
        }
    });
    session.Frame();
    session.DragAndHold({list.RowCenter(3), session.ItemCenter("target")}, 8, 2);
    CHECK(deliveries == 1);
    CHECK(received.index == 3);
    CHECK_FALSE(list.delivered);
}

// ---- The ghost ----------------------------------------------------------------------------------

TEST_CASE("The ghost follows the pointer at the grab offset with the ghost opacity")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestList list;
    session.SetUi([&] { list.Ui(session, "a"); });
    session.Frame();
    const ImVec2 grab(list.slots[1].Min.x + 30.0f, list.slots[1].Min.y + 5.0f);
    session.MouseMove(grab);
    session.MouseDown();
    session.MouseMove(ImVec2(grab.x + 20.0f, grab.y + 40.0f));
    const ImVec2 pointer(grab.x + 60.0f, grab.y + 70.0f);
    session.MouseMove(pointer);
    const ImRect ghost = fi::DragDropGhostRect();
    CHECK(ghost.Min.x == doctest::Approx(pointer.x - 30.0f));
    CHECK(ghost.Min.y == doctest::Approx(pointer.y - 5.0f));
    CHECK(ghost.GetWidth() == doctest::Approx(list.rowSize.x));
    CHECK(ghost.GetHeight() == doctest::Approx(list.rowSize.y));

    REQUIRE(list.active);
    CHECK(list.info.facetSource);
    CHECK_FALSE(list.info.keyboard);
    CHECK(list.info.grabOffset.x == doctest::Approx(30.0f));
    CHECK(list.info.sourceSize.y == doctest::Approx(list.rowSize.y));

    // Every vertex of the ghost is at most as opaque as the token.
    const ImDrawList* foreground = ImGui::GetForegroundDrawList();
    const int limit = static_cast<int>(facet::T().drag.ghostOpacity * 255.0f + 1.0f);
    int vertices = 0;
    int maxAlpha = 0;
    for (const ImDrawVert& v : foreground->VtxBuffer)
    {
        ++vertices;
        maxAlpha = std::max(maxAlpha, static_cast<int>((v.col >> IM_COL32_A_SHIFT) & 0xFF));
    }
    CHECK(vertices > 0);
    CHECK(maxAlpha <= limit);
    session.MouseUp();
    session.Frames(2);
    CHECK(fi::DragDropGhostRect().GetWidth() == 0.0f);
}

TEST_CASE("The drag and its ghost survive the source row disappearing")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestList list;
    session.SetUi([&] { list.Ui(session, "a"); });
    session.Frame();
    session.MouseMove(list.RowCenter(0));
    session.MouseDown();
    session.MouseMove(list.GapPoint(2));
    REQUIRE(list.lifted == 0);
    list.hidden = 0;  // the source row is no longer submitted
    session.Frames(3);
    session.MouseMove(list.GapPoint(4));
    CHECK(list.active);
    CHECK(fi::DragDropGhostRect().GetWidth() > 0.0f);
    session.Frames(2);
    session.MouseUp();
    session.Frame();
    REQUIRE(list.delivered);
    CHECK(list.deliveredPayload.index == 0);
    CHECK(list.deliveredIndex == 4);
}

TEST_CASE("A forced drag shows a drag without input")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestList list;
    session.SetUi([&] { list.Ui(session, "a"); });
    session.Frame();
    // The pointer in the gap before row 3, as an offset from row 0's center.
    list.forceFirst = true;
    list.forceOffset = ImVec2(0.0f, list.GapPoint(3).y - list.RowCenter(0).y);
    session.Frames(4);
    CHECK(list.lifted == 0);
    CHECK(list.drop.hovered);
    CHECK(list.drop.insertIndex == 3);
    CHECK_FALSE(list.delivered);
    CHECK(fi::DragDropGhostRect().GetWidth() > 0.0f);
    list.forceFirst = false;
    session.Frames(2);
    CHECK(list.lifted == -1);
    CHECK(fi::DragDropGhostRect().GetWidth() == 0.0f);
}

// ---- Keyboard -----------------------------------------------------------------------------------

TEST_CASE("Space picks a focused row up, the arrows move it and Enter drops it")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestList list;
    session.SetUi([&] { list.Ui(session, "a"); });
    // After the window's first frames, so Dear ImGui's initial focus does not replace ours.
    session.Frames(2);
    list.focusKey = 1;
    session.Frames(2);
    session.KeyPress(ImGuiKey_Space);
    REQUIRE(list.active);
    CHECK(list.info.keyboard);
    CHECK(list.lifted == 1);
    CHECK(list.drop.hovered);
    CHECK(list.drop.keyboard);
    CHECK(list.drop.insertIndex == 1);
    session.KeyPress(ImGuiKey_DownArrow);
    CHECK(list.drop.insertIndex == 3);
    session.KeyPress(ImGuiKey_DownArrow);
    CHECK(list.drop.insertIndex == 4);
    session.KeyPress(ImGuiKey_UpArrow);
    CHECK(list.drop.insertIndex == 3);
    // The ghost sits over the gap the rows show.
    CHECK(fi::DragDropGhostRect().GetWidth() > 0.0f);
    session.KeyPress(ImGuiKey_Enter);
    REQUIRE(list.delivered);
    CHECK(list.deliveredKeyboard);
    CHECK(list.deliveredIndex == 3);
    CHECK(list.deliveredPayload.index == 1);
    session.Frame();
    CHECK_FALSE(list.active);
    std::vector<int> model = list.keys;
    ApplyDrop(model, 1, model, list.deliveredIndex, true);
    CHECK(model == std::vector<int>{0, 2, 1, 3, 4});
}

TEST_CASE("Escape puts a keyboard drag back and Space drops it too")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestList list;
    bool canceled = false;
    session.SetUi([&] {
        list.Ui(session, "a");
        canceled = canceled || fi::DragDropCanceled();
    });
    session.Frames(2);
    list.focusKey = 2;
    session.Frames(2);
    session.KeyPress(ImGuiKey_Space);
    session.KeyPress(ImGuiKey_UpArrow);
    CHECK(list.drop.insertIndex == 1);
    session.KeyPress(ImGuiKey_Escape);
    CHECK(canceled);
    CHECK_FALSE(list.active);
    CHECK_FALSE(list.delivered);
    // The focus stayed on the row, so Space picks it up again; Space also drops.
    session.KeyPress(ImGuiKey_Space);
    session.KeyPress(ImGuiKey_UpArrow);
    session.KeyPress(ImGuiKey_UpArrow);
    session.KeyPress(ImGuiKey_Space);
    REQUIRE(list.delivered);
    CHECK(list.deliveredIndex == 0);
}

TEST_CASE("The keyboard moves a row into the other list")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestList left;
    TestList right;
    right.list = 1;
    right.keys = {10, 11, 12};
    session.SetUi([&] {
        ImGui::BeginGroup();
        left.Ui(session, "left");
        ImGui::EndGroup();
        ImGui::SameLine(0.0f, 40.0f);
        ImGui::BeginGroup();
        right.Ui(session, "right");
        ImGui::EndGroup();
    });
    session.Frames(2);
    left.focusKey = 3;
    session.Frames(2);
    session.KeyPress(ImGuiKey_Space);
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(right.drop.hovered);
    CHECK_FALSE(left.drop.hovered);
    session.KeyPress(ImGuiKey_Enter);
    REQUIRE(right.delivered);
    CHECK(right.deliveredPayload.list == 0);
    CHECK(right.deliveredPayload.index == 3);
    CHECK(right.deliveredIndex == 3);
    ApplyDrop(left.keys, right.deliveredPayload.index, right.keys, right.deliveredIndex, false);
    CHECK(right.keys == std::vector<int>{10, 11, 12, 3});
}

TEST_CASE("A click ends a keyboard drag without a drop")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestList list;
    session.SetUi([&] { list.Ui(session, "a"); });
    session.Frames(2);
    list.focusKey = 0;
    session.Frames(2);
    session.KeyPress(ImGuiKey_Space);
    session.KeyPress(ImGuiKey_DownArrow);
    REQUIRE(list.active);
    session.Click(ImVec2(600.0f, 500.0f));
    CHECK_FALSE(list.active);
    CHECK_FALSE(list.delivered);
}

// ---- Edge scrolling -----------------------------------------------------------------------------

TEST_CASE("Edge scrolling asks the innermost area that can still scroll toward the edge")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestList list;
    list.keys.clear();
    for (int i = 0; i < 20; ++i)
        list.keys.push_back(i);
    // The inner list sits at the bottom of the outer area, so near its bottom edge the pointer is
    // in the edge band of both; near its top edge only in the inner one's.
    ImGuiID outerId = 0;
    ImGuiID innerId = 0;
    ImRect innerRect;
    float setInnerScroll = -1.0f;
    float innerScroll = 0.0f;
    session.SetUi([&] {
        ImGui::BeginChild("outer", ImVec2(300.0f, 250.0f));
        outerId = ImGui::GetCurrentWindow()->ID;
        const float outerBottom = ImGui::GetCurrentWindow()->InnerRect.Max.y;
        ImGui::Dummy(ImVec2(10.0f, outerBottom - ImGui::GetCursorScreenPos().y - 100.0f - ImGui::GetStyle().ItemSpacing.y));
        ImGui::BeginChild("inner", ImVec2(260.0f, 100.0f));
        innerId = ImGui::GetCurrentWindow()->ID;
        if (setInnerScroll >= 0.0f)
        {
            ImGui::SetScrollY(setInnerScroll);
            setInnerScroll = -1.0f;
        }
        list.Ui(session, "a");
        innerRect = ImGui::GetCurrentWindow()->InnerRect;
        innerScroll = ImGui::GetScrollY();
        ImGui::EndChild();
        ImGui::Dummy(ImVec2(10.0f, 300.0f));
        ImGui::EndChild();
    });
    session.Frames(2);
    const ImVec2 nearBottom(innerRect.GetCenter().x, innerRect.Max.y - 4.0f);
    const ImVec2 nearTop(innerRect.GetCenter().x, innerRect.Min.y + 4.0f);
    session.MouseMove(list.RowCenter(0));
    session.MouseDown();
    session.MouseMove(list.RowCenter(1));
    session.MouseMove(nearBottom);
    session.Frame();
    CHECK(fi::DragDropRequestedEdgeScroll(innerId));
    CHECK_FALSE(fi::DragDropRequestedEdgeScroll(outerId));
    // The scroll core moved the inner list down. Back at its start (the pointer in the middle,
    // outside both edge bands), there is nothing above: nobody is asked near its top edge,
    // since the outer area is not near its own edge there.
    CHECK(innerScroll > 0.0f);
    session.MouseMove(innerRect.GetCenter());
    setInnerScroll = 0.0f;
    session.Frames(2);
    REQUIRE(innerScroll == 0.0f);
    session.MouseMove(nearTop);
    session.Frame();
    CHECK_FALSE(fi::DragDropRequestedEdgeScroll(innerId));
    // Scrolled to its end, the inner list passes the bottom edge on to the outer area.
    setInnerScroll = 10000.0f;
    session.Frames(2);
    session.MouseMove(ImVec2(nearBottom.x, innerRect.Max.y - 4.0f));
    session.Frame();
    CHECK_FALSE(fi::DragDropRequestedEdgeScroll(innerId));
    CHECK(fi::DragDropRequestedEdgeScroll(outerId));
    // Near its top edge the inner list scrolls back up.
    session.MouseMove(nearTop);
    session.Frame();
    CHECK(fi::DragDropRequestedEdgeScroll(innerId));
    session.MouseUp();
    session.Frames(2);
    CHECK_FALSE(fi::DragDropRequestedEdgeScroll(innerId));
}

TEST_CASE("Past the edge of the area that scrolled, the drag keeps asking it")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestList list;
    list.keys.clear();
    for (int i = 0; i < 20; ++i)
        list.keys.push_back(i);
    ImGuiID innerId = 0;
    ImRect innerRect;
    session.SetUi([&] {
        ImGui::BeginChild("inner", ImVec2(260.0f, 100.0f));
        innerId = ImGui::GetCurrentWindow()->ID;
        list.Ui(session, "a");
        innerRect = ImGui::GetCurrentWindow()->InnerRect;
        ImGui::EndChild();
    });
    session.Frames(2);
    session.MouseMove(list.RowCenter(0));
    session.MouseDown();
    session.MouseMove(list.RowCenter(1));
    session.MouseMove(ImVec2(innerRect.GetCenter().x, innerRect.Max.y - 4.0f));
    session.Frame();
    CHECK(fi::DragDropRequestedEdgeScroll(innerId));
    session.MouseMove(ImVec2(innerRect.GetCenter().x, innerRect.Max.y + 40.0f));
    session.Frame();
    CHECK(fi::DragDropRequestedEdgeScroll(innerId));
    session.MouseUp();
    session.Frames(2);
}

// ---- Usage errors -------------------------------------------------------------------------------

TEST_CASE("Oversized payloads, long types and unbalanced calls are usage errors")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    const int before = fi::UsageErrorCount();
    bool began = false;
    session.SetUi([&] {
        fi::ItemBehavior(ImGui::GetID("row"), ImVec2(100.0f, 20.0f));
        unsigned char big[fi::kDragDropMaxPayloadSize + 1] = {};
        began = began || fi::DragDropBeginSource(kRowType, big, sizeof(big));
        const int value = 1;
        began = began || fi::DragDropBeginSource("a type name that is longer than thirty-two", &value, sizeof(value));
        fi::DragDropEndSource();
    });
    session.Frame();
    CHECK_FALSE(began);
    CHECK(fi::UsageErrorCount() == before + 3);
}
