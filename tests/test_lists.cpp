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

#include "internal/focus.h"
#include "internal/list_view.h"
#include "internal/popup_host.h"
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
