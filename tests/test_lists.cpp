// Behaviour of the lists family (facet/widgets/lists.h).
//
// One test case per behaviour of an implemented widget; docs/writing-a-widget.md describes the
// pattern. The API checks, and the placeholder checks of the widgets that are still planned, are in
// tests/test_api_lists.cpp. Popup levels are read inside the frame (the popup host answers only
// there), so the ui code of a test records them in a PopupProbe.

#include <array>
#include <cstdio>
#include <cstring>
#include <span>
#include <string>
#include <vector>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "core/core_internal.h"
#include "internal/focus.h"
#include "internal/list_view.h"
#include "internal/popup_host.h"
#include "internal/scope_stack.h"
#include "internal/scroll.h"
#include "support/session.h"
#include "widgets/lists/list_parts.h"

namespace
{

namespace fi = facet::internal;

const facet::ChoiceItem kFruit[] = {
    {.label = "Apple"},
    {.label = "Banana"},
    {.label = "Cherry", .disabled = true},
    {.label = "Date"},
    {.label = "Elderberry"},
};

const facet::ChoiceItem kGrouped[] = {
    {.label = "Option 1", .group = "Group 1"},
    {.label = "Option 2", .group = "Group 1"},
    {.label = "Option 3", .group = "Group 2"},
};

// The open list of a dropdown-like widget as the last frame left it.
struct PopupProbe
{
    ImGuiID level = 0;
    bool open = false;
    ImRect rect;
    fi::PopupSide side = fi::PopupSide::Below;
    std::string query;

    // Call right after the widget `label`, inside the ui code.
    void Record(const char* label)
    {
        level = fi::ListsLevelId(ImGui::GetID(label));
        open = fi::IsPopupLevelOpen(level);
        rect = fi::PopupLevelRect(level);
        side = fi::PopupLevelSide(level);
        query = fi::ListsPopupQuery(level);
    }

    // The center of row `row` of an open list without a filter field: the popup padding, the area
    // padding, then the rows.
    ImVec2 Row(int row, facet::ControlSize size = facet::ControlSize::Regular) const
    {
        const float pad = facet::Px(facet::T().spacing.xs) + fi::FocusRingOutset();
        const float height = fi::ListRowHeight(size);
        return ImVec2(rect.GetCenter().x, rect.Min.y + pad + height * (static_cast<float>(row) + 0.5f));
    }
};

// Generated labels for the getter overloads: "Option <n>", formatted into one static buffer.
bool NumberedOption(void*, int index, facet::ChoiceItem* out)
{
    static char text[32];
    std::snprintf(text, sizeof(text), "Option %d", index + 1);
    out->label = text;
    return true;
}

// The same, counting the calls in *user.
bool CountedOption(void* user, int index, facet::ChoiceItem* out)
{
    ++*static_cast<int*>(user);
    return NumberedOption(nullptr, index, out);
}

} // namespace

// ---- Dropdown ---------------------------------------------------------------------------------

TEST_CASE("A dropdown opens on a click and a click on a row chooses it and closes the list")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int index = 0;
    int writes = 0;
    bool edited = false;
    PopupProbe popup;
    session.SetUi([&] {
        if (facet::Dropdown("Fruit", &index, kFruit))
            ++writes;
        edited = edited || ImGui::IsItemEdited();
        session.Track("fruit");
        popup.Record("Fruit");
    });
    session.Frame();
    session.Click(session.ItemCenter("fruit"));
    REQUIRE(popup.open);
    CHECK(popup.rect.Min.y >= session.ItemRect("fruit").Max.y);
    CHECK(popup.rect.GetWidth() >= session.ItemRect("fruit").GetWidth() * 0.5f);
    session.Click(popup.Row(3));
    CHECK(index == 3);
    CHECK(writes == 1);
    CHECK(edited);
    CHECK_FALSE(popup.open);
    // A click on the disabled row writes nothing and keeps the list open.
    session.Click(session.ItemCenter("fruit"));
    session.Click(popup.Row(2));
    CHECK(index == 3);
    CHECK(writes == 1);
    CHECK(popup.open);
    // A click on the field of an open list closes it without writing.
    session.Click(session.ItemCenter("fruit"));
    CHECK_FALSE(popup.open);
    CHECK(index == 3);
}

TEST_CASE("A focused dropdown opens with Space, Enter and Down, and the arrows skip disabled items")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int index = 1;
    int writes = 0;
    PopupProbe popup;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        if (facet::Dropdown("Fruit", &index, kFruit))
            ++writes;
        session.Track("fruit");
        popup.Record("Fruit");
    });
    session.Frame();
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    REQUIRE(ImGui::GetFocusID() == session.ItemId("fruit"));

    for (const ImGuiKey key : {ImGuiKey_Space, ImGuiKey_Enter, ImGuiKey_DownArrow})
    {
        CAPTURE(static_cast<int>(key));
        session.KeyPress(key);
        CHECK(popup.open);
        // The field keeps the keyboard focus while its list is open.
        CHECK(ImGui::GetFocusID() == session.ItemId("fruit"));
        session.KeyPress(ImGuiKey_Escape);
        CHECK_FALSE(popup.open);
        CHECK(index == 1);
    }
    CHECK(writes == 0);

    // Down from Banana passes the disabled Cherry; Enter chooses Date and closes.
    session.KeyPress(ImGuiKey_Space);
    session.KeyPress(ImGuiKey_DownArrow);
    session.KeyPress(ImGuiKey_Enter);
    CHECK(index == 3);
    CHECK(writes == 1);
    CHECK_FALSE(popup.open);
    CHECK(ImGui::GetFocusID() == session.ItemId("fruit"));

    // Up twice from Date: Banana (Cherry skipped), then Apple; Space chooses.
    session.KeyPress(ImGuiKey_Enter);
    session.KeyPress(ImGuiKey_UpArrow);
    session.KeyPress(ImGuiKey_UpArrow);
    session.KeyPress(ImGuiKey_Space);
    CHECK(index == 0);
    CHECK(writes == 2);
    // End jumps to the last enabled item, Home to the first.
    session.KeyPress(ImGuiKey_Enter);
    session.KeyPress(ImGuiKey_End);
    session.KeyPress(ImGuiKey_Enter);
    CHECK(index == 4);
    session.KeyPress(ImGuiKey_Enter);
    session.KeyPress(ImGuiKey_Home);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(index == 0);
}

TEST_CASE("Typing on a closed dropdown jumps to the next matching item and cycles")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    const char* const labels[] = {"Alpha", "Bravo", "Beta", "Charlie"};
    int index = 0;
    int writes = 0;
    PopupProbe popup;
    session.SetUi([&] {
        if (facet::Dropdown("Name", &index, labels))
            ++writes;
        session.Track("name");
        popup.Record("Name");
    });
    session.Frame();
    session.Click(session.ItemCenter("name"));
    session.KeyPress(ImGuiKey_Escape);  // focused, list closed
    REQUIRE(ImGui::GetFocusID() == session.ItemId("name"));
    session.Type("b");
    session.Frame();
    CHECK(index == 1);
    session.Type("b");
    session.Frame();
    CHECK(index == 2);
    CHECK(writes == 2);
    CHECK_FALSE(popup.open);
}

TEST_CASE("Typing in an open dropdown list moves the active row without choosing")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    const char* const labels[] = {"Alpha", "Bravo", "Charlie", "Delta"};
    int index = 0;
    PopupProbe popup;
    session.SetUi([&] {
        facet::Dropdown("Name", &index, labels);
        session.Track("name");
        popup.Record("Name");
    });
    session.Frame();
    session.Click(session.ItemCenter("name"));
    REQUIRE(popup.open);
    session.Type("c");
    session.Frame();
    CHECK(index == 0);
    session.KeyPress(ImGuiKey_Enter);
    CHECK(index == 2);
}

TEST_CASE("A searchable dropdown filters with the fuzzy matcher and Enter takes the best match")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int index = -1;
    PopupProbe popup;
    session.SetUi([&] {
        facet::Dropdown("Fruit", &index, kFruit, {.searchable = true});
        session.Track("fruit");
        popup.Record("Fruit");
    });
    session.Frame();
    session.Click(session.ItemCenter("fruit"));
    session.Frames(2);
    REQUIRE(popup.open);
    session.Type("eld");
    session.Frame();
    CHECK(popup.query == "eld");
    session.KeyPress(ImGuiKey_Enter);
    CHECK(index == 4);
    CHECK_FALSE(popup.open);
    CHECK(ImGui::GetFocusID() == session.ItemId("fruit"));

    // Escape in the filter closes the list without writing; the next opening starts unfiltered.
    session.Click(session.ItemCenter("fruit"));
    session.Frames(2);
    session.Type("ban");
    session.Frame();
    session.KeyPress(ImGuiKey_Escape);
    CHECK_FALSE(popup.open);
    CHECK(index == 4);
    CHECK(ImGui::GetFocusID() == session.ItemId("fruit"));
    session.KeyPress(ImGuiKey_Enter);
    session.Frames(2);
    CHECK(popup.open);
    CHECK(popup.query.empty());
}

TEST_CASE("A read-only or disabled dropdown does not open and never writes")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int index = 0;
    PopupProbe readOnly;
    PopupProbe disabled;
    session.SetUi([&] {
        facet::Dropdown("Read only", &index, kFruit, {.readOnly = true});
        session.Track("readonly");
        readOnly.Record("Read only");
        ImGui::BeginDisabled();
        facet::Dropdown("Disabled", &index, kFruit);
        session.Track("disabled");
        disabled.Record("Disabled");
        ImGui::EndDisabled();
    });
    session.Frame();
    session.Click(session.ItemCenter("readonly"));
    CHECK_FALSE(readOnly.open);
    session.Type("d");
    session.KeyPress(ImGuiKey_DownArrow);
    session.Click(session.ItemCenter("disabled"));
    CHECK_FALSE(disabled.open);
    CHECK(index == 0);
}

TEST_CASE("A dropdown list flips above the field when there is no room below")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int index = 0;
    PopupProbe popup;
    session.SetUi([&] {
        ImGui::SetCursorPosY(ImGui::GetWindowHeight() - facet::Px(60.0f));
        facet::Dropdown("Low", &index, kFruit);
        session.Track("low");
        popup.Record("Low");
    });
    session.Frame();
    session.Click(session.ItemCenter("low"));
    session.Frame();
    REQUIRE(popup.open);
    CHECK(popup.side == fi::PopupSide::Above);
    CHECK(popup.rect.Max.y <= session.ItemRect("low").Min.y);
}

TEST_CASE("A dropdown list follows its field when the field moves")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int index = 0;
    float offset = 0.0f;
    PopupProbe popup;
    session.SetUi([&] {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + offset);
        facet::Dropdown("Moving", &index, kFruit);
        session.Track("moving");
        popup.Record("Moving");
    });
    session.Frame();
    session.Click(session.ItemCenter("moving"));
    session.Frame();
    REQUIRE(popup.open);
    const float before = popup.rect.Min.y;
    offset = 40.0f;
    session.Frames(2);
    REQUIRE(popup.open);
    CHECK(popup.rect.Min.y == doctest::Approx(before + 40.0f));
}

TEST_CASE("A dropdown over ten thousand items reveals the chosen row and calls the getter for few rows")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int index = 7400;
    int calls = 0;
    PopupProbe popup;
    session.SetUi([&] {
        facet::Dropdown("Long", &index, 10000, &CountedOption, &calls);
        session.Track("long");
        popup.Record("Long");
    });
    session.Frame();
    session.Click(session.ItemCenter("long"));
    session.Frames(2);
    REQUIRE(popup.open);
    // Steady frames with the list open ask only for the rows shown.
    calls = 0;
    session.Frame();
    CHECK(calls < 100);
    // The active row starts on the chosen item: Down and Enter take the next one.
    session.KeyPress(ImGuiKey_DownArrow);
    session.KeyPress(ImGuiKey_Enter);
    CHECK(index == 7401);
    // The chosen row is in view when the list opens: a click in the middle of the list lands near it.
    session.Click(session.ItemCenter("long"));
    session.Frames(2);
    REQUIRE(popup.open);
    session.Click(popup.Row(0));
    CHECK(index > 7300);
    CHECK(index < 7500);
}

TEST_CASE("A dropdown shows group headers that cannot be chosen")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int index = -1;
    PopupProbe popup;
    session.SetUi([&] {
        facet::Dropdown("Grouped", &index, kGrouped);
        session.Track("grouped");
        popup.Record("Grouped");
    });
    session.Frame();
    session.Click(session.ItemCenter("grouped"));
    session.Frame();
    // Rows: header 1, Option 1, Option 2, header 2, Option 3.
    session.Click(popup.Row(0));
    CHECK(index == -1);
    CHECK(popup.open);
    session.Click(popup.Row(4));
    CHECK(index == 2);
    // From no choice, the active row starts on the first item, past the header.
    index = -1;
    session.Click(session.ItemCenter("grouped"));
    session.KeyPress(ImGuiKey_Enter);
    CHECK(index == 0);
}

TEST_CASE("A dropdown whose items are loading or failed keeps its value and shows no rows")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int index = 1;
    facet::DataState state = facet::DataState::Loading;
    PopupProbe popup;
    session.SetUi([&] {
        facet::Dropdown("Remote", &index, kFruit, {.state = state});
        session.Track("remote");
        popup.Record("Remote");
    });
    session.Frame();
    session.Click(session.ItemCenter("remote"));
    session.Frame();
    REQUIRE(popup.open);
    session.Click(popup.Row(0));
    session.KeyPress(ImGuiKey_Escape);
    CHECK(index == 1);
    state = facet::DataState::Error;
    session.Click(session.ItemCenter("remote"));
    session.KeyPress(ImGuiKey_Enter);
    CHECK(index == 1);
}

TEST_CASE("A mixed dropdown writes the item chosen, also the one it already held")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int index = 0;
    int writes = 0;
    PopupProbe popup;
    session.SetUi([&] {
        if (facet::Dropdown("Mixed", &index, kFruit, {.mixed = true}))
            ++writes;
        session.Track("mixed");
        popup.Record("Mixed");
    });
    session.Frame();
    session.Click(session.ItemCenter("mixed"));
    session.KeyPress(ImGuiKey_Enter);
    CHECK(index == 0);
    CHECK(writes == 1);
    CHECK_FALSE(popup.open);
}

TEST_CASE("A dropdown outside a frame or with a null index gives the neutral result")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int index = 2;
    CHECK_FALSE(facet::Dropdown("outside", &index, kFruit));
    PopupProbe popup;
    session.SetUi([&] {
        CHECK_FALSE(facet::Dropdown("Null", nullptr, kFruit));
        session.Track("null");
        popup.Record("Null");
    });
    session.Frame();
    session.Click(session.ItemCenter("null"));
    CHECK_FALSE(popup.open);
    CHECK(index == 2);
}

// ---- ListBox and SelectableList ----------------------------------------------------------------

namespace
{

// The center of row `row` of a framed list `rect` (the tracked widget, without a message line)
// showing `visibleRows` rows.
ImVec2 BoxRowCenter(const ImRect& rect, int visibleRows, int row, facet::ControlSize size = facet::ControlSize::Regular)
{
    const float height = fi::ListRowHeight(size);
    const float top = rect.Max.y - fi::FocusRingOutset() - height * static_cast<float>(visibleRows);
    return ImVec2(rect.GetCenter().x, top + height * (static_cast<float>(row) + 0.5f));
}

} // namespace

TEST_CASE("A list box chooses the clicked row, skips disabled rows and group headers")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int index = 0;
    int grouped = -1;
    int writes = 0;
    session.SetUi([&] {
        if (facet::ListBox("Fruit", &index, kFruit, {.visibleRows = 5}))
            ++writes;
        session.Track("fruit");
        facet::ListBox("Grouped", &grouped, kGrouped, {.visibleRows = 5});
        session.Track("grouped");
    });
    session.Frame();
    session.Click(BoxRowCenter(session.ItemRect("fruit"), 5, 3));
    CHECK(index == 3);
    CHECK(writes == 1);
    session.Click(BoxRowCenter(session.ItemRect("fruit"), 5, 2));
    CHECK(index == 3);
    // Rows: header 1, Option 1, Option 2, header 2, Option 3.
    session.Click(BoxRowCenter(session.ItemRect("grouped"), 5, 0));
    CHECK(grouped == -1);
    session.Click(BoxRowCenter(session.ItemRect("grouped"), 5, 4));
    CHECK(grouped == 2);
    // The arrows skip the header between the groups.
    session.KeyPress(ImGuiKey_UpArrow);
    CHECK(grouped == 1);
}

TEST_CASE("A list box is one Tab stop whose arrows, Home, End and typing move the choice")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    const char* const labels[] = {"Alpha", "Bravo", "Charlie", "Delta", "Echo"};
    int index = 1;
    int writes = 0;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        if (facet::ListBox("Names", &index, labels, {.visibleRows = 3}))
            ++writes;
        ImGui::Button("after");
        session.Track("after");
    });
    session.Frame();
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_DownArrow);
    CHECK(index == 2);
    session.KeyPress(ImGuiKey_End);
    CHECK(index == 4);
    session.KeyPress(ImGuiKey_Home);
    CHECK(index == 0);
    session.Type("d");
    session.Frame();
    CHECK(index == 3);
    CHECK(writes == 4);
    // One Tab leaves the list.
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("after"));
}

TEST_CASE("A read-only list box moves the focus but never writes")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int index = 1;
    session.SetUi([&] {
        facet::ListBox("Fruit", &index, kFruit, {.visibleRows = 5, .readOnly = true});
        session.Track("fruit");
    });
    session.Frame();
    session.Click(BoxRowCenter(session.ItemRect("fruit"), 5, 3));
    session.KeyPress(ImGuiKey_DownArrow);
    CHECK(index == 1);
}

TEST_CASE("A list box keeps its height and shows the loading, error and empty states")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int index = 0;
    facet::DataState state = facet::DataState::Ready;
    std::span<const facet::ChoiceItem> items = kFruit;
    float readyHeight = 0.0f;
    bool result = false;
    session.SetUi([&] {
        result = facet::ListBox("Remote", &index, items, {.visibleRows = 3, .state = state});
        session.Track("remote");
    });
    session.Frame();
    readyHeight = session.ItemRect("remote").GetHeight();
    for (const facet::DataState s : {facet::DataState::Loading, facet::DataState::Error})
    {
        state = s;
        session.Frame();
        CHECK(session.ItemRect("remote").GetHeight() == doctest::Approx(readyHeight));
        session.Click(BoxRowCenter(session.ItemRect("remote"), 3, 1));
        CHECK(index == 0);
        CHECK_FALSE(result);
    }
    state = facet::DataState::Ready;
    items = {};
    session.Frame();
    CHECK(session.ItemRect("remote").GetHeight() == doctest::Approx(readyHeight));
}

TEST_CASE("A list box over a long getter list asks only for the rows shown")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int index = 5000;
    int calls = 0;
    session.SetUi([&] {
        facet::ListBox("Long", &index, 100000, &CountedOption, &calls, {.visibleRows = 6});
        session.Track("long");
    });
    session.Frames(2);
    calls = 0;
    session.Frame();
    CHECK(calls < 200);
}

TEST_CASE("A selectable list selects by click, Ctrl and Shift click, leaving disabled rows out")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ImGuiSelectionBasicStorage selection;
    int changes = 0;
    session.SetUi([&] {
        if (facet::SelectableList("Fruit", &selection, kFruit, {.visibleRows = 5}))
            ++changes;
        session.Track("fruit");
    });
    session.Frame();
    const auto row = [&](int i) { return BoxRowCenter(session.ItemRect("fruit"), 5, i); };
    session.Click(row(0));
    CHECK(selection.Size == 1);
    CHECK(selection.Contains(0));
    // Shift+click: the range 0 to 4 without the disabled Cherry.
    session.KeyDown(ImGuiMod_Shift);
    session.Click(row(4));
    session.KeyUp(ImGuiMod_Shift);
    CHECK(selection.Size == 4);
    CHECK_FALSE(selection.Contains(2));
    // Ctrl+click toggles one row.
    session.KeyDown(ImGuiMod_Ctrl);
    session.Click(row(1));
    session.KeyUp(ImGuiMod_Ctrl);
    CHECK(selection.Size == 3);
    CHECK_FALSE(selection.Contains(1));
    // A plain click selects only that row.
    session.Click(row(3));
    CHECK(selection.Size == 1);
    CHECK(selection.Contains(3));
    CHECK(changes >= 4);
}

TEST_CASE("A selectable list extends with Shift and arrows, selects all with Ctrl+A and clears with Escape")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ImGuiSelectionBasicStorage selection;
    session.SetUi([&] {
        facet::SelectableList("Fruit", &selection, kFruit, {.visibleRows = 5});
        session.Track("fruit");
    });
    session.Frame();
    session.Click(BoxRowCenter(session.ItemRect("fruit"), 5, 0));
    session.KeyPress(ImGuiKey_DownArrow | ImGuiMod_Shift);
    CHECK(selection.Size == 2);
    session.KeyPress(ImGuiKey_DownArrow | ImGuiMod_Shift);
    CHECK(selection.Size == 3);  // Cherry is skipped by the arrows and left out
    CHECK(selection.Contains(3));
    CHECK_FALSE(selection.Contains(2));
    session.KeyPress(ImGuiKey_A | ImGuiMod_Ctrl);
    CHECK(selection.Size == 4);
    session.KeyPress(ImGuiKey_Escape);
    CHECK(selection.Size == 0);
}

TEST_CASE("A selectable list reports the row activated by double click or Enter")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ImGuiSelectionBasicStorage selection;
    int activated = -1;
    session.SetUi([&] {
        facet::SelectableList("Fruit", &selection, kFruit, {.visibleRows = 5});
        session.Track("fruit");
        if (facet::SelectableListActivatedRow() >= 0)
            activated = facet::SelectableListActivatedRow();
    });
    session.Frame();
    session.DoubleClick(BoxRowCenter(session.ItemRect("fruit"), 5, 1));
    CHECK(activated == 1);
    session.KeyPress(ImGuiKey_DownArrow);
    session.KeyPress(ImGuiKey_DownArrow);
    session.KeyPress(ImGuiKey_Enter);
    CHECK(activated == 4);
    CHECK(facet::SelectableListActivatedRow() == -1);  // outside the frame of the call
}

TEST_CASE("A read-only selectable list or one without storage changes nothing")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ImGuiSelectionBasicStorage selection;
    selection.SetItemSelected(1, true);
    bool changed = false;
    session.SetUi([&] {
        changed |= facet::SelectableList("Read only", &selection, kFruit, {.visibleRows = 5, .readOnly = true});
        session.Track("readonly");
        changed |= facet::SelectableList("No storage", nullptr, kFruit, {.visibleRows = 5});
        session.Track("none");
    });
    session.Frame();
    session.Click(BoxRowCenter(session.ItemRect("readonly"), 5, 3));
    session.KeyPress(ImGuiKey_A | ImGuiMod_Ctrl);
    session.Click(BoxRowCenter(session.ItemRect("none"), 5, 3));
    CHECK_FALSE(changed);
    CHECK(selection.Size == 1);
    CHECK(selection.Contains(1));
}

TEST_CASE("Tab and Shift+Tab enter and leave a selectable list without changing the selection")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ImGuiSelectionBasicStorage selection;
    selection.SetItemSelected(3, true);
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        facet::SelectableList("Fruit", &selection, kFruit, {.visibleRows = 5});
        session.Track("fruit");
        ImGui::Button("after");
        session.Track("after");
    });
    session.Frame();
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    const ImGuiID row = ImGui::GetFocusID();
    CHECK(row != session.ItemId("before"));
    CHECK(row != session.ItemId("after"));
    CHECK(selection.Size == 1);
    CHECK(selection.Contains(3));
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("after"));
    CHECK(selection.Size == 1);
    session.KeyPress(ImGuiKey_Tab | ImGuiMod_Shift);
    CHECK(ImGui::GetFocusID() == row);
    CHECK(selection.Size == 1);
    session.KeyPress(ImGuiKey_Tab | ImGuiMod_Shift);
    CHECK(ImGui::GetFocusID() == session.ItemId("before"));
    CHECK(selection.Size == 1);
}

// ---- MultiDropdown ----------------------------------------------------------------------------

TEST_CASE("A multi dropdown toggles rows by click and keeps its list open")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::array<bool, 5> checked = {false, true, false, false, false};
    int writes = 0;
    PopupProbe popup;
    session.SetUi([&] {
        if (facet::MultiDropdown("Fruit", checked, kFruit))
            ++writes;
        session.Track("fruit");
        popup.Record("Fruit");
    });
    session.Frame();
    session.Click(session.ItemCenter("fruit"));
    REQUIRE(popup.open);
    session.Click(popup.Row(0));
    CHECK(checked[0]);
    CHECK(popup.open);
    session.Click(popup.Row(1));
    CHECK_FALSE(checked[1]);
    session.Click(popup.Row(2));  // disabled
    CHECK_FALSE(checked[2]);
    CHECK(writes == 2);
    CHECK(popup.open);
    session.Click(session.ItemCenter("fruit"));
    CHECK_FALSE(popup.open);
}

TEST_CASE("A multi dropdown opens and toggles with the keyboard; Tab and Escape close it")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::array<bool, 5> checked = {};
    PopupProbe popup;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        facet::MultiDropdown("Fruit", checked, kFruit);
        session.Track("fruit");
        popup.Record("Fruit");
    });
    session.Frame();
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Space);
    REQUIRE(popup.open);
    session.KeyPress(ImGuiKey_Space);
    CHECK(checked[0]);
    session.KeyPress(ImGuiKey_DownArrow);
    session.KeyPress(ImGuiKey_Enter);
    CHECK(checked[1]);
    CHECK(popup.open);
    session.KeyPress(ImGuiKey_Tab);
    CHECK_FALSE(popup.open);
    CHECK(checked[0]);
    CHECK(checked[1]);
    CHECK(ImGui::GetFocusID() == session.ItemId("fruit"));
    session.KeyPress(ImGuiKey_DownArrow);
    CHECK(popup.open);
    session.KeyPress(ImGuiKey_Escape);
    CHECK_FALSE(popup.open);
}

TEST_CASE("The select-all row of a multi dropdown checks or clears every enabled item")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::array<bool, 5> checked = {true, false, false, false, false};
    PopupProbe popup;
    session.SetUi([&] {
        facet::MultiDropdown("Fruit", checked, kFruit, {.selectAllRow = true});
        session.Track("fruit");
        popup.Record("Fruit");
    });
    session.Frame();
    session.Click(session.ItemCenter("fruit"));
    session.Click(popup.Row(0));  // Mixed: checks every enabled item
    CHECK(checked == std::array<bool, 5>{true, true, false, true, true});
    session.Seconds(0.5f);        // not a double click
    session.Click(popup.Row(0));  // On: clears them
    CHECK(checked == std::array<bool, 5>{false, false, false, false, false});
}

TEST_CASE("A multi dropdown never writes entries missing from its span, and a read-only one does not open")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::array<bool, 2> shortSpan = {};
    std::array<bool, 5> checked = {};
    PopupProbe popup;
    PopupProbe readOnly;
    session.SetUi([&] {
        facet::MultiDropdown("Short", shortSpan, kFruit, {.selectAllRow = true});
        session.Track("short");
        popup.Record("Short");
        facet::MultiDropdown("Read only", checked, kFruit, {.readOnly = true});
        session.Track("readonly");
        readOnly.Record("Read only");
    });
    session.Frame();
    session.Click(session.ItemCenter("short"));
    session.Click(popup.Row(0));
    CHECK(shortSpan == std::array<bool, 2>{true, true});
    session.Click(popup.Row(5));  // Elderberry: no entry to write
    CHECK(shortSpan == std::array<bool, 2>{true, true});
    session.KeyPress(ImGuiKey_Escape);
    session.Click(session.ItemCenter("readonly"));
    CHECK_FALSE(readOnly.open);
}

TEST_CASE("A searchable multi dropdown filters its rows and toggles the active one with Enter")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::array<bool, 5> checked = {};
    PopupProbe popup;
    session.SetUi([&] {
        facet::MultiDropdown("Fruit", checked, kFruit, {.searchable = true});
        session.Track("fruit");
        popup.Record("Fruit");
    });
    session.Frame();
    session.Click(session.ItemCenter("fruit"));
    session.Frames(2);
    session.Type("date");
    session.Frame();
    session.KeyPress(ImGuiKey_Enter);
    CHECK(checked[3]);
    CHECK(popup.open);
    session.KeyPress(ImGuiKey_Escape);
    CHECK_FALSE(popup.open);
}

// ---- BeginCombo, ComboItem, EndCombo --------------------------------------------------------------

namespace
{

// A combo over kFruit whose rows are ComboItems; returns the row chosen this frame or -1.
int FruitCombo(const char* label, int current, const facet::ComboOptions& options = {})
{
    int chosen = -1;
    if (facet::BeginCombo(label, kFruit[current].label, options))
    {
        for (int i = 0; i < static_cast<int>(std::size(kFruit)); ++i)
        {
            ImGui::PushID(i);
            ImGui::BeginDisabled(kFruit[i].disabled);
            if (facet::ComboItem(kFruit[i].label, i == current, {.detail = kFruit[i].detail}))
                chosen = i;
            ImGui::EndDisabled();
            ImGui::PopID();
        }
        facet::EndCombo();
    }
    return chosen;
}

} // namespace

TEST_CASE("A combo opens on a click, its rows are chosen by click, and it closes")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int current = 0;
    PopupProbe popup;
    session.SetUi([&] {
        if (const int chosen = FruitCombo("Fruit", current); chosen >= 0)
            current = chosen;
        session.Track("fruit");
        popup.Record("Fruit");
    });
    session.Frame();
    session.Click(session.ItemCenter("fruit"));
    REQUIRE(popup.open);
    session.Click(popup.Row(3));
    CHECK(current == 3);
    CHECK_FALSE(popup.open);
    // A disabled row is shown but refused.
    session.Click(session.ItemCenter("fruit"));
    session.Click(popup.Row(2));
    CHECK(current == 3);
    CHECK(popup.open);
}

TEST_CASE("A combo opens with Enter on the selected row; the arrows and Enter choose, Escape closes")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int current = 3;
    PopupProbe popup;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        if (const int chosen = FruitCombo("Fruit", current); chosen >= 0)
            current = chosen;
        session.Track("fruit");
        popup.Record("Fruit");
    });
    session.Frame();
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Enter);
    REQUIRE(popup.open);
    session.KeyPress(ImGuiKey_DownArrow);
    session.KeyPress(ImGuiKey_Enter);
    CHECK(current == 4);
    CHECK_FALSE(popup.open);
    CHECK(ImGui::GetFocusID() == session.ItemId("fruit"));
    session.KeyPress(ImGuiKey_DownArrow);
    CHECK(popup.open);
    session.KeyPress(ImGuiKey_UpArrow);
    session.KeyPress(ImGuiKey_Escape);
    CHECK_FALSE(popup.open);
    CHECK(current == 4);
    CHECK(ImGui::GetFocusID() == session.ItemId("fruit"));
}

TEST_CASE("A combo list is at most maxVisibleRows rows tall and a read-only combo does not open")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    PopupProbe shortList;
    PopupProbe readOnly;
    session.SetUi([&] {
        FruitCombo("Small", 0, {.maxVisibleRows = 2});
        session.Track("small");
        shortList.Record("Small");
        FruitCombo("Read only", 0, {.readOnly = true});
        session.Track("readonly");
        readOnly.Record("Read only");
    });
    session.Frame();
    session.Click(session.ItemCenter("small"));
    session.Frames(2);
    REQUIRE(shortList.open);
    const float rows = fi::ListRowHeight(facet::ControlSize::Regular);
    CHECK(shortList.rect.GetHeight() < rows * 3.0f);
    CHECK(shortList.rect.GetHeight() >= rows * 2.0f);
    session.KeyPress(ImGuiKey_Escape);
    session.Click(session.ItemCenter("readonly"));
    CHECK_FALSE(readOnly.open);
}

TEST_CASE("A combo item outside a combo is reported and returns false")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool result = true;
    int reported = 0;
    session.SetUi([&] {
        const int before = facet::internal::UsageErrorCount();
        result = facet::ComboItem("Stray", true);
        reported = facet::internal::UsageErrorCount() - before;
    });
    session.Frame();
    CHECK_FALSE(result);
    CHECK(reported == 1);
}

// ---- TreeView -------------------------------------------------------------------------------------

namespace
{

// A tree of two groups: Group 1 (Item 1, Item 2), Group 2 (Item 3), and a top-level Item 4.
struct TreeUi
{
    ImGuiSelectionBasicStorage selection;
    facet::TreeViewOptions options;
    bool group2Loading = false;
    bool group2Toggled = false;
    int activated = 0;
    bool open = false;
    bool selectionChanged = false;

    void Ui(facet::test::Session& session)
    {
        open = facet::BeginTreeView("tree", &selection, options);
        session.Track("tree");
        if (!open)
            return;
        if (facet::BeginTreeNode("Group 1", {.icon = facet::Icon("folder"), .defaultOpen = true, .id = 1}))
        {
            session.Track("group1");
            if (facet::TreeLeaf("Item 1", {.id = 11}))
                activated = 11;
            session.Track("item1");
            if (facet::TreeLeaf("Item 2", {.id = 12}))
                activated = 12;
            session.Track("item2");
            facet::EndTreeNode();
        }
        else
        {
            session.Track("group1");
        }
        const bool group2 = facet::BeginTreeNode("Group 2", {.id = 2, .loading = group2Loading});
        session.Track("group2");
        if (ImGui::IsItemToggledOpen())
            group2Toggled = true;
        if (group2)
        {
            if (!group2Loading && facet::TreeLeaf("Item 3", {.id = 21}))
                activated = 21;
            session.Track("item3");
            facet::EndTreeNode();
        }
        if (facet::TreeLeaf("Item 4", {.id = 4}))
            activated = 4;
        session.Track("item4");
        facet::EndTreeView();
        selectionChanged = selectionChanged || facet::TreeViewSelectionChanged();
    }
};

} // namespace

TEST_CASE("A tree view selects nodes by click, Ctrl and Shift click, and keeps them when a parent closes")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TreeUi ui;
    session.SetUi([&] { ui.Ui(session); });
    session.Frames(2);
    session.Click(session.ItemCenter("item1"));
    CHECK(ui.selection.Contains(11));
    CHECK(ui.selectionChanged);
    session.KeyDown(ImGuiMod_Ctrl);
    session.Click(session.ItemCenter("item4"));
    session.KeyUp(ImGuiMod_Ctrl);
    CHECK(ui.selection.Size == 2);
    // Closing Group 1 with its expander keeps Item 1 selected.
    const ImRect group = session.ItemRect("group1");
    const float indent = facet::Px(facet::T().list.indent);
    session.Click(ImVec2(group.Min.x + facet::Px(facet::T().spacing.xs) + indent * 0.5f, group.GetCenter().y));
    CHECK(ui.selection.Contains(11));
    CHECK(ui.selection.Size == 2);
    // Shift+click from Item 4 up to Group 1 selects the visible nodes between.
    session.KeyDown(ImGuiMod_Shift);
    session.Click(session.ItemCenter("group1"));
    session.KeyUp(ImGuiMod_Shift);
    CHECK(ui.selection.Contains(1));
    CHECK(ui.selection.Contains(2));
    CHECK(ui.selection.Contains(4));
}

TEST_CASE("Left and Right close, open and move between parent and child in a tree view")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TreeUi ui;
    session.SetUi([&] { ui.Ui(session); });
    session.Frames(2);
    session.Click(session.ItemCenter("item2"));
    session.KeyPress(ImGuiKey_LeftArrow);  // a leaf: to its parent
    CHECK(ImGui::GetFocusID() == session.ItemId("group1"));
    session.KeyPress(ImGuiKey_LeftArrow);  // an open node: closes
    session.Frame();
    CHECK(session.ItemRect("item1").GetHeight() == 0.0f);
    session.KeyPress(ImGuiKey_RightArrow);  // a closed node: opens
    session.Frame();
    CHECK(session.ItemRect("item1").GetHeight() > 0.0f);
    session.KeyPress(ImGuiKey_RightArrow);  // an open node: to its first child
    CHECK(ImGui::GetFocusID() == session.ItemId("item1"));
    session.KeyPress(ImGuiKey_Enter);
    CHECK(ui.activated == 11);
    session.KeyPress(ImGuiKey_DownArrow);
    session.KeyPress(ImGuiKey_DownArrow);
    CHECK(ImGui::GetFocusID() == session.ItemId("group2"));
}

TEST_CASE("A tree view reports a toggle for lazy children and shows a loading row under an open node")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TreeUi ui;
    ui.group2Loading = true;
    session.SetUi([&] { ui.Ui(session); });
    session.Frames(2);
    const float before = session.ItemRect("item4").Min.y;
    session.Click(session.ItemCenter("group2"));
    session.KeyPress(ImGuiKey_RightArrow);
    session.Frame();
    CHECK(ui.group2Toggled);
    // The loading row takes one row between Group 2 and Item 4.
    CHECK(session.ItemRect("item4").Min.y == doctest::Approx(before + fi::ListRowHeight(facet::ControlSize::Regular)));
}

TEST_CASE("Tree rows of every level span the width of the view at the list row height")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TreeUi ui;
    session.SetUi([&] { ui.Ui(session); });
    session.Frames(2);
    // The indent (list.indent per level) moves the row content, not the row: the selection and hover
    // fills of a child reach as far left as its parent's.
    CHECK(session.ItemRect("item1").Min.x == doctest::Approx(session.ItemRect("group1").Min.x));
    CHECK(session.ItemRect("item1").GetHeight() == doctest::Approx(fi::ListRowHeight(facet::ControlSize::Regular)));
}

TEST_CASE("A tree view in the loading or error state draws the state and returns false")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TreeUi ui;
    ui.options.state = facet::DataState::Loading;
    session.SetUi([&] { ui.Ui(session); });
    session.Frames(2);
    CHECK_FALSE(ui.open);
    CHECK(session.ItemRect("tree").GetHeight() >= fi::ListRowHeight(facet::ControlSize::Regular));
    ui.options.state = facet::DataState::Error;
    session.Frame();
    CHECK_FALSE(ui.open);
    CHECK(facet::internal::OpenScopeCount() == 0);
}

TEST_CASE("Tab enters a tree view with a selection at its first node and leaves it")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TreeUi ui;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        ui.Ui(session);
        ImGui::Button("after");
        session.Track("after");
    });
    session.Frames(2);
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("group1"));
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("after"));
    CHECK(ui.selection.Size == 0);
}

// ---- ReorderableList ------------------------------------------------------------------------------

namespace
{

// A reorderable list over five items with stable ids that applies every move it reports.
struct ReorderUi
{
    std::array<facet::ChoiceItem, 5> items = {{{.label = "Item 1", .id = 1},
                                               {.label = "Item 2", .id = 2},
                                               {.label = "Item 3", .id = 3},
                                               {.label = "Item 4", .id = 4},
                                               {.label = "Item 5", .id = 5}}};
    facet::ReorderableListOptions options;
    facet::ReorderMove last;
    int moves = 0;

    void Ui(facet::test::Session& session)
    {
        facet::ReorderMove move;
        if (facet::ReorderableList("reorder", &move, items, options))
        {
            last = move;
            ++moves;
            facet::ApplyReorderMove(items, move);
        }
        session.Track("reorder");
    }

    // The ids in their current order.
    std::array<ImGuiID, 5> Order() const
    {
        std::array<ImGuiID, 5> order{};
        for (std::size_t i = 0; i < items.size(); ++i)
            order[i] = items[i].id;
        return order;
    }

    static ImVec2 Row(const facet::test::Session& session, int index, float fraction = 0.5f)
    {
        const ImRect rect = session.ItemRect("reorder");
        const float height = fi::ListRowHeight(facet::ControlSize::Regular);
        return ImVec2(rect.GetCenter().x, rect.Min.y + height * (static_cast<float>(index) + fraction));
    }
};

} // namespace

TEST_CASE("Dragging a row reports the move in both directions")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ReorderUi ui;
    session.SetUi([&] { ui.Ui(session); });
    session.Frames(2);
    // Item 1 dropped in the lower half of row 2 lands after Item 3: from 0 to 2.
    session.Drag({ReorderUi::Row(session, 0), ReorderUi::Row(session, 1), ReorderUi::Row(session, 2, 0.8f)}, 6);
    session.Frames(2);
    CHECK(ui.moves == 1);
    CHECK(ui.last.from == 0);
    CHECK(ui.last.to == 2);
    CHECK(ui.Order() == std::array<ImGuiID, 5>{2, 3, 1, 4, 5});
    // Item 5 dropped on the upper half of row 0 lands first: from 4 to 0.
    session.Drag({ReorderUi::Row(session, 4), ReorderUi::Row(session, 2), ReorderUi::Row(session, 0, 0.2f)}, 6);
    session.Frames(2);
    CHECK(ui.moves == 2);
    CHECK(ui.last.from == 4);
    CHECK(ui.last.to == 0);
    CHECK(ui.Order() == std::array<ImGuiID, 5>{5, 2, 3, 1, 4});
}

TEST_CASE("A drag canceled with Escape or dropped on its own place moves nothing")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ReorderUi ui;
    session.SetUi([&] { ui.Ui(session); });
    session.Frames(2);
    session.MouseMove(ReorderUi::Row(session, 1));
    session.MouseDown();
    session.MouseMove(ReorderUi::Row(session, 2));
    session.MouseMove(ReorderUi::Row(session, 3, 0.8f));
    session.Frames(2);
    session.KeyPress(ImGuiKey_Escape);
    session.MouseUp();
    session.Frames(2);
    CHECK(ui.moves == 0);
    session.Drag({ReorderUi::Row(session, 2), ReorderUi::Row(session, 2, 0.9f), ReorderUi::Row(session, 2, 0.6f)}, 6);
    session.Frames(2);
    CHECK(ui.moves == 0);
    CHECK(ui.Order() == std::array<ImGuiID, 5>{1, 2, 3, 4, 5});
}

TEST_CASE("Alt+Up and Alt+Down move the focused row and the focus stays on it")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ReorderUi ui;
    session.SetUi([&] { ui.Ui(session); });
    session.Frames(2);
    session.Click(ReorderUi::Row(session, 1));
    session.KeyPress(ImGuiKey_DownArrow | ImGuiMod_Alt);
    CHECK(ui.Order() == std::array<ImGuiID, 5>{1, 3, 2, 4, 5});
    CHECK(ui.last.from == 1);
    CHECK(ui.last.to == 2);
    session.KeyPress(ImGuiKey_DownArrow | ImGuiMod_Alt);
    CHECK(ui.Order() == std::array<ImGuiID, 5>{1, 3, 4, 2, 5});
    session.KeyPress(ImGuiKey_UpArrow | ImGuiMod_Alt);
    session.KeyPress(ImGuiKey_UpArrow | ImGuiMod_Alt);
    session.KeyPress(ImGuiKey_UpArrow | ImGuiMod_Alt);
    CHECK(ui.Order() == std::array<ImGuiID, 5>{2, 1, 3, 4, 5});
    // At the top Alt+Up does nothing.
    session.KeyPress(ImGuiKey_UpArrow | ImGuiMod_Alt);
    CHECK(ui.Order() == std::array<ImGuiID, 5>{2, 1, 3, 4, 5});
    CHECK(ui.moves == 5);
}

TEST_CASE("Space picks a row up, the arrows move it, Space drops it and Escape puts it back")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ReorderUi ui;
    session.SetUi([&] { ui.Ui(session); });
    session.Frames(2);
    session.Click(ReorderUi::Row(session, 0));
    session.KeyPress(ImGuiKey_Space);
    session.KeyPress(ImGuiKey_DownArrow);
    session.KeyPress(ImGuiKey_DownArrow);
    session.KeyPress(ImGuiKey_Escape);
    session.Frames(2);
    CHECK(ui.moves == 0);
    session.KeyPress(ImGuiKey_Space);
    session.KeyPress(ImGuiKey_DownArrow);
    session.KeyPress(ImGuiKey_DownArrow);
    session.KeyPress(ImGuiKey_Space);
    session.Frames(2);
    CHECK(ui.moves == 1);
    CHECK(ui.last.from == 0);
    CHECK(ui.last.to == 2);
    CHECK(ui.Order() == std::array<ImGuiID, 5>{2, 3, 1, 4, 5});
}

TEST_CASE("A read-only reorderable list moves nothing")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ReorderUi ui;
    ui.options.readOnly = true;
    session.SetUi([&] { ui.Ui(session); });
    session.Frames(2);
    session.Drag({ReorderUi::Row(session, 0), ReorderUi::Row(session, 3, 0.8f)}, 6);
    session.Click(ReorderUi::Row(session, 1));
    session.KeyPress(ImGuiKey_DownArrow | ImGuiMod_Alt);
    session.Frames(2);
    CHECK(ui.moves == 0);
}

TEST_CASE("A drag resting at the edge of a scroll area around a reorderable list scrolls it")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ReorderUi ui;
    float scroll = 0.0f;
    session.SetUi([&] {
        if (fi::BeginScrollAreaCore("area", {.size = ImVec2(300.0f, 90.0f)}))
        {
            ui.Ui(session);
            scroll = ImGui::GetScrollY();
            fi::EndScrollAreaCore();
        }
        session.Track("area");
    });
    session.Frames(2);
    const ImRect area = session.ItemRect("area");
    const ImVec2 edge(area.GetCenter().x, area.Max.y - 4.0f);
    session.DragAndHold({ReorderUi::Row(session, 0), edge}, 6, 30);
    session.Frames(2);
    CHECK(scroll > 0.0f);
}
