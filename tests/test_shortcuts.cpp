// The shortcut registry (core/input.h): registration, routing by scope through Dear ImGui's
// shortcut routing, text fields, triggers, callbacks and conflicts, with scripted key events.

#include <string>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/core/input.h>

#include "support/session.h"

namespace
{

// Every case starts from an empty registry and leaves one behind (the registry lives for the
// whole process).
struct RegistryGuard
{
    RegistryGuard() { Clear(); }
    ~RegistryGuard() { Clear(); }
    static void Clear()
    {
        facet::ShortcutInfo info;
        while (facet::GetShortcut(0, &info))
            facet::UnregisterShortcut(info.id);
    }
};

} // namespace

TEST_CASE("Shortcuts register under unique names, keep their id when replaced and list in order")
{
    RegistryGuard guard;
    const facet::ShortcutId save = facet::RegisterShortcut("file.save", ImGuiMod_Ctrl | ImGuiKey_S,
                                                           {.label = "Save", .group = "File"});
    const facet::ShortcutId open = facet::RegisterShortcut("file.open", ImGuiMod_Ctrl | ImGuiKey_O);
    REQUIRE(save.IsValid());
    REQUIRE(open.IsValid());
    CHECK_FALSE(save == open);
    CHECK_FALSE(facet::RegisterShortcut(nullptr, ImGuiKey_A).IsValid());
    CHECK_FALSE(facet::RegisterShortcut("", ImGuiKey_A).IsValid());
    CHECK(facet::FindShortcut("file.save") == save);
    CHECK_FALSE(facet::FindShortcut("missing").IsValid());
    CHECK(facet::ShortcutCount() == 2);

    facet::ShortcutInfo info;
    REQUIRE(facet::GetShortcut(0, &info));
    CHECK(info.id == save);
    CHECK(std::string(info.name) == "file.save");
    CHECK(std::string(info.label) == "Save");
    CHECK(std::string(info.group) == "File");
    CHECK(info.primary == (ImGuiMod_Ctrl | ImGuiKey_S));
    REQUIRE(facet::GetShortcut(open, &info));
    CHECK(std::string(info.label) == "file.open");  // no label: the name
    CHECK(std::string(info.group).empty());
    CHECK_FALSE(facet::GetShortcut(2, &info));
    CHECK_FALSE(facet::GetShortcut(-1, &info));

    // Registering the name again replaces the entry in place.
    const facet::ShortcutId again = facet::RegisterShortcut("file.save", ImGuiKey_F2, {.label = "Save now"});
    CHECK(again == save);
    CHECK(facet::ShortcutCount() == 2);
    REQUIRE(facet::GetShortcut(0, &info));
    CHECK(info.primary == ImGuiKey_F2);
    CHECK(std::string(info.label) == "Save now");

    // A string handed out stays put while other entries come and go.
    const char* name = info.name;
    for (int i = 0; i < 40; ++i)
        facet::RegisterShortcut(("filler." + std::to_string(i)).c_str(), ImGuiKey_None);
    facet::UnregisterShortcut(open);
    CHECK(std::string(name) == "file.save");

    facet::UnregisterShortcut(save);
    CHECK_FALSE(facet::GetShortcut(save, &info));
    facet::UnregisterShortcut(save);  // unknown ids are ignored
}

TEST_CASE("A global shortcut fires once per press, with exactly its modifiers")
{
    RegistryGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    const facet::ShortcutId id = facet::RegisterShortcut("demo.count", ImGuiMod_Ctrl | ImGuiKey_K);
    const facet::ShortcutId repeating =
        facet::RegisterShortcut("demo.repeat", ImGuiKey_PageDown, {.repeat = true});
    int count = 0, repeats = 0;
    session.SetUi([&] {
        count += facet::ShortcutPressed(id) ? 1 : 0;
        repeats += facet::ShortcutPressed(repeating) ? 1 : 0;
    });
    session.Frame();
    session.KeyPress(ImGuiMod_Ctrl | ImGuiKey_K);
    CHECK(count == 1);
    session.KeyPress(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_K);
    CHECK(count == 1);
    session.KeyPress(ImGuiKey_K);
    CHECK(count == 1);
    session.HoldKey(ImGuiMod_Ctrl | ImGuiKey_K, 1.0f);
    CHECK(count == 2);  // no repeat unless asked for
    session.HoldKey(ImGuiKey_PageDown, 1.0f);
    CHECK(repeats > 3);
    // Outside a frame nothing fires.
    CHECK_FALSE(facet::ShortcutPressed(id));
}

TEST_CASE("Disabled, rebound and reset shortcuts")
{
    RegistryGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    const facet::ShortcutId id =
        facet::RegisterShortcut("view.zoom", ImGuiMod_Ctrl | ImGuiKey_Equal, {.alternate = ImGuiKey_KeypadAdd});
    int count = 0;
    session.SetUi([&] { count += facet::ShortcutPressed(id) ? 1 : 0; });
    session.Frame();
    session.KeyPress(ImGuiKey_KeypadAdd);
    CHECK(count == 1);

    facet::SetShortcutEnabled(id, false);
    session.KeyPress(ImGuiMod_Ctrl | ImGuiKey_Equal);
    CHECK(count == 1);
    facet::ShortcutInfo info;
    REQUIRE(facet::GetShortcut(id, &info));
    CHECK_FALSE(info.enabled);
    facet::SetShortcutEnabled(id, true);
    session.Frame();
    session.KeyPress(ImGuiMod_Ctrl | ImGuiKey_Equal);
    CHECK(count == 2);

    facet::SetShortcutChords(id, ImGuiKey_F5, ImGuiKey_None);
    session.Frame();
    session.KeyPress(ImGuiMod_Ctrl | ImGuiKey_Equal);
    session.KeyPress(ImGuiKey_KeypadAdd);
    CHECK(count == 2);
    session.KeyPress(ImGuiKey_F5);
    CHECK(count == 3);
    REQUIRE(facet::GetShortcut(id, &info));
    CHECK(info.primary == ImGuiKey_F5);
    CHECK(info.defaultPrimary == (ImGuiMod_Ctrl | ImGuiKey_Equal));

    facet::ResetShortcut(id);
    session.Frame();
    session.KeyPress(ImGuiKey_F5);
    CHECK(count == 3);
    session.KeyPress(ImGuiKey_KeypadAdd);
    CHECK(count == 4);
}

TEST_CASE("TriggerShortcut fires in the next frame; callbacks fire once")
{
    RegistryGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    int calls = 0, polled = 0, unpolledCalls = 0;
    const auto onFire = [](facet::ShortcutId, void* user) { ++*static_cast<int*>(user); };
    const facet::ShortcutId asked =
        facet::RegisterShortcut("cmd.asked", ImGuiMod_Ctrl | ImGuiKey_J, {.callback = onFire, .user = &calls});
    // A Global shortcut that the app never asks for still fires its callback.
    facet::RegisterShortcut("cmd.unasked", ImGuiMod_Ctrl | ImGuiKey_U, {.callback = onFire, .user = &unpolledCalls});
    session.SetUi([&] {
        polled += facet::ShortcutPressed(asked) ? 1 : 0;
        polled += facet::ShortcutPressed(asked) ? 1 : 0;  // asked twice: same answer, one callback
    });
    session.Frame();

    facet::TriggerShortcut(asked);
    CHECK(polled == 0);
    session.Frame();
    CHECK(polled == 2);
    CHECK(calls == 1);
    session.Frame();
    CHECK(polled == 2);

    session.KeyPress(ImGuiMod_Ctrl | ImGuiKey_J);
    CHECK(calls == 2);
    session.Frame();
    session.KeyPress(ImGuiMod_Ctrl | ImGuiKey_U);
    CHECK(unpolledCalls == 1);

    // A disabled shortcut is not triggered.
    facet::SetShortcutEnabled(asked, false);
    facet::TriggerShortcut(asked);
    session.Frames(2);
    CHECK(calls == 2);
}

namespace
{

// Two side-by-side windows: "left" asks for a Window shortcut, "right" for nothing.
void TwoWindows(const facet::ShortcutId windowShortcut, int* windowCount, facet::test::Session& session)
{
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(300.0f, 300.0f));
    ImGui::Begin("left", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    ImGui::Button("left button");
    session.Track("left");
    *windowCount += facet::ShortcutPressed(windowShortcut) ? 1 : 0;
    ImGui::End();
    ImGui::SetNextWindowPos(ImVec2(400.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(300.0f, 300.0f));
    ImGui::Begin("right", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    ImGui::Button("right button");
    session.Track("right");
    ImGui::End();
}

} // namespace

TEST_CASE("A window shortcut fires only while its window is focused, and wins over a global one")
{
    RegistryGuard guard;
    facet::test::Session session({.window = false});
    REQUIRE(session.Ready());
    const facet::ShortcutId global = facet::RegisterShortcut("app.find", ImGuiMod_Ctrl | ImGuiKey_F);
    const facet::ShortcutId local =
        facet::RegisterShortcut("panel.find", ImGuiMod_Ctrl | ImGuiKey_F, {.scope = facet::ShortcutScope::Window});
    int globalCount = 0, localCount = 0;
    session.SetUi([&] {
        globalCount += facet::ShortcutPressed(global) ? 1 : 0;
        TwoWindows(local, &localCount, session);
    });
    session.Frame();

    session.Click(session.ItemCenter("right"));
    session.KeyPress(ImGuiMod_Ctrl | ImGuiKey_F);
    CHECK(localCount == 0);
    CHECK(globalCount == 1);

    session.Click(session.ItemCenter("left"));
    session.KeyPress(ImGuiMod_Ctrl | ImGuiKey_F);
    CHECK(localCount == 1);
    CHECK(globalCount == 1);  // the innermost scope took the chord

    // Both overlap, so each one reports the other as a conflict.
    CHECK(facet::FindChordConflict(ImGuiMod_Ctrl | ImGuiKey_F, global) == local);
    CHECK(facet::FindChordConflict(ImGuiMod_Ctrl | ImGuiKey_F, local) == global);
}

TEST_CASE("A focused-item shortcut fires only while its item has keyboard focus")
{
    RegistryGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    const facet::ShortcutId item =
        facet::RegisterShortcut("row.rename", ImGuiKey_F2, {.scope = facet::ShortcutScope::FocusedItem});
    const facet::ShortcutId window =
        facet::RegisterShortcut("list.rename", ImGuiKey_F2, {.scope = facet::ShortcutScope::Window});
    int itemCount = 0, windowCount = 0;
    session.SetUi([&] {
        ImGui::Button("row");
        session.Track("row");
        itemCount += facet::ShortcutPressed(item) ? 1 : 0;
        ImGui::Button("other");
        session.Track("other");
        windowCount += facet::ShortcutPressed(window) ? 1 : 0;
    });
    session.Frame();
    session.Click(session.ItemCenter("other"));
    session.KeyPress(ImGuiKey_F2);
    CHECK(itemCount == 0);
    CHECK(windowCount == 1);

    session.Click(session.ItemCenter("row"));
    session.Frame();
    session.KeyPress(ImGuiKey_F2);
    CHECK(itemCount == 1);
    CHECK(windowCount == 1);  // the item is inside the window's scope and wins

    // Item shortcuts on different items never compete; on the same item they do.
    const facet::ShortcutId otherItem =
        facet::RegisterShortcut("other.rename", ImGuiKey_F2, {.scope = facet::ShortcutScope::FocusedItem});
    const facet::ShortcutId sameItem =
        facet::RegisterShortcut("row.edit", ImGuiKey_F2, {.scope = facet::ShortcutScope::FocusedItem});
    facet::SetShortcutEnabled(window, false);
    session.SetUi([&] {
        ImGui::Button("row");
        facet::ShortcutPressed(item);
        facet::ShortcutPressed(sameItem);
        ImGui::Button("other");
        facet::ShortcutPressed(otherItem);
    });
    session.Frame();
    CHECK(facet::FindChordConflict(ImGuiKey_F2, otherItem) == facet::ShortcutId{});
    CHECK(facet::FindChordConflict(ImGuiKey_F2, item) == sameItem);
}

TEST_CASE("A chord of two modifiers without a key is listed but never routed")
{
    RegistryGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    const facet::ShortcutId mods = facet::RegisterShortcut("odd.mods", ImGuiMod_Ctrl | ImGuiMod_Shift);
    const facet::ShortcutId plain = facet::RegisterShortcut("odd.plain", ImGuiMod_Ctrl | ImGuiKey_E);
    int modsCount = 0, plainCount = 0;
    session.SetUi([&] {
        modsCount += facet::ShortcutPressed(mods) ? 1 : 0;
        plainCount += facet::ShortcutPressed(plain) ? 1 : 0;
    });
    session.Frame();
    session.KeyPress(ImGuiMod_Ctrl | ImGuiMod_Shift);
    session.KeyPress(ImGuiMod_Ctrl | ImGuiKey_E);
    CHECK(modsCount == 0);
    CHECK(plainCount == 1);
    CHECK(facet::FindChordConflict(ImGuiMod_Ctrl | ImGuiMod_Shift) == mods);
}

TEST_CASE("A text field keeps typed characters and its own chords")
{
    RegistryGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    const facet::ShortcutId plain = facet::RegisterShortcut("tool.knife", ImGuiKey_K);
    const facet::ShortcutId selectAll = facet::RegisterShortcut("edit.select_all", ImGuiMod_Ctrl | ImGuiKey_A);
    const facet::ShortcutId palette = facet::RegisterShortcut("app.palette", ImGuiMod_Ctrl | ImGuiKey_P);
    const facet::ShortcutId function = facet::RegisterShortcut("app.refresh", ImGuiKey_F5);
    int plainCount = 0, selectAllCount = 0, paletteCount = 0, functionCount = 0;
    char text[32] = "";
    session.SetUi([&] {
        ImGui::InputText("field", text, sizeof(text));
        session.Track("field");
        plainCount += facet::ShortcutPressed(plain) ? 1 : 0;
        selectAllCount += facet::ShortcutPressed(selectAll) ? 1 : 0;
        paletteCount += facet::ShortcutPressed(palette) ? 1 : 0;
        functionCount += facet::ShortcutPressed(function) ? 1 : 0;
    });
    session.Frame();
    session.KeyPress(ImGuiKey_K);
    CHECK(plainCount == 1);

    session.Click(session.ItemCenter("field"));
    REQUIRE(ImGui::GetIO().WantTextInput);
    ImGui::GetIO().AddKeyEvent(ImGuiKey_K, true);
    session.Type("k");
    ImGui::GetIO().AddKeyEvent(ImGuiKey_K, false);
    session.Frames(2);
    CHECK(std::string(text) == "k");
    CHECK(plainCount == 1);
    session.KeyPress(ImGuiMod_Ctrl | ImGuiKey_A);
    CHECK(selectAllCount == 0);   // the field's own select-all
    session.KeyPress(ImGuiMod_Ctrl | ImGuiKey_P);
    CHECK(paletteCount == 1);     // not a character, not the field's: it fires
    session.KeyPress(ImGuiKey_F5);
    CHECK(functionCount == 1);
}

TEST_CASE("A focused-item shortcut leaves an active text field its own chords")
{
    RegistryGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    const facet::ShortcutId selectAll = facet::RegisterShortcut(
        "field.select_all", ImGuiMod_Ctrl | ImGuiKey_A, {.scope = facet::ShortcutScope::FocusedItem});
    const facet::ShortcutId rename =
        facet::RegisterShortcut("field.rename", ImGuiKey_F2, {.scope = facet::ShortcutScope::FocusedItem});
    const facet::ShortcutId windowRename =
        facet::RegisterShortcut("window.rename", ImGuiKey_F2, {.scope = facet::ShortcutScope::Window});
    int selectAllCount = 0, renameCount = 0, windowRenameCount = 0;
    char text[32] = "abc";
    session.SetUi([&] {
        // The window shortcut is asked first, so only the step-aside rule lets the item win.
        windowRenameCount += facet::ShortcutPressed(windowRename) ? 1 : 0;
        ImGui::InputText("field", text, sizeof(text));
        session.Track("field");
        selectAllCount += facet::ShortcutPressed(selectAll) ? 1 : 0;
        renameCount += facet::ShortcutPressed(rename) ? 1 : 0;
    });
    session.Frame();
    session.Click(session.ItemCenter("field"));
    session.Frame();
    REQUIRE(ImGui::GetActiveID() == session.ItemId("field"));
    REQUIRE(GImGui->NavId == session.ItemId("field"));
    ImGuiInputTextState* state = ImGui::GetInputTextState(session.ItemId("field"));
    REQUIRE(state != nullptr);
    state->ClearSelection();
    session.Frame();
    REQUIRE_FALSE(state->HasSelection());

    session.KeyPress(ImGuiMod_Ctrl | ImGuiKey_A);
    CHECK(selectAllCount == 0);  // the field's select-all wins over the item's shortcut
    CHECK(state->HasSelection());

    // A chord the field does not use goes to the item, not to its window, while editing ...
    session.KeyPress(ImGuiKey_F2);
    CHECK(renameCount == 1);
    CHECK(windowRenameCount == 0);

    // ... and while the field has keyboard focus without being edited.
    session.KeyPress(ImGuiKey_Escape);
    REQUIRE(ImGui::GetActiveID() == 0);
    REQUIRE(GImGui->NavId == session.ItemId("field"));
    session.KeyPress(ImGuiKey_F2);
    CHECK(renameCount == 2);
    CHECK(windowRenameCount == 0);
}

TEST_CASE("Shortcuts do not fire inside a disabled block")
{
    RegistryGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    const facet::ShortcutId id = facet::RegisterShortcut("demo.disabled", ImGuiKey_F6);
    int count = 0;
    session.SetUi([&] {
        ImGui::BeginDisabled();
        count += facet::ShortcutPressed(id) ? 1 : 0;
        ImGui::EndDisabled();
    });
    session.Frame();
    session.KeyPress(ImGuiKey_F6);
    CHECK(count == 0);
}

TEST_CASE("Conflicts: the same chord in overlapping scopes")
{
    RegistryGuard guard;
    const facet::ShortcutId a = facet::RegisterShortcut("a", ImGuiMod_Ctrl | ImGuiKey_D);
    const facet::ShortcutId b = facet::RegisterShortcut("b", ImGuiKey_F1, {.alternate = ImGuiMod_Ctrl | ImGuiKey_D});
    const facet::ShortcutId c = facet::RegisterShortcut("c", ImGuiKey_LeftShift);
    CHECK(facet::FindChordConflict(ImGuiMod_Ctrl | ImGuiKey_D, a) == b);
    CHECK(facet::FindChordConflict(ImGuiMod_Ctrl | ImGuiKey_D, b) == a);
    CHECK(facet::FindChordConflict(ImGuiMod_Ctrl | ImGuiKey_D) == a);
    CHECK_FALSE(facet::FindChordConflict(ImGuiKey_F1, b).IsValid());
    CHECK_FALSE(facet::FindChordConflict(ImGuiKey_None).IsValid());
    // A sided modifier key with or without its flag is the same chord; the other side is not.
    CHECK(facet::FindChordConflict(ImGuiKey_LeftShift | ImGuiMod_Shift) == c);
    CHECK_FALSE(facet::FindChordConflict(ImGuiKey_RightShift).IsValid());
    // Disabled shortcuts never conflict; rebinding removes the conflict.
    facet::SetShortcutEnabled(b, false);
    CHECK_FALSE(facet::FindChordConflict(ImGuiMod_Ctrl | ImGuiKey_D, a).IsValid());
    facet::SetShortcutEnabled(b, true);
    facet::SetShortcutChords(b, ImGuiKey_F1, ImGuiKey_None);
    CHECK_FALSE(facet::FindChordConflict(ImGuiMod_Ctrl | ImGuiKey_D, a).IsValid());
}

TEST_CASE("Window shortcuts in separate windows do not conflict; nested ones do")
{
    RegistryGuard guard;
    facet::test::Session session({.window = false});
    REQUIRE(session.Ready());
    const facet::ShortcutId left =
        facet::RegisterShortcut("left.copy", ImGuiMod_Ctrl | ImGuiKey_C, {.scope = facet::ShortcutScope::Window});
    const facet::ShortcutId right =
        facet::RegisterShortcut("right.copy", ImGuiMod_Ctrl | ImGuiKey_C, {.scope = facet::ShortcutScope::Window});
    const facet::ShortcutId child =
        facet::RegisterShortcut("child.copy", ImGuiMod_Ctrl | ImGuiKey_C, {.scope = facet::ShortcutScope::Window});
    // Not asked for yet: the window is unknown, so they may overlap.
    CHECK(facet::FindChordConflict(ImGuiMod_Ctrl | ImGuiKey_C, left).IsValid());
    session.SetUi([&] {
        ImGui::Begin("left");
        facet::ShortcutPressed(left);
        ImGui::BeginChild("inner", ImVec2(100.0f, 100.0f));
        facet::ShortcutPressed(child);
        ImGui::EndChild();
        ImGui::End();
        ImGui::Begin("right");
        facet::ShortcutPressed(right);
        ImGui::End();
    });
    session.Frame();
    CHECK_FALSE(facet::FindChordConflict(ImGuiMod_Ctrl | ImGuiKey_C, right).IsValid());
    CHECK(facet::FindChordConflict(ImGuiMod_Ctrl | ImGuiKey_C, left) == child);
    CHECK(facet::FindChordConflict(ImGuiMod_Ctrl | ImGuiKey_C, child) == left);
}
