// Key chords, chord text, key binds and the chord capture session (core/input.h,
// src/internal/keys.h), driven by scripted key events through the shared test session.

#include <string>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/core/input.h>

#include "internal/keys.h"
#include "support/session.h"

using facet::internal::ChordCapture;
using facet::internal::ChordCaptureEvent;

namespace
{

std::string Display(ImGuiKeyChord chord)
{
    char buffer[64];
    facet::ChordToText(chord, buffer, sizeof(buffer));
    return buffer;
}

std::string Config(ImGuiKeyChord chord)
{
    char buffer[64];
    facet::ChordToConfigText(chord, buffer, sizeof(buffer));
    return buffer;
}

ImGuiKeyChord Parse(const char* text)
{
    ImGuiKeyChord chord = -1;
    REQUIRE(facet::ChordFromText(text, &chord));
    return chord;
}

bool IsModifierEntry(ImGuiKey key)
{
    return key >= ImGuiKey_ReservedForModCtrl && key <= ImGuiKey_ReservedForModSuper;
}

constexpr ImGuiKeyChord kModifierSets[] = {0, ImGuiMod_Ctrl, ImGuiMod_Shift | ImGuiMod_Alt,
                                           ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiMod_Alt | ImGuiMod_Super};

} // namespace

TEST_CASE("Chords display with folded modifiers in a fixed order")
{
    CHECK(Display(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_K) == "Ctrl+Shift+K");
    CHECK(Display(ImGuiMod_Shift | ImGuiMod_Ctrl | ImGuiKey_K) == "Ctrl+Shift+K");
    CHECK(Display(ImGuiMod_Super | ImGuiMod_Alt | ImGuiKey_F4) == "Alt+Win+F4");
    CHECK(Display(ImGuiKey_PageUp) == "Page Up");
    CHECK(Display(ImGuiMod_Ctrl | ImGuiKey_Semicolon) == "Ctrl+;");
    CHECK(Display(ImGuiKey_KeypadAdd | ImGuiMod_Ctrl) == "Ctrl+Num +");
    CHECK(Display(ImGuiKey_MouseX1) == "Mouse 4");
    CHECK(Display(ImGuiKey_GamepadFaceDown) == "Gamepad A");
    CHECK(Display(ImGuiKey_None).empty());
    // Left and right modifier keys fold into their modifier.
    CHECK(Display(ImGuiKey_RightCtrl) == "Ctrl");
    CHECK(Display(ImGuiKey_LeftShift | ImGuiMod_Ctrl) == "Ctrl+Shift");
    CHECK(Display(ImGuiKey_LeftShift | ImGuiMod_Shift) == "Shift");
    CHECK(Display(ImGuiMod_Ctrl | ImGuiMod_Alt) == "Ctrl+Alt");

    CHECK(std::string(facet::KeyNameText(ImGuiKey_LeftCtrl)) == "Ctrl");
    CHECK(std::string(facet::KeyNameText(ImGuiKey_RightCtrl)) == "Ctrl");
    CHECK(std::string(facet::KeyNameText(static_cast<ImGuiKey>(ImGuiMod_Super))) == "Win");
    CHECK(std::string(facet::KeyNameText(ImGuiKey_None)).empty());
    CHECK(std::string(facet::KeyNameText(static_cast<ImGuiKey>(12345))).empty());
}

TEST_CASE("Every named key has a display and a configuration name")
{
    for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; ++k)
    {
        const ImGuiKey key = static_cast<ImGuiKey>(k);
        INFO("key ", ImGui::GetKeyName(key));
        CHECK(facet::KeyNameText(key)[0] != '\0');
        const std::string config = facet::internal::KeyConfigName(key);
        CHECK_FALSE(config.empty());
        // Configuration names are identifiers: letters and digits only.
        for (const char c : config)
            CHECK(((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')));
    }
}

TEST_CASE("The configuration text round-trips every key with modifiers exactly")
{
    for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; ++k)
    {
        const ImGuiKey key = static_cast<ImGuiKey>(k);
        if (IsModifierEntry(key))
            continue;
        for (const ImGuiKeyChord mods : kModifierSets)
        {
            const ImGuiKeyChord chord = key | mods;
            const std::string text = Config(chord);
            INFO("chord text ", text);
            ImGuiKeyChord parsed = 0;
            REQUIRE(facet::ChordFromText(text.c_str(), &parsed));
            CHECK(parsed == facet::internal::NormalizeChord(chord));
        }
    }
    CHECK(Config(ImGuiKey_RightCtrl) == "RightCtrl");
    CHECK(Config(ImGuiKey_LeftShift | ImGuiMod_Shift | ImGuiMod_Ctrl) == "Ctrl+LeftShift");
    CHECK(Config(ImGuiMod_Ctrl | ImGuiKey_Semicolon) == "Ctrl+Semicolon");
    CHECK(Config(ImGuiMod_Ctrl | ImGuiMod_Shift) == "Ctrl+Shift");
    CHECK(Parse("Ctrl+Shift") == (ImGuiMod_Ctrl | ImGuiMod_Shift));
}

TEST_CASE("The display text parses back to a chord for the same key presses")
{
    for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; ++k)
    {
        const ImGuiKey key = static_cast<ImGuiKey>(k);
        if (IsModifierEntry(key))
            continue;
        for (const ImGuiKeyChord mods : kModifierSets)
        {
            const ImGuiKeyChord chord = key | mods;
            const std::string text = Display(chord);
            INFO("chord text ", text);
            ImGuiKeyChord parsed = 0;
            REQUIRE(facet::ChordFromText(text.c_str(), &parsed));
            if (facet::internal::IsSidedModifierKey(key))
                CHECK(parsed == facet::internal::RequiredModifiers(chord));  // either side
            else
                CHECK(parsed == facet::internal::NormalizeChord(chord));
        }
    }
}

TEST_CASE("Chord text parsing accepts spacing, case and modifier spellings")
{
    CHECK(Parse("ctrl + shift + k") == (ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_K));
    CHECK(Parse("  Control+K  ") == (ImGuiMod_Ctrl | ImGuiKey_K));
    CHECK(Parse("Shift+Ctrl+K") == (ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_K));
    CHECK(Parse("Super+L") == (ImGuiMod_Super | ImGuiKey_L));
    CHECK(Parse("WIN+L") == (ImGuiMod_Super | ImGuiKey_L));
    CHECK(Parse("Ctrl+Num +") == (ImGuiMod_Ctrl | ImGuiKey_KeypadAdd));
    CHECK(Parse("page up") == ImGuiKey_PageUp);
    CHECK(Parse("PageUp") == ImGuiKey_PageUp);
    CHECK(Parse("Shift") == ImGuiMod_Shift);
    CHECK(Parse("RightCtrl") == (ImGuiKey_RightCtrl | ImGuiMod_Ctrl));
    CHECK(Parse("") == ImGuiKey_None);
    CHECK(Parse("   ") == ImGuiKey_None);

    ImGuiKeyChord chord = ImGuiKey_A;
    CHECK_FALSE(facet::ChordFromText("Ctrl+", &chord));
    CHECK_FALSE(facet::ChordFromText("Ctrl+Nope", &chord));
    CHECK_FALSE(facet::ChordFromText("Ctrl++", &chord));
    CHECK_FALSE(facet::ChordFromText("Ctrlx+K", &chord));
    CHECK_FALSE(facet::ChordFromText(nullptr, &chord));
    CHECK_FALSE(facet::ChordFromText("K", nullptr));
    CHECK(chord == ImGuiKey_A);  // unchanged after every failure
}

TEST_CASE("Chord text is cut at the buffer and the full length is returned")
{
    char shortBuffer[5];
    CHECK(facet::ChordToText(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_K, shortBuffer, sizeof(shortBuffer)) == 12);
    CHECK(std::string(shortBuffer) == "Ctrl");
    CHECK(facet::ChordToText(ImGuiKey_K, nullptr, 0) == 1);
    char one[1] = {'x'};
    CHECK(facet::ChordToConfigText(ImGuiKey_K, one, 1) == 1);
    CHECK(one[0] == '\0');
}

TEST_CASE("Chord queries need exactly the chord's modifiers to press, at least them to hold")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool down = false, pressed = false, pressedShift = false, heldShift = false;
    int pressCount = 0, repeatCount = 0;
    session.SetUi([&] {
        down = facet::IsChordDown(ImGuiMod_Ctrl | ImGuiKey_S);
        pressed = facet::IsChordPressed(ImGuiMod_Ctrl | ImGuiKey_S);
        pressedShift = facet::IsChordPressed(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_S);
        heldShift = facet::IsChordDown(ImGuiKey_LeftShift);
        pressCount += facet::IsChordPressed(ImGuiKey_D) ? 1 : 0;
        repeatCount += facet::IsChordPressed(ImGuiKey_D, true) ? 1 : 0;
    });
    session.Frame();

    session.KeyDown(ImGuiMod_Ctrl | ImGuiKey_S);
    CHECK(pressed);
    CHECK(down);
    session.Frame();
    CHECK_FALSE(pressed);  // only in the frame it went down
    CHECK(down);
    session.KeyUp(ImGuiMod_Ctrl | ImGuiKey_S);
    CHECK_FALSE(down);

    // Ctrl+Shift+S is not Ctrl+S, but a held Ctrl+S survives an extra Shift.
    session.KeyDown(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_S);
    CHECK_FALSE(pressed);
    CHECK(pressedShift);
    CHECK(down);
    session.KeyUp(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_S);

    // A sided modifier key binds that key alone.
    ImGui::GetIO().AddKeyEvent(ImGuiKey_LeftShift, true);
    ImGui::GetIO().AddKeyEvent(ImGuiMod_Shift, true);
    session.Frame();
    CHECK(heldShift);
    ImGui::GetIO().AddKeyEvent(ImGuiKey_LeftShift, false);
    ImGui::GetIO().AddKeyEvent(ImGuiMod_Shift, false);
    session.Frame();
    CHECK_FALSE(heldShift);

    // Repeat: held for a second, the key repeats only when asked for.
    pressCount = repeatCount = 0;
    session.HoldKey(ImGuiKey_D, 1.0f);
    CHECK(pressCount == 1);
    CHECK(repeatCount > 3);

    // Outside a frame nothing is down.
    CHECK_FALSE(facet::IsChordDown(ImGuiKey_D));
}

TEST_CASE("Modifier-only chords and mouse button chords")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool modsPressed = false, modsDown = false, mouseDown = false, ctrlMouse = false;
    session.SetUi([&] {
        modsPressed = facet::IsChordPressed(ImGuiMod_Ctrl | ImGuiMod_Shift);
        modsDown = facet::IsChordDown(ImGuiMod_Ctrl | ImGuiMod_Shift);
        mouseDown = facet::IsChordDown(ImGuiKey_MouseX1);
        ctrlMouse = facet::IsChordPressed(ImGuiMod_Ctrl | ImGuiKey_MouseX1);
    });
    session.Frame();
    session.KeyDown(ImGuiMod_Ctrl);
    CHECK_FALSE(modsPressed);
    session.KeyDown(ImGuiMod_Shift);
    CHECK(modsPressed);
    CHECK(modsDown);
    session.KeyUp(ImGuiMod_Shift);
    CHECK_FALSE(modsDown);

    session.MouseDown(3);  // ImGuiMouseButton 3 is the fourth button: ImGuiKey_MouseX1
    CHECK(mouseDown);
    CHECK(ctrlMouse);
    session.MouseUp(3);
    CHECK_FALSE(mouseDown);
    session.KeyUp(ImGuiMod_Ctrl);
}

TEST_CASE("Key binds: hold follows the key, toggle flips once per press, always and off")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    facet::Keybind hold{.primary = ImGuiKey_G, .mode = facet::BindMode::Hold};
    facet::Keybind toggle{.primary = ImGuiKey_T, .alternate = ImGuiMod_Ctrl | ImGuiKey_Y, .mode = facet::BindMode::Toggle};
    facet::Keybind always{.mode = facet::BindMode::Always};
    facet::Keybind off{.primary = ImGuiKey_G, .mode = facet::BindMode::Off};
    bool latch = false, latchCopy = false, offLatch = true;
    bool holdActive = false, toggleActive = false, toggleAgain = false, alwaysActive = false, offActive = true;
    bool pressedNoLatch = false;
    session.SetUi([&] {
        holdActive = facet::IsBindActive(hold, nullptr);
        toggleActive = facet::IsBindActive(toggle, &latch);
        toggleAgain = facet::IsBindActive(toggle, &latch);  // same latch twice: one flip
        latchCopy = latch;
        pressedNoLatch = facet::IsBindActive(toggle, nullptr);
        alwaysActive = facet::IsBindActive(always, nullptr);
        offActive = facet::IsBindActive(off, &offLatch);
    });
    session.Frame();
    CHECK_FALSE(holdActive);
    CHECK(alwaysActive);
    CHECK_FALSE(offActive);

    session.KeyDown(ImGuiKey_G);
    CHECK(holdActive);
    CHECK_FALSE(offActive);
    CHECK(offLatch);  // Off leaves the latch alone
    session.Seconds(0.5f);
    CHECK(holdActive);
    session.KeyUp(ImGuiKey_G);
    CHECK_FALSE(holdActive);

    session.KeyDown(ImGuiKey_T);
    CHECK(toggleActive);
    CHECK(toggleAgain);
    CHECK(latchCopy);
    CHECK(pressedNoLatch);
    // Key repeat does not flip it again.
    session.Seconds(1.0f);
    CHECK(toggleActive);
    session.KeyUp(ImGuiKey_T);
    CHECK(toggleActive);
    session.KeyPress(ImGuiKey_T);
    CHECK_FALSE(toggleActive);
    // The alternate chord flips it too; both chords in one frame flip it once.
    session.KeyPress(ImGuiMod_Ctrl | ImGuiKey_Y);
    CHECK(toggleActive);
    ImGui::GetIO().AddKeyEvent(ImGuiKey_T, true);
    session.KeyDown(ImGuiMod_Ctrl | ImGuiKey_Y);
    CHECK_FALSE(toggleActive);
    ImGui::GetIO().AddKeyEvent(ImGuiKey_T, false);
    session.KeyUp(ImGuiMod_Ctrl | ImGuiKey_Y);
    CHECK_FALSE(toggleActive);
}

TEST_CASE("Key binds ignore keyboard chords while a text field has the keyboard, unless asked")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    facet::Keybind plain{.primary = ImGuiKey_G, .mode = facet::BindMode::Hold};
    facet::Keybind typing{.primary = ImGuiKey_G, .mode = facet::BindMode::Hold, .whileTextInput = true};
    facet::Keybind mouse{.primary = ImGuiKey_MouseX2, .mode = facet::BindMode::Hold};
    bool plainActive = false, typingActive = false, mouseActive = false;
    char text[32] = "";
    session.SetUi([&] {
        ImGui::InputText("field", text, sizeof(text));
        session.Track("field");
        plainActive = facet::IsBindActive(plain, nullptr);
        typingActive = facet::IsBindActive(typing, nullptr);
        mouseActive = facet::IsBindActive(mouse, nullptr);
    });
    session.Frame();
    session.Click(session.ItemCenter("field"));
    REQUIRE(ImGui::GetIO().WantTextInput);
    session.KeyDown(ImGuiKey_G);
    CHECK_FALSE(plainActive);
    CHECK(typingActive);
    session.KeyUp(ImGuiKey_G);
    session.MouseDown(4);
    CHECK(mouseActive);
    session.MouseUp(4);

    // With the field closed the plain bind works again.
    session.KeyPress(ImGuiKey_Escape);
    REQUIRE_FALSE(ImGui::GetIO().WantTextInput);
    session.KeyDown(ImGuiKey_G);
    CHECK(plainActive);
    session.KeyUp(ImGuiKey_G);
}

namespace
{

// A minimal capture item: a button that starts listening on click and stays the active item
// while it listens, as KeybindCapture will.
struct CaptureHarness
{
    ChordCapture capture;
    facet::internal::ChordCaptureOptions options;
    ChordCaptureEvent lastEvent = ChordCaptureEvent::None;
    int events = 0;
    bool startNow = false;
    bool updateOnStart = false;  // a widget may update the session in the frame it starts it

    void Draw(facet::test::Session& session)
    {
        const ImGuiID id = ImGui::GetID("capture");
        if (capture.Listening())
        {
            const ImVec2 pos = ImGui::GetCursorScreenPos();
            const ImRect bb(pos, ImVec2(pos.x + 100.0f, pos.y + 20.0f));
            ImGui::ItemSize(bb);
            ImGui::ItemAdd(bb, id);
            const ChordCaptureEvent event = capture.Update();
            if (event != ChordCaptureEvent::None)
            {
                lastEvent = event;
                ++events;
                if (ImGui::GetActiveID() == id)
                    ImGui::ClearActiveID();
            }
        }
        else if (ImGui::Button("capture") || startNow)
        {
            startNow = false;
            capture.Start(id, options);
            ImGui::SetActiveID(id, ImGui::GetCurrentWindow());
            if (updateOnStart && capture.Update() != ChordCaptureEvent::None)
                ++events;
        }
        session.Track("capture");
        if (ImGui::Button("next"))
        {
        }
        session.Track("next");
    }
};

} // namespace

TEST_CASE("Chord capture reports the chord when its last key is released")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    CaptureHarness harness;
    session.SetUi([&] { harness.Draw(session); });
    session.Frame();
    session.Click(session.ItemCenter("capture"));
    REQUIRE(harness.capture.Listening());

    session.KeyDown(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_K);
    CHECK(harness.capture.Held() == (ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_K));
    CHECK(harness.events == 0);
    // Releasing the key first and the modifiers later still captures one chord, on the last
    // release.
    ImGui::GetIO().AddKeyEvent(ImGuiKey_K, false);
    session.Frame();
    CHECK(harness.events == 0);
    session.KeyUp(ImGuiMod_Ctrl | ImGuiMod_Shift);
    CHECK(harness.events == 1);
    CHECK(harness.lastEvent == ChordCaptureEvent::Captured);
    CHECK(harness.capture.Result() == (ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_K));
    CHECK_FALSE(harness.capture.Listening());
}

TEST_CASE("Chord capture: Escape cancels, the starting key is ignored, Clear ends the session")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    CaptureHarness harness;
    session.SetUi([&] { harness.Draw(session); });
    session.Frame();

    session.Click(session.ItemCenter("capture"));
    session.KeyPress(ImGuiKey_Escape);
    CHECK(harness.lastEvent == ChordCaptureEvent::Canceled);
    CHECK(harness.capture.Result() == 0);

    // The key that starts the capture (Enter, in the frame it goes down) is not recorded, even
    // when the session is updated in that same frame; the next key is.
    harness.updateOnStart = true;
    harness.startNow = true;
    session.KeyDown(ImGuiKey_Enter);
    REQUIRE(harness.capture.Listening());
    session.KeyUp(ImGuiKey_Enter);
    CHECK(harness.capture.Listening());
    CHECK(harness.events == 1);  // only the Escape above
    session.KeyPress(ImGuiKey_B);
    CHECK(harness.lastEvent == ChordCaptureEvent::Captured);
    CHECK(harness.capture.Result() == ImGuiKey_B);
    harness.updateOnStart = false;

    harness.startNow = true;
    session.Frame();
    REQUIRE(harness.capture.Listening());
    harness.capture.Clear();
    session.Frame();
    CHECK_FALSE(harness.capture.Listening());
    CHECK(harness.capture.Last() == ChordCaptureEvent::Cleared);
}

TEST_CASE("Chord capture: modifiers alone, mouse buttons only when allowed")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    CaptureHarness harness;
    session.SetUi([&] { harness.Draw(session); });
    session.Frame();

    // Without allowModifierOnly a press of modifiers alone is ignored and listening goes on.
    harness.startNow = true;
    session.Frame();
    session.KeyPress(ImGuiMod_Ctrl | ImGuiMod_Shift);
    CHECK(harness.capture.Listening());
    CHECK(harness.events == 0);
    session.KeyPress(ImGuiMod_Alt | ImGuiKey_F4);
    CHECK(harness.capture.Result() == (ImGuiMod_Alt | ImGuiKey_F4));

    harness.options.allowModifierOnly = true;
    harness.startNow = true;
    session.Frame();
    session.KeyPress(ImGuiMod_Ctrl | ImGuiMod_Shift);
    CHECK(harness.lastEvent == ChordCaptureEvent::Captured);
    CHECK(harness.capture.Result() == (ImGuiMod_Ctrl | ImGuiMod_Shift));

    // A mouse button cancels while mouse buttons are not accepted ...
    harness.options = {};
    harness.startNow = true;
    session.Frame();
    session.MouseDown(3);
    CHECK(harness.lastEvent == ChordCaptureEvent::Canceled);
    session.MouseUp(3);
    // ... and is the chord when they are.
    harness.options.allowMouse = true;
    harness.startNow = true;
    session.Frame();
    session.KeyDown(ImGuiMod_Ctrl);
    session.MouseDown(3);
    session.MouseUp(3);
    session.KeyUp(ImGuiMod_Ctrl);
    CHECK(harness.lastEvent == ChordCaptureEvent::Captured);
    CHECK(harness.capture.Result() == (ImGuiMod_Ctrl | ImGuiKey_MouseX1));
}

TEST_CASE("While capturing, host shortcuts, binds, stock key queries and navigation see nothing")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    CaptureHarness harness;
    int hostShortcut = 0, legacyPressed = 0, bindPressed = 0;
    const facet::Keybind bind{.primary = ImGuiMod_Ctrl | ImGuiKey_K, .mode = facet::BindMode::Hold};
    session.SetUi([&] {
        harness.Draw(session);
        hostShortcut += ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_K, ImGuiInputFlags_RouteGlobal) ? 1 : 0;
        legacyPressed += ImGui::IsKeyPressed(ImGuiKey_K, false) ? 1 : 0;
        bindPressed += facet::IsBindPressed(bind) ? 1 : 0;
    });
    session.Frame();

    // Without a capture all three see the chord.
    session.KeyPress(ImGuiMod_Ctrl | ImGuiKey_K);
    REQUIRE(hostShortcut == 1);
    REQUIRE(legacyPressed == 1);
    REQUIRE(bindPressed == 1);

    session.Click(session.ItemCenter("capture"));
    REQUIRE(harness.capture.Listening());
    REQUIRE(facet::internal::ActiveChordCaptureOwner() == ImGui::GetActiveID());
    session.KeyPress(ImGuiMod_Ctrl | ImGuiKey_K);
    CHECK(harness.capture.Result() == (ImGuiMod_Ctrl | ImGuiKey_K));
    CHECK(hostShortcut == 1);
    CHECK(bindPressed == 1);
    // The capture draws before the stock query in this frame order; facet::NewFrame locks
    // keys held since the last frame, so a stock query after it sees nothing either.
    CHECK(legacyPressed == 1);

    // Tab does not move the keyboard focus while listening: it is captured.
    session.Click(session.ItemCenter("capture"));
    REQUIRE(harness.capture.Listening());
    const ImGuiID navBefore = GImGui->NavId;
    session.KeyPress(ImGuiKey_Tab);
    CHECK(harness.capture.Result() == ImGuiKey_Tab);
    CHECK(GImGui->NavId == navBefore);
}
