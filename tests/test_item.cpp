// Item behaviour (src/internal/item.h): the behaviour every Facet widget runs, the forced state
// request used by capture runs, and the widget entry and exit helpers.

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "internal/item.h"
#include "support/session.h"

using facet::internal::ForcedItemState;
using facet::internal::ItemBehavior;
using facet::internal::ItemOptions;
using facet::internal::ItemState;
using facet::internal::PressMode;

TEST_CASE("A forced state request is taken once by the next widget")
{
    facet::internal::ClearForcedItemState();
    facet::internal::ForceNextItemState(ForcedItemState::Hovered | ForcedItemState::Focused, 1);
    const facet::internal::ForcedItemRequest taken = facet::internal::TakeForcedItemState();
    CHECK(Has(taken.state, ForcedItemState::Hovered));
    CHECK(Has(taken.state, ForcedItemState::Focused));
    CHECK_FALSE(Has(taken.state, ForcedItemState::Pressed));
    CHECK(taken.part == 1);
    // Taken: the widget after it gets nothing.
    const facet::internal::ForcedItemRequest next = facet::internal::TakeForcedItemState();
    CHECK(next.state == ForcedItemState::None);
    CHECK(next.part == -1);
}

TEST_CASE("A second request replaces the first")
{
    facet::internal::ClearForcedItemState();
    facet::internal::ForceNextItemState(ForcedItemState::Hovered);
    facet::internal::ForceNextItemState(ForcedItemState::Dragging, 0);
    const facet::internal::ForcedItemRequest taken = facet::internal::TakeForcedItemState();
    CHECK(taken.state == ForcedItemState::Dragging);
    CHECK(taken.part == 0);
}

TEST_CASE("A request no widget took is dropped at EndFrame")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    session.SetUi([] { facet::internal::ForceNextItemState(ForcedItemState::Open); });
    session.Frame();
    CHECK(facet::internal::TakeForcedItemState().state == ForcedItemState::None);
}

namespace
{

// One control made of the item behaviour, with what the caller can ask Dear ImGui afterwards.
struct TestButton
{
    ItemOptions options;
    ImVec2 size = ImVec2(60.0f, 20.0f);
    bool disabled = false;
    bool leadingButton = false;
    ItemState state;
    int presses = 0;
    bool itemHovered = false;
    bool itemHoveredDisabled = false;
    bool itemActive = false;
    bool itemFocused = false;
    ImRect itemRect;
    ImGuiMouseCursor cursor = ImGuiMouseCursor_Arrow;

    void Ui(facet::test::Session& session)
    {
        if (leadingButton)
            ImGui::Button("first");
        if (disabled)
            ImGui::BeginDisabled();
        state = ItemBehavior(ImGui::GetID("button"), size, options);
        presses += state.pressed ? 1 : 0;
        itemHovered = ImGui::IsItemHovered();
        itemHoveredDisabled = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);
        itemActive = ImGui::IsItemActive();
        itemFocused = ImGui::IsItemFocused();
        itemRect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        cursor = ImGui::GetMouseCursor();
        session.Track("button");
        if (disabled)
            ImGui::EndDisabled();
    }
};

void Install(facet::test::Session& session, TestButton& button)
{
    session.SetUi([&] { button.Ui(session); });
    session.Frames(2);
}

} // namespace

TEST_CASE("Item behaviour: hover, press and release like a stock button")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestButton button;
    Install(session, button);
    const ImVec2 center = session.ItemCenter("button");
    session.MouseMove(center);
    CHECK(button.state.hovered);
    CHECK(button.itemHovered);
    CHECK(button.cursor == ImGuiMouseCursor_Hand);
    session.MouseDown();
    CHECK(button.state.held);
    CHECK(button.state.active);
    CHECK(button.state.activated);
    CHECK(button.itemActive);
    CHECK(button.state.dragging);
    CHECK(button.presses == 0);
    session.MouseUp();
    CHECK(button.presses == 1);
    CHECK(button.state.deactivated);
    CHECK(button.state.released);
    CHECK_FALSE(button.state.keyboardPressed);
    // The animated amounts follow the state and reach it.
    session.Seconds(0.5f);
    CHECK(button.state.hoverAmount == 1.0f);
    CHECK(button.state.pressAmount == 0.0f);
}

TEST_CASE("Item behaviour: the hit rectangle grows to the minimum hit target")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    facet::SetTheme(facet::Theme::Light(), false);
    facet::SetUiScale(1.0f);
    TestButton button;
    button.size = ImVec2(12.0f, 12.0f);
    Install(session, button);
    const float minimum = facet::T().metrics.minHitTarget;
    CHECK(button.state.hitRect.GetWidth() == doctest::Approx(minimum));
    CHECK(button.state.hitRect.GetHeight() == doctest::Approx(minimum));
    // Dear ImGui reports the visible rectangle.
    CHECK(button.itemRect.GetWidth() == doctest::Approx(12.0f));
    // A click outside the visible box but inside the hit rectangle presses.
    const ImRect visible = button.itemRect;
    session.Click(ImVec2(visible.Max.x + 4.0f, visible.GetCenter().y));
    CHECK(button.presses == 1);
    // Larger items keep their own rectangle.
    CHECK(facet::internal::HitRect(ImRect(0.0f, 0.0f, 50.0f, 30.0f), minimum).GetWidth() == 50.0f);
}

TEST_CASE("Item behaviour: disabled items do nothing and show the not-allowed cursor")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestButton button;
    button.disabled = true;
    Install(session, button);
    session.MouseMove(session.ItemCenter("button"));
    CHECK(button.state.disabled);
    CHECK_FALSE(button.state.hovered);
    CHECK_FALSE(button.itemHovered);
    CHECK(button.itemHoveredDisabled);
    CHECK(button.cursor == ImGuiMouseCursor_NotAllowed);
    session.Click(session.ItemCenter("button"));
    CHECK(button.presses == 0);
    // Not a Tab stop.
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() != session.ItemId("button"));

    // The option disables the same way.
    button.disabled = false;
    button.options.disabled = true;
    session.Click(session.ItemCenter("button"));
    CHECK(button.presses == 0);
    CHECK(button.state.disabled);
}

TEST_CASE("Item behaviour: keyboard activation and focus without the hover look")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestButton button;
    button.leadingButton = true;
    Install(session, button);
    session.KeyPress(ImGuiKey_Tab);
    if (ImGui::GetFocusID() != session.ItemId("button"))
        session.KeyPress(ImGuiKey_Tab);
    REQUIRE(ImGui::GetFocusID() == session.ItemId("button"));
    CHECK(button.state.focused);
    CHECK(button.itemFocused);
    CHECK(button.state.focusVisible);  // the last input was the keyboard
    CHECK_FALSE(button.state.hovered); // focus is not reported as hover
    session.KeyDown(ImGuiKey_Space);
    CHECK(button.state.keyboardPressed);
    session.KeyUp(ImGuiKey_Space);
    CHECK(button.presses == 1);
    session.KeyPress(ImGuiKey_Enter);
    CHECK(button.presses == 2);
    // A click keeps the focus but hides the ring.
    session.Click(session.ItemCenter("button"));
    CHECK(button.state.focused);
    CHECK_FALSE(button.state.focusVisible);
}

TEST_CASE("Item behaviour: repeat presses while held, by time")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestButton button;
    button.options.repeat = true;
    Install(session, button);
    session.MouseMove(session.ItemCenter("button"));
    session.MouseDown();
    CHECK(button.presses == 1);  // repeat buttons press on the click
    session.Seconds(0.5f);
    session.MouseUp();
    // The press, then repeats from io.KeyRepeatDelay (0.275 s) every 0.05 s.
    CHECK(button.presses >= 5);
}

TEST_CASE("Item behaviour: forced states show without input and without effects")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestButton button;
    session.SetUi([&] {
        facet::internal::ForceNextItemState(ForcedItemState::Hovered | ForcedItemState::Focused |
                                            ForcedItemState::Dragging | ForcedItemState::Open);
        button.Ui(session);
    });
    session.Frames(30);
    CHECK(button.state.hovered);
    CHECK(button.state.held);
    CHECK(button.state.dragging);
    CHECK(button.state.focused);
    CHECK(button.state.focusVisible);
    CHECK(button.state.open);
    CHECK(button.state.forced != ForcedItemState::None);
    CHECK(button.state.focusAmount == 1.0f);
    CHECK(button.state.pressAmount == 1.0f);
    // Nothing really happened.
    CHECK_FALSE(button.itemActive);
    CHECK(button.presses == 0);
    CHECK(ImGui::GetActiveID() == 0);

    // A request for another part leaves this item alone; part -1 reaches part 0.
    facet::internal::ItemOptions other;
    other.part = 1;
    button.options = other;
    session.Frame();
    CHECK_FALSE(button.state.held);
    button.options.part = 0;
    session.SetUi([&] {
        facet::internal::ForceNextItemState(ForcedItemState::Pressed);
        button.Ui(session);
    });
    session.Frame();
    CHECK(button.state.held);
}

TEST_CASE("Item behaviour: stock item queries, tooltips and drag and drop work after the call")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool sourceOpened = false;
    bool tooltipSet = false;
    session.SetUi([&] {
        ItemBehavior(ImGui::GetID("source"), ImVec2(60.0f, 20.0f), {.press = PressMode::Click});
        session.Track("source");
        if (ImGui::BeginDragDropSource())
        {
            sourceOpened = true;
            ImGui::SetDragDropPayload("test", "x", 1);
            ImGui::EndDragDropSource();
        }
        ImGui::SetItemTooltip("tip");
        tooltipSet = tooltipSet || ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip);
    });
    session.Frames(2);
    const ImVec2 center = session.ItemCenter("source");
    session.MouseMove(center);
    session.Seconds(1.0f);
    CHECK(tooltipSet);
    session.Drag({center, ImVec2(center.x + 80.0f, center.y + 40.0f)});
    CHECK(sourceOpened);
}

TEST_CASE("Widget entry: next-item data is captured and the width follows the precedence")
{
    facet::test::Session session({.displaySize = ImVec2(600.0f, 300.0f)});
    REQUIRE(session.Ready());
    facet::SetUiScale(1.0f);
    float widths[6] = {};
    bool hadWidth = false;
    bool cleared = false;
    session.SetUi([&] {
        const facet::internal::NextItemData none = facet::internal::TakeNextItemData();
        const float available = ImGui::GetContentRegionAvail().x;
        widths[0] = facet::internal::ResolveItemWidth(0.0f, none, facet::kFill) - available;   // default: fill
        widths[1] = facet::internal::ResolveItemWidth(0.0f, none, 120.0f);                    // default: fixed
        widths[2] = facet::internal::ResolveItemWidth(80.0f, none, 120.0f);                   // options units
        widths[3] = facet::internal::ResolveItemWidth(-30.0f, none, 120.0f) - available;      // fill minus 30
        ImGui::SetNextItemWidth(150.0f);
        const facet::internal::NextItemData next = facet::internal::TakeNextItemData();
        hadWidth = next.hasWidth;
        cleared = (GImGui->NextItemData.HasFlags & ImGuiNextItemDataFlags_HasWidth) == 0;
        widths[4] = facet::internal::ResolveItemWidth(0.0f, next, 120.0f);  // SetNextItemWidth
        ImGui::PushItemWidth(90.0f);
        widths[5] = facet::internal::ResolveItemWidth(0.0f, none, 120.0f);  // PushItemWidth
        ImGui::PopItemWidth();
        facet::internal::ApplyNextItemData(next);
        ImGui::Dummy(ImVec2(1.0f, 1.0f));  // consumes it
    });
    session.Frame();
    CHECK(widths[0] == doctest::Approx(0.0f));
    CHECK(widths[1] == 120.0f);
    CHECK(widths[2] == 80.0f);
    CHECK(widths[3] == doctest::Approx(-30.0f));
    CHECK(hadWidth);
    CHECK(cleared);
    CHECK(widths[4] == 150.0f);
    CHECK(widths[5] == 90.0f);
}

TEST_CASE("Widget exit: a group of parts reports the widget id while no part is active")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ImGuiID lastId = 0;
    ImGuiID widgetId = 0;
    ImGuiID partId = 0;
    session.SetUi([&] {
        widgetId = ImGui::GetID("widget");
        ImGui::BeginGroup();
        ImGui::PushID("widget");
        const ItemState part = ItemBehavior(ImGui::GetID("part"), ImVec2(40.0f, 20.0f), {.press = PressMode::Click});
        partId = part.id;
        session.Track("part");
        ImGui::PopID();
        ImGui::EndGroup();
        facet::internal::FinishWidgetGroup(widgetId);
        lastId = ImGui::GetItemID();
    });
    session.Frames(2);
    CHECK(lastId == widgetId);
    session.MouseMove(session.ItemCenter("part"));
    session.MouseDown();
    CHECK(lastId == partId);
    session.MouseUp();
    CHECK(lastId == partId);  // just deactivated: the part, so IsItemDeactivated holds
    session.Frame();
    CHECK(lastId == widgetId);
}
