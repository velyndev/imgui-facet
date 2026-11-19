// List and selection core (src/internal/list_core.h) and the list view and tree built on it
// (src/internal/list_view.h): row ranges, virtualization, selection against Dear ImGui's stock
// multi-select lists, keyboard, typeahead, trees, data states and per-frame allocation.

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>
#include <vector>

#ifdef _DEBUG
#include <crtdbg.h>
#endif

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "core/core_internal.h"
#include "internal/list_core.h"
#include "internal/list_view.h"
#include "internal/scroll.h"
#include "support/session.h"

namespace fi = facet::internal;

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

struct Items
{
    std::vector<std::string> labels;
    std::vector<bool> disabled;
    std::vector<ImGuiID> ids;
};

void ItemsEntry(void* user, int index, fi::ListEntryInfo* out)
{
    const Items& items = *static_cast<const Items*>(user);
    out->label = items.labels[static_cast<size_t>(index)].c_str();
    out->disabled = index < static_cast<int>(items.disabled.size()) && items.disabled[static_cast<size_t>(index)];
    out->id = index < static_cast<int>(items.ids.size()) ? items.ids[static_cast<size_t>(index)] : 0;
}

// Generated rows for the long lists: "Row 0000000" and up, every 1000th disabled when asked.
bool g_generatedDisabled = false;
void GeneratedEntry(void*, int index, fi::ListEntryInfo* out)
{
    static char label[32];
    std::snprintf(label, sizeof(label), "Row %07d", index);
    out->label = label;
    out->disabled = g_generatedDisabled && index % 1000 == 999;
}

Items Fruits()
{
    Items items;
    items.labels = {"Apple", "apricot", "Banana", "Blueberry", "Cherry", "Date", "Elderberry", "Fig"};
    return items;
}

// A list view in a 300 x 240 area that records the rows handed out each frame.
struct ListUi
{
    fi::ListViewDesc desc;
    fi::ListViewResult result;
    std::vector<int> rows;
    bool track = true;
    ImGuiID windowId = 0;

    void Ui(facet::test::Session& session)
    {
        rows.clear();
        result = {};
        if (desc.areaSize.x == 0.0f)
            desc.areaSize = ImVec2(300.0f, 240.0f);
        if (fi::BeginListView(ImGui::GetID("list"), desc))
        {
            windowId = ImGui::GetCurrentWindow()->ID;
            facet::RowRange range;
            char name[32];
            while (facet::NextVisibleRows(&range))
            {
                for (int i = range.first; i < range.last; ++i)
                {
                    fi::ListViewRow(i);
                    rows.push_back(i);
                    if (track)
                    {
                        std::snprintf(name, sizeof(name), "row%d", i);
                        session.Track(name);
                    }
                }
            }
            result = fi::EndListView();
        }
    }
    ImGuiWindow* Window() const { return ImGui::FindWindowByID(windowId); }
    bool Submitted(int index) const { return std::find(rows.begin(), rows.end(), index) != rows.end(); }
    bool Unique() const
    {
        std::set<int> seen(rows.begin(), rows.end());
        return seen.size() == rows.size();
    }
};

ImVec2 Row(const facet::test::Session& session, int index)
{
    char name[32];
    std::snprintf(name, sizeof(name), "row%d", index);
    return session.ItemCenter(name);
}

ImGuiID RowId(const facet::test::Session& session, int index)
{
    char name[32];
    std::snprintf(name, sizeof(name), "row%d", index);
    return session.ItemId(name);
}

std::set<ImGuiID> Selected(ImGuiSelectionBasicStorage& selection)
{
    std::set<ImGuiID> out;
    void* it = nullptr;
    ImGuiID id = 0;
    while (selection.GetNextSelectedItem(&it, &id))
        out.insert(id);
    return out;
}

void ClickWith(facet::test::Session& session, ImVec2 at, ImGuiKeyChord mods)
{
    if (mods != 0)
        session.KeyDown(mods);
    session.Click(at);
    if (mods != 0)
        session.KeyUp(mods);
}

} // namespace

TEST_CASE("List rows: only the visible lines, whole lines for grids, keepAlive out of view")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    int count = 100;
    int columns = 1;
    int keepAlive = -1;
    bool scrollToEnd = false;
    std::vector<facet::RowRange> ranges;
    session.SetUi([&] {
        ranges.clear();
        if (fi::BeginScrollAreaCore("rows", {.size = ImVec2(300.0f, 100.0f)}))
        {
            fi::BeginListRows({.rowCount = count, .rowHeight = 30.0f, .columns = columns, .keepAlive = keepAlive});
            facet::RowRange range;
            while (fi::NextListRows(&range))
            {
                ranges.push_back(range);
                for (int i = range.first; i < range.last; i += columns)
                    ImGui::Dummy(ImVec2(10.0f, 30.0f - ImGui::GetStyle().ItemSpacing.y));
            }
            fi::EndListRows();
            if (scrollToEnd)
                ImGui::SetScrollY(ImGui::GetScrollMaxY());
            fi::EndScrollAreaCore();
        }
    });
    session.Frames(2);
    REQUIRE(!ranges.empty());
    CHECK(ranges.front().first == 0);
    const int visible = ranges.back().last - ranges.front().first;
    CHECK(visible <= 6);
    // Scrolled to the end, keepAlive 0 is still handed out, before the visible range.
    keepAlive = 0;
    scrollToEnd = true;
    session.Frames(3);
    REQUIRE(ranges.size() == 2);
    CHECK(ranges[0].first == 0);
    CHECK(ranges[0].last == 1);
    CHECK(ranges[1].last == count);
    // A grid: ranges hold whole lines of four tiles, the last line may be short.
    columns = 4;
    count = 10;
    keepAlive = -1;
    scrollToEnd = false;
    session.Frames(2);
    REQUIRE(!ranges.empty());
    for (const facet::RowRange& range : ranges)
        CHECK(range.first % 4 == 0);
    CHECK(ranges.back().last == 10);
    // Outside any range nothing is handed out.
    facet::RowRange outside{3, 4};
    CHECK_FALSE(fi::NextListRows(&outside));
    CHECK(outside.first == 0);
    CHECK(outside.last == 0);
}

TEST_CASE("List rows: unbalanced calls are reported, an early stop still lays out every row")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    const int errors = fi::UsageErrorCount();
    session.SetUi([] { fi::EndListRows(); });
    session.Frame();
    CHECK(fi::UsageErrorCount() == errors + 1);
    session.SetUi([] { fi::BeginListRows({.rowCount = 3, .rowHeight = 20.0f}); });
    session.Frame();
    CHECK(fi::UsageErrorCount() == errors + 2);
    float height = 0.0f;
    session.SetUi([&] {
        const float top = ImGui::GetCursorScreenPos().y;
        fi::BeginListRows({.rowCount = 1000, .rowHeight = 20.0f});
        fi::EndListRows();  // no loop at all
        height = ImGui::GetCursorScreenPos().y - top;
    });
    session.Frame();
    CHECK(height == doctest::Approx(20000.0f));
}

TEST_CASE("List view: a million rows hand out as many rows as fifty, none twice")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    ListUi ui;
    ui.track = false;
    ui.desc.entry = &GeneratedEntry;
    ui.desc.rowCount = 50;
    session.SetUi([&] { ui.Ui(session); });
    session.Frames(2);
    const size_t fifty = ui.rows.size();
    CHECK(fifty > 0);
    CHECK(fifty < 15);
    ui.desc.rowCount = 1000000;
    session.Frames(2);
    CHECK(ui.rows.size() == fifty);
    CHECK(ui.Unique());
    // At the very end of the list the rows are still one row height apart.
    ui.track = true;
    ui.desc.revealRow = 999999;
    session.Frames(3);
    ui.desc.revealRow = -1;
    REQUIRE(ui.Submitted(999999));
    REQUIRE(ui.Submitted(999998));
    const float step = Row(session, 999999).y - Row(session, 999998).y;
    CHECK(step == doctest::Approx(fi::ListRowHeight(facet::ControlSize::Regular)));
    CHECK(ui.rows.size() <= fifty + 1);
}

TEST_CASE("List view: the row height follows the control size")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    ListUi ui;
    Items items = Fruits();
    ui.desc.rowCount = static_cast<int>(items.labels.size());
    ui.desc.entry = &ItemsEntry;
    ui.desc.user = &items;
    session.SetUi([&] { ui.Ui(session); });
    using facet::ControlSize;
    for (const ControlSize size : {ControlSize::Small, ControlSize::Regular, ControlSize::Large})
    {
        ui.desc.size = size;
        session.Frames(2);
        CHECK(Row(session, 1).y - Row(session, 0).y == doctest::Approx(fi::ListRowHeight(size)));
    }
    const facet::ListTokens& list = facet::T().list;
    CHECK(fi::ListRowHeight(facet::ControlSize::Small) == doctest::Approx(facet::Px(list.rowHeightSmall)));
    CHECK(fi::ListRowHeight(facet::ControlSize::Large) == doctest::Approx(facet::Px(list.rowHeightLarge)));
    facet::PushControlSize(facet::ControlSize::Small);
    CHECK(fi::ListRowHeight(facet::ControlSize::Inherit) == doctest::Approx(facet::Px(list.rowHeightSmall)));
    facet::PopControlSize();
}

TEST_CASE("List view: every selection gesture matches a stock multi-select list")
{
    MotionGuard guard;
    facet::test::Session session({.displaySize = ImVec2(900.0f, 600.0f)});
    REQUIRE(session.Ready());
    constexpr int kCount = 12;
    ImGuiSelectionBasicStorage stock;
    ImGuiSelectionBasicStorage mine;
    ListUi ui;
    Items items;
    for (int i = 0; i < kCount; ++i)
        items.labels.push_back("Entry " + std::to_string(i));
    ui.desc.rowCount = kCount;
    ui.desc.selection = &mine;
    ui.desc.entry = &ItemsEntry;
    ui.desc.user = &items;
    ui.desc.areaSize = ImVec2(300.0f, 500.0f);
    session.SetUi([&] {
        ImGui::BeginChild("stock", ImVec2(300.0f, 500.0f));
        ImGuiMultiSelectIO* io = ImGui::BeginMultiSelect(
            ImGuiMultiSelectFlags_ClearOnEscape | ImGuiMultiSelectFlags_ClearOnClickVoid, stock.Size, kCount);
        stock.ApplyRequests(io);
        char name[32];
        for (int i = 0; i < kCount; ++i)
        {
            ImGui::SetNextItemSelectionUserData(i);
            ImGui::Selectable(items.labels[static_cast<size_t>(i)].c_str(), stock.Contains(static_cast<ImGuiID>(i)), 0,
                              ImVec2(0.0f, 24.0f));
            std::snprintf(name, sizeof(name), "s%d", i);
            session.Track(name);
        }
        io = ImGui::EndMultiSelect();
        stock.ApplyRequests(io);
        ImGui::EndChild();
        ImGui::SameLine();
        ui.Ui(session);
    });
    session.Frames(2);

    struct Step
    {
        enum Kind { Click, Key } kind;
        int row = 0;
        ImGuiKeyChord chord = 0;
    };
    const Step steps[] = {
        {Step::Click, 2, 0},
        {Step::Click, 5, ImGuiMod_Ctrl},
        {Step::Click, 8, ImGuiMod_Shift},
        {Step::Key, 0, ImGuiKey_DownArrow | ImGuiMod_Shift},
        {Step::Key, 0, ImGuiKey_DownArrow | ImGuiMod_Shift},
        {Step::Click, 3, ImGuiMod_Ctrl},
        {Step::Key, 0, ImGuiKey_UpArrow},
        {Step::Key, 0, ImGuiKey_DownArrow},
        {Step::Key, 0, ImGuiKey_A | ImGuiMod_Ctrl},
        {Step::Key, 0, ImGuiKey_Escape},
        {Step::Click, 6, 0},
        {Step::Key, 0, ImGuiKey_Space | ImGuiMod_Ctrl},
        {Step::Click, 9, ImGuiMod_Ctrl | ImGuiMod_Shift},
        {Step::Click, 9, 0},
    };
    std::vector<std::set<ImGuiID>> stockResults;
    for (const Step& step : steps)
    {
        char name[32];
        std::snprintf(name, sizeof(name), "s%d", step.row);
        if (step.kind == Step::Click)
            ClickWith(session, session.ItemCenter(name), step.chord);
        else
            session.KeyPress(step.chord);
        stockResults.push_back(Selected(stock));
    }
    size_t index = 0;
    for (const Step& step : steps)
    {
        if (step.kind == Step::Click)
            ClickWith(session, Row(session, step.row), step.chord);
        else
            session.KeyPress(step.chord);
        const std::set<ImGuiID> result = Selected(mine);
        CAPTURE(index);
        CHECK(result == stockResults[index]);
        ++index;
    }
}

TEST_CASE("List view: the selection follows stable ids through sorting and filtering")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::vector<ImGuiID> view;
    for (ImGuiID id = 100; id < 110; ++id)
        view.push_back(id);
    ImGuiSelectionBasicStorage selection;
    selection.UserData = &view;
    selection.AdapterIndexToStorageId = [](ImGuiSelectionBasicStorage* storage, int index) {
        return (*static_cast<std::vector<ImGuiID>*>(storage->UserData))[static_cast<size_t>(index)];
    };
    ListUi ui;
    ui.desc.selection = &selection;
    session.SetUi([&] {
        ui.desc.rowCount = static_cast<int>(view.size());
        ui.Ui(session);
    });
    session.Frames(2);
    session.Click(Row(session, 2));
    REQUIRE(selection.Contains(102));
    const ImGuiID focused = GImGui->NavId;
    // Reverse the order: id 102 moves to index 7, still selected and still focused.
    std::reverse(view.begin(), view.end());
    session.Frames(2);
    CHECK(fi::IsListEntrySelected(&selection, 7));
    CHECK_FALSE(fi::IsListEntrySelected(&selection, 2));
    CHECK(RowId(session, 7) == focused);
    // Filter to the even ids: 102 is at index 3 of {108, 106, 104, 102, 100}.
    std::vector<ImGuiID> even;
    for (const ImGuiID id : view)
        if (id % 2 == 0)
            even.push_back(id);
    view = even;
    session.Frames(2);
    CHECK(fi::IsListEntrySelected(&selection, 3));
    // A Shift range in the filtered view selects only what it shows.
    ClickWith(session, Row(session, 0), ImGuiMod_Shift);
    CHECK(Selected(selection) == std::set<ImGuiID>{102, 104, 106, 108});
}

TEST_CASE("List view: select all on a million rows leaves disabled rows out")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    ImGuiSelectionBasicStorage selection;
    ListUi ui;
    ui.desc.rowCount = 1000000;
    ui.desc.selection = &selection;
    ui.desc.entry = &GeneratedEntry;
    g_generatedDisabled = true;
    session.SetUi([&] { ui.Ui(session); });
    session.Frames(2);
    session.Click(Row(session, 0));
    session.KeyPress(ImGuiKey_A | ImGuiMod_Ctrl);
    CHECK(selection.Size == 999000);
    CHECK(fi::IsListEntrySelected(&selection, 998));
    CHECK_FALSE(fi::IsListEntrySelected(&selection, 999));
    // A Shift range across a disabled row skips it too.
    session.KeyPress(ImGuiKey_Escape);
    CHECK(selection.Size == 0);
    ui.desc.revealRow = 1003;
    session.Frames(3);
    ui.desc.revealRow = -1;
    session.Click(Row(session, 997));
    ClickWith(session, Row(session, 1003), ImGuiMod_Shift);
    CHECK(selection.Size == 6);
    CHECK_FALSE(fi::IsListEntrySelected(&selection, 999));
    g_generatedDisabled = false;
}

TEST_CASE("List view: arrows, Page Up and Down, Home and End move the choice; disabled rows are skipped")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    int choice = -1;
    Items items;
    for (int i = 0; i < 1000; ++i)
        items.labels.push_back("Entry " + std::to_string(i));
    items.disabled.assign(1000, false);
    items.disabled[1] = true;
    ListUi ui;
    ui.desc.rowCount = 1000;
    ui.desc.choice = &choice;
    ui.desc.entry = &ItemsEntry;
    ui.desc.user = &items;
    session.SetUi([&] { ui.Ui(session); });
    session.Frames(2);
    session.Click(Row(session, 0));
    CHECK(choice == 0);
    session.KeyPress(ImGuiKey_DownArrow);
    CHECK(choice == 2);
    session.KeyPress(ImGuiKey_End);
    session.Frames(2);
    CHECK(choice == 999);
    CHECK(session.ItemVisible("row999"));
    session.KeyPress(ImGuiKey_Home);
    session.Frames(2);
    CHECK(choice == 0);
    CHECK(session.ItemVisible("row0"));
    session.KeyPress(ImGuiKey_PageDown);
    session.Frames(2);
    CHECK(choice > 2);
    CHECK(choice < 20);
    const int afterPage = choice;
    char name[32];
    std::snprintf(name, sizeof(name), "row%d", afterPage);
    CHECK(session.ItemVisible(name));
    session.KeyPress(ImGuiKey_PageUp);
    session.Frames(2);
    CHECK(choice < afterPage);
    // A click on a disabled row changes nothing.
    session.KeyPress(ImGuiKey_Home);
    session.Frames(2);
    session.Click(Row(session, 1));
    CHECK(choice == 0);
}

TEST_CASE("List view: the focused row stays alive when scrolled out, and it is the Tab stop")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    int choice = -1;
    ListUi ui;
    ui.desc.rowCount = 1000;
    ui.desc.choice = &choice;
    ui.desc.entry = &GeneratedEntry;
    session.SetUi([&] { ui.Ui(session); });
    session.Frames(2);
    session.Click(Row(session, 3));
    const ImGuiID focused = GImGui->NavId;
    REQUIRE(focused == RowId(session, 3));
    const ImVec2 center = ui.Window()->Rect().GetCenter();
    for (int i = 0; i < 4; ++i)
        session.Wheel(center, -5.0f);
    session.Seconds(1.0f);
    CHECK_FALSE(session.ItemVisible("row3"));
    CHECK(ui.Submitted(3));
    CHECK(ui.Unique());
    CHECK(GImGui->NavId == focused);
    // The keyboard continues from the focused row, which scrolls back into view.
    session.KeyPress(ImGuiKey_DownArrow);
    session.Frames(2);
    CHECK(choice == 4);
    CHECK(session.ItemVisible("row4"));
}

TEST_CASE("List view: typeahead jumps to the next matching label and restarts after a pause")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    int choice = -1;
    Items items = Fruits();
    items.disabled.assign(items.labels.size(), false);
    items.disabled[6] = true;  // Elderberry
    ListUi ui;
    ui.desc.rowCount = static_cast<int>(items.labels.size());
    ui.desc.choice = &choice;
    ui.desc.entry = &ItemsEntry;
    ui.desc.user = &items;
    session.SetUi([&] { ui.Ui(session); });
    session.Frames(2);
    session.Click(Row(session, 0));
    REQUIRE(choice == 0);
    session.Type("b");
    session.Frame();
    CHECK(choice == 2);
    CHECK(GImGui->NavId == RowId(session, 2));
    session.Type("l");
    session.Frame();
    CHECK(choice == 3);  // "bl": Blueberry
    session.Seconds(facet::T().list.typeaheadTimeout + 0.1f);
    session.Type("c");
    session.Frame();
    CHECK(choice == 4);
    // The same letter again cycles: "a" from Cherry wraps to Apple, then apricot.
    session.Seconds(facet::T().list.typeaheadTimeout + 0.1f);
    session.Type("a");
    session.Frame();
    CHECK(choice == 0);
    session.Type("a");
    session.Frame();
    CHECK(choice == 1);
    // Disabled entries never match.
    session.Seconds(facet::T().list.typeaheadTimeout + 0.1f);
    session.Type("e");
    session.Frame();
    CHECK(choice == 1);
}

TEST_CASE("List view: the typeahead search")
{
    Items items = Fruits();
    items.labels.push_back("Hidden##key");
    items.disabled.assign(items.labels.size(), false);
    items.disabled[5] = true;  // Date
    void* user = &items;
    CHECK(fi::ListTypeaheadFind("b", 9, -1, &ItemsEntry, user) == 2);
    CHECK(fi::ListTypeaheadFind("b", 9, 2, &ItemsEntry, user) == 3);
    CHECK(fi::ListTypeaheadFind("b", 9, 3, &ItemsEntry, user) == 2);  // wraps
    CHECK(fi::ListTypeaheadFind("bb", 9, 2, &ItemsEntry, user) == 3);  // repeated: cycles
    CHECK(fi::ListTypeaheadFind("ba", 9, 2, &ItemsEntry, user) == 2);  // longer text: from the current entry
    CHECK(fi::ListTypeaheadFind("APR", 9, 0, &ItemsEntry, user) == 1);  // case-insensitive
    CHECK(fi::ListTypeaheadFind("d", 9, 0, &ItemsEntry, user) == -1);  // Date is disabled
    CHECK(fi::ListTypeaheadFind("hidden", 9, 0, &ItemsEntry, user) == 8);
    CHECK(fi::ListTypeaheadFind("hiddenk", 9, 0, &ItemsEntry, user) == -1);  // nothing after "##"
    CHECK(fi::ListTypeaheadFind("", 9, 0, &ItemsEntry, user) == -1);
    CHECK(fi::ListTypeaheadFind("a", 0, 0, &ItemsEntry, user) == -1);
}

TEST_CASE("List view: Enter and a double click activate a row")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    ImGuiSelectionBasicStorage selection;
    ListUi ui;
    Items items = Fruits();
    ui.desc.rowCount = static_cast<int>(items.labels.size());
    ui.desc.selection = &selection;
    ui.desc.entry = &ItemsEntry;
    ui.desc.user = &items;
    int activated = -1;
    session.SetUi([&] {
        ui.Ui(session);
        if (ui.result.activated >= 0)
            activated = ui.result.activated;
    });
    session.Frames(2);
    session.DoubleClick(Row(session, 3));
    CHECK(activated == 3);
    activated = -1;
    session.KeyPress(ImGuiKey_DownArrow);
    session.KeyPress(ImGuiKey_Enter);
    CHECK(activated == 4);
    CHECK(selection.Contains(4));
}

TEST_CASE("List view: loading, error and empty states keep the area and return false")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    ListUi ui;
    ui.desc.rowCount = 5;
    ui.desc.entry = &GeneratedEntry;
    bool open = false;
    ImVec2 size;
    session.SetUi([&] {
        const float top = ImGui::GetCursorScreenPos().y;
        ui.desc.areaSize = ImVec2(300.0f, 240.0f);
        open = fi::BeginListView(ImGui::GetID("list"), ui.desc);
        if (open)
            fi::EndListView();
        size = ImVec2(ImGui::GetItemRectSize().x, ImGui::GetCursorScreenPos().y - top);
    });
    session.Frame();
    CHECK(open);
    const ImVec2 ready = size;
    for (const facet::DataState state : {facet::DataState::Loading, facet::DataState::Error})
    {
        ui.desc.state = state;
        session.Frame();
        CHECK_FALSE(open);
        CHECK(size.y == doctest::Approx(ready.y));
    }
    ui.desc.state = facet::DataState::Ready;
    ui.desc.rowCount = 0;
    session.Frame();
    CHECK_FALSE(open);
    CHECK(size.y == doctest::Approx(ready.y));
}

TEST_CASE("List view: a visible list allocates nothing per frame in the steady state")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    ImGuiSelectionBasicStorage selection;
    fi::ListViewDesc desc;
    desc.rowCount = 1000000;
    desc.selection = &selection;
    desc.entry = &GeneratedEntry;
    desc.areaSize = ImVec2(300.0f, 240.0f);
    session.SetUi([&] {
        if (fi::BeginListView(ImGui::GetID("list"), desc))
        {
            facet::RowRange range;
            while (facet::NextVisibleRows(&range))
                for (int i = range.first; i < range.last; ++i)
                {
                    const fi::ListRowState row = fi::ListViewRow(i);
                    ImGui::TextUnformatted("Row");
                    (void)row;
                }
            fi::EndListView();
        }
    });
    session.Frames(10);
    session.MouseMove(ImVec2(100.0f, 60.0f));  // hover a row
    session.Frames(30);

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
#ifdef _DEBUG
    static long crtAllocations = 0;
    crtAllocations = 0;
    const _CRT_ALLOC_HOOK previous =
        _CrtSetAllocHook([](int type, void*, size_t, int, long, const unsigned char*, int) -> int {
            if (type == _HOOK_ALLOC || type == _HOOK_REALLOC)
                ++crtAllocations;
            return 1;
        });
#endif
    session.Frames(20);
#ifdef _DEBUG
    _CrtSetAllocHook(previous);
    CHECK(crtAllocations == 0);
#endif
    ImGui::SetAllocatorFunctions(oldAlloc, oldFree, oldUser);
    CHECK(allocations == 0);
}

TEST_CASE("List view: unbalanced calls are reported")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    const int errors = fi::UsageErrorCount();
    session.SetUi([] { (void)fi::EndListView(); });
    session.Frame();
    CHECK(fi::UsageErrorCount() == errors + 1);
    session.SetUi([] { (void)fi::ListViewRow(0); });
    session.Frame();
    CHECK(fi::UsageErrorCount() == errors + 2);
}

namespace
{

// Root A (open): Leaf A1, Node A2 (Leaf A2a, Leaf A2b), Leaf A3; Root B (lazy): Leaf B1.
struct TreeUi
{
    ImGuiSelectionBasicStorage selection;
    fi::ListTreeResult result;
    bool loaded = false;
    bool loading = false;
    bool toggledB = false;
    bool itemToggledB = false;

    void Node(facet::test::Session& session, const char* label, bool leaf, bool defaultOpen = false)
    {
        const fi::ListTreeNodeState node = fi::ListTreeNode(label, {.leaf = leaf, .defaultOpen = defaultOpen});
        session.Track(label);
        (void)node;
    }

    void Ui(facet::test::Session& session)
    {
        if (!fi::BeginListTree(ImGui::GetID("tree"), {.selection = &selection, .areaSize = ImVec2(300.0f, 400.0f)}))
            return;
        const fi::ListTreeNodeState a = fi::ListTreeNode("Root A", {.defaultOpen = true});
        session.Track("Root A");
        if (a.open)
        {
            Node(session, "Leaf A1", true);
            const fi::ListTreeNodeState a2 = fi::ListTreeNode("Node A2", {.defaultOpen = true});
            session.Track("Node A2");
            if (a2.open)
            {
                Node(session, "Leaf A2a", true);
                Node(session, "Leaf A2b", true);
                fi::ListTreePop();
            }
            Node(session, "Leaf A3", true);
            fi::ListTreePop();
        }
        const fi::ListTreeNodeState b = fi::ListTreeNode("Root B", {.loading = loading});
        session.Track("Root B");
        if (b.toggled)
        {
            toggledB = true;
            itemToggledB = ImGui::IsItemToggledOpen();
            if (b.open && !loaded)
                loading = true;
        }
        if (b.open)
        {
            if (loaded)
                Node(session, "Leaf B1", true);
            fi::ListTreePop();
        }
        result = fi::EndListTree();
    }
};

ImGuiID TreeKey(const facet::test::Session& session, const char* label)
{
    return session.ItemId(label);
}

} // namespace

TEST_CASE("Tree: Right opens and enters, Left goes to the parent and closes")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    TreeUi ui;
    ui.loaded = true;
    session.SetUi([&] { ui.Ui(session); });
    session.Frames(2);
    session.Click(session.ItemCenter("Root B"));
    CHECK(ui.selection.Contains(TreeKey(session, "Root B")));
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(ui.toggledB);
    CHECK(ui.itemToggledB);
    REQUIRE(session.ItemId("Leaf B1") != 0);
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(GImGui->NavId == session.ItemId("Leaf B1"));
    CHECK(Selected(ui.selection) == std::set<ImGuiID>{TreeKey(session, "Leaf B1")});
    session.KeyPress(ImGuiKey_LeftArrow);
    CHECK(GImGui->NavId == session.ItemId("Root B"));
    CHECK(Selected(ui.selection) == std::set<ImGuiID>{TreeKey(session, "Root B")});
    ui.toggledB = false;
    session.KeyPress(ImGuiKey_LeftArrow);
    CHECK(ui.toggledB);
    // Down and Up move through the visible nodes.
    session.KeyPress(ImGuiKey_UpArrow);
    CHECK(GImGui->NavId == session.ItemId("Leaf A3"));
}

TEST_CASE("Tree: lazy children show a loading row until they arrive")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    TreeUi ui;
    session.SetUi([&] { ui.Ui(session); });
    session.Frames(2);
    session.DoubleClick(session.ItemCenter("Root B"));
    CHECK(ui.toggledB);
    CHECK(ui.loading);
    session.Frames(2);
    // The children arrive.
    ui.loaded = true;
    ui.loading = false;
    session.Frames(2);
    CHECK(session.ItemId("Leaf B1") != 0);
    CHECK(session.ItemVisible("Leaf B1"));
}

TEST_CASE("Tree: selected nodes under a closed parent stay selected until the selection is cleared")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    TreeUi ui;
    session.SetUi([&] { ui.Ui(session); });
    session.Frames(2);
    const ImGuiID a2a = TreeKey(session, "Leaf A2a");
    session.Click(session.ItemCenter("Leaf A2a"));
    CHECK(ui.selection.Contains(a2a));
    // Close Node A2 with its expander: the hidden child stays selected.
    const ImRect a2 = session.ItemRect("Node A2");
    const float expanderX = a2.Min.x + facet::Px(facet::T().spacing.xs) + facet::Px(facet::T().list.indent) * 1.5f;
    session.Click(ImVec2(expanderX, a2.GetCenter().y));
    session.Frames(2);
    CHECK(ui.selection.Contains(a2a));
    CHECK(ui.selection.Size == 1);
    // Ctrl+A selects the visible nodes; the hidden one stays as it was.
    session.Click(session.ItemCenter("Leaf A1"));
    CHECK_FALSE(ui.selection.Contains(a2a));  // a plain click clears the whole storage
    session.KeyPress(ImGuiKey_A | ImGuiMod_Ctrl);
    CHECK(ui.selection.Size == 5);  // Root A, Leaf A1, Node A2, Leaf A3, Root B
    CHECK_FALSE(ui.selection.Contains(a2a));
    session.KeyPress(ImGuiKey_Escape);
    CHECK(ui.selection.Size == 0);
    // Shift ranges run over the visible nodes only.
    session.Click(session.ItemCenter("Leaf A1"));
    ClickWith(session, session.ItemCenter("Root B"), ImGuiMod_Shift);
    CHECK(ui.selection.Size == 4);
    CHECK_FALSE(ui.selection.Contains(a2a));
}

TEST_CASE("Tree: typing jumps to the next node whose label starts with the text")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    TreeUi ui;
    session.SetUi([&] { ui.Ui(session); });
    session.Frames(2);
    session.Click(session.ItemCenter("Root A"));
    session.Type("l");
    session.Frames(2);
    CHECK(GImGui->NavId == session.ItemId("Leaf A1"));
    session.Type("l");
    session.Frames(2);
    CHECK(GImGui->NavId == session.ItemId("Leaf A2a"));
    session.Seconds(facet::T().list.typeaheadTimeout + 0.1f);
    session.Type("root b");
    session.Frames(2);
    CHECK(GImGui->NavId == session.ItemId("Root B"));
    CHECK(ui.selection.Contains(TreeKey(session, "Root B")));
}

TEST_CASE("List view: the arrows continue from the row typeahead moved to")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    int choice = -1;
    Items items;
    for (int i = 0; i < 60; ++i)
        items.labels.push_back((i < 50 ? "a" : "z") + std::to_string(i));
    ListUi ui;
    ui.desc.rowCount = 60;
    ui.desc.choice = &choice;
    ui.desc.pill = true;
    ui.desc.entry = &ItemsEntry;
    ui.desc.user = &items;
    session.SetUi([&] {
        ImGui::BeginChild("page", ImVec2(0.0f, 0.0f));
        ui.Ui(session);
        ImGui::Dummy(ImVec2(10.0f, 2000.0f));
        ImGui::EndChild();
    });
    session.Frames(2);
    session.Click(Row(session, 0));
    session.Type("z");
    session.Frames(2);
    CHECK(choice == 50);
    session.KeyPress(ImGuiKey_UpArrow);
    CHECK(choice == 49);
    session.KeyPress(ImGuiKey_DownArrow);
    CHECK(choice == 50);
}

TEST_CASE("Tree: Left on a closed child node goes to its parent inside a scrolling page")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    TreeUi ui;
    session.SetUi([&] {
        ImGui::BeginChild("page", ImVec2(0.0f, 0.0f));
        ui.Ui(session);
        ImGui::Dummy(ImVec2(10.0f, 2000.0f));
        ImGui::EndChild();
    });
    session.Frames(2);
    session.Click(session.ItemCenter("Root A"));
    session.KeyPress(ImGuiKey_DownArrow);
    session.KeyPress(ImGuiKey_DownArrow);
    REQUIRE(GImGui->NavId == session.ItemId("Node A2"));
    session.KeyPress(ImGuiKey_LeftArrow);  // closes it
    session.KeyPress(ImGuiKey_LeftArrow);  // goes to the parent
    CHECK(GImGui->NavId == session.ItemId("Root A"));
    session.KeyPress(ImGuiKey_RightArrow);  // Root A is open: to its first child
    CHECK(GImGui->NavId == session.ItemId("Leaf A1"));
}

TEST_CASE("List selection: a grid wraps the arrows at the end of a line")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    ImGuiSelectionBasicStorage selection;
    session.SetUi([&] {
        if (!fi::BeginScrollAreaCore("grid", {.size = ImVec2(400.0f, 300.0f)}))
            return;
        fi::BeginListSelection(&selection, 9, {.boxSelect = true, .grid = true});
        fi::BeginListRows({.rowCount = 9, .rowHeight = 40.0f, .columns = 3});
        facet::RowRange range;
        char name[16];
        while (fi::NextListRows(&range))
        {
            for (int i = range.first; i < range.last; ++i)
            {
                if (i % 3 != 0)
                    ImGui::SameLine();
                ImGui::SetNextItemSelectionUserData(i);
                std::snprintf(name, sizeof(name), "tile%d", i);
                ImGui::Selectable(name, fi::IsListEntrySelected(&selection, i), 0,
                                  ImVec2(80.0f, 40.0f - ImGui::GetStyle().ItemSpacing.y));
                session.Track(name);
            }
        }
        fi::EndListRows();
        fi::EndListSelection();
        fi::EndScrollAreaCore();
    });
    session.Frames(2);
    session.Click(session.ItemCenter("tile2"));
    REQUIRE(selection.Contains(2));
    session.KeyPress(ImGuiKey_RightArrow);
    session.Frames(2);
    CHECK(selection.Size == 1);
    CHECK(selection.Contains(3));
}
