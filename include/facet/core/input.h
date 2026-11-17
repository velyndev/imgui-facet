#pragma once

// Key chords, key binds and the shortcut registry.
//
// A chord is Dear ImGui's ImGuiKeyChord: one ImGuiKey (a keyboard key, a mouse button such as
// ImGuiKey_MouseX1, or a gamepad button such as ImGuiKey_GamepadFaceDown) combined with
// ImGuiMod_Ctrl, ImGuiMod_Shift, ImGuiMod_Alt and ImGuiMod_Super. ImGuiKey_None (0) is no
// chord. A chord whose key is a modifier key alone (ImGuiKey_LeftShift) binds that key.
//
// Facet reads input only from ImGuiIO, so scripted input drives all of this, and it never
// changes io settings. Every function is for the thread that runs the Dear ImGui frame (see
// context.h); Keybind values may be built on any thread. The queries need a current Dear ImGui
// context and return false (or nothing) without one.
//
// Widgets built on this: KeybindCapture (records a Keybind), KeybindModeSelector (edits its
// BindMode), KeyCap and ShortcutHint (draw a chord with KeyNameText), MenuItem (shows a
// registered shortcut), CommandPalette (lists and triggers shortcuts) and KeybindList (edits
// the registry).

#include <cstdint>

#include <imgui.h>

namespace facet
{

// --- Chords ---------------------------------------------------------------------------------

// The key of a chord without its modifier flags.
constexpr ImGuiKey ChordKey(ImGuiKeyChord chord)
{
    return static_cast<ImGuiKey>(chord & ~ImGuiMod_Mask_);
}

// The ImGuiMod_* flags of a chord.
constexpr ImGuiKeyChord ChordMods(ImGuiKeyChord chord)
{
    return chord & ImGuiMod_Mask_;
}

// The key is down together with every modifier the chord names; other modifiers may be down
// too, so a held binding survives an extra Shift.
bool IsChordDown(ImGuiKeyChord chord);

// The key went down this frame (or repeated, with `repeat`) while exactly the chord's
// modifiers are down, so Ctrl+S does not fire for Ctrl+Shift+S.
bool IsChordPressed(ImGuiKeyChord chord, bool repeat = false);

// The display name of one key, never null: "Ctrl", "Shift", "Alt", "Win", "A", "F5", "Space",
// "Page Up", "Mouse 4", "Gamepad A". English; built-in strings, valid for the whole process.
const char* KeyNameText(ImGuiKey key);

// Writes the chord as text, modifiers first in the order Ctrl, Shift, Alt, Win, joined by "+"
// ("Ctrl+Shift+S"), into buffer (bufferSize bytes, always zero terminated when bufferSize > 0)
// and returns the full length, which may exceed bufferSize - 1 when the text was cut.
// ImGuiKey_None writes "". The text parses back with ChordFromText.
int ChordToText(ImGuiKeyChord chord, char* buffer, int bufferSize);

// Parses text written by ChordToText, case-insensitive, with optional spaces around "+".
// Also accepts "Control" and "Super" for the modifiers. Returns false and leaves *chord
// unchanged when the text names no key or an unknown one; "" parses as ImGuiKey_None.
bool ChordFromText(const char* text, ImGuiKeyChord* chord);

// --- Key binds ------------------------------------------------------------------------------

// How a bind turns input into an active state.
enum class BindMode
{
    Hold,    // active while a chord is held
    Toggle,  // each press of a chord flips the state
    Always,  // always active, whatever is pressed
    Off,     // never active
};

// A binding with a primary and an alternate chord; either one triggers it. Plain data that the
// app stores and saves (for example as ChordToText text).
struct Keybind
{
    ImGuiKeyChord primary = ImGuiKey_None;    // ImGuiKey_None: unbound
    ImGuiKeyChord alternate = ImGuiKey_None;  // ImGuiKey_None: unbound
    BindMode mode = BindMode::Hold;
};

// Whether the bind is active this frame. Hold: IsChordDown of either chord. Toggle: a press of
// either chord (IsChordPressed, no repeat) flips *toggleState, and the result is *toggleState;
// the caller owns the latch, so one Keybind can drive several states. Always: true. Off:
// false. `toggleState` may be null for the modes other than Toggle; Toggle with a null latch
// returns whether a chord was pressed this frame. Unbound chords never match.
bool IsBindActive(const Keybind& bind, bool* toggleState);

// Either chord of the bind was pressed this frame, whatever the mode (Off excepted).
bool IsBindPressed(const Keybind& bind);

// --- Shortcut registry ----------------------------------------------------------------------

// Handle of a registered shortcut; 0 is invalid.
struct ShortcutId
{
    std::uint32_t value = 0;
    constexpr bool IsValid() const { return value != 0; }
    friend constexpr bool operator==(const ShortcutId&, const ShortcutId&) = default;
};

// Where a shortcut fires. The routing is Dear ImGui's (ImGui::Shortcut route flags), decided
// where ShortcutPressed is called.
enum class ShortcutScope
{
    Global,   // anywhere in the app, unless a focused widget owns the key (a text field)
    Window,   // only while the window that calls ShortcutPressed, or a child of it, is focused
};

struct ShortcutOptions
{
    const char* label = nullptr;        // shown in lists and menus; null uses the name
    const char* group = nullptr;        // heading that groups shortcuts in lists; null: none
    ImGuiKeyChord alternate = ImGuiKey_None;  // a second chord
    ShortcutScope scope = ShortcutScope::Global;
    bool repeat = false;                // fires again while the key repeats
    bool enabled = true;
};

// Registers a shortcut under a unique, stable `name` ("file.save"), which also keys saved
// rebinds. Strings are copied. The chord given here is the default the shortcut returns to
// with ResetShortcut. Registering a name that exists replaces its entry and keeps its id.
// Returns an invalid id for a null or empty name.
ShortcutId RegisterShortcut(const char* name, ImGuiKeyChord chord, const ShortcutOptions& options = {});
// Removes the shortcut; its id becomes invalid. Unknown ids are ignored.
void UnregisterShortcut(ShortcutId id);
// The shortcut registered under `name`, or an invalid id.
ShortcutId FindShortcut(const char* name);

// A disabled shortcut stays listed but never fires.
void SetShortcutEnabled(ShortcutId id, bool enabled);
// Rebinds the shortcut's chords (KeybindList); ImGuiKey_None clears one.
void SetShortcutChords(ShortcutId id, ImGuiKeyChord primary, ImGuiKeyChord alternate);
// Back to the chords given at registration.
void ResetShortcut(ShortcutId id);

// True on the frame the shortcut fires: a chord was pressed in the shortcut's scope, or
// TriggerShortcut asked for it. Call it once per frame where the app handles the action; for
// ShortcutScope::Window, call it inside the window it belongs to.
bool ShortcutPressed(ShortcutId id);
// Makes ShortcutPressed return true for this shortcut in the next frame, as if its chord had
// been pressed (CommandPalette runs commands this way). Disabled shortcuts are not triggered.
void TriggerShortcut(ShortcutId id);

// One registered shortcut, for lists. The strings belong to the registry and stay valid until
// the shortcut is unregistered or replaced.
struct ShortcutInfo
{
    ShortcutId id;
    const char* name = "";
    const char* label = "";
    const char* group = "";              // "" when none
    ImGuiKeyChord primary = ImGuiKey_None;
    ImGuiKeyChord alternate = ImGuiKey_None;
    ImGuiKeyChord defaultPrimary = ImGuiKey_None;
    ImGuiKeyChord defaultAlternate = ImGuiKey_None;
    ShortcutScope scope = ShortcutScope::Global;
    bool enabled = true;
};

// Number of registered shortcuts.
int ShortcutCount();
// The shortcut at `index` (0 to ShortcutCount() - 1) in registration order; false when the
// index is out of range.
bool GetShortcut(int index, ShortcutInfo* info);
bool GetShortcut(ShortcutId id, ShortcutInfo* info);
// Another enabled shortcut in an overlapping scope that uses `chord` as a chord, for conflict
// marks; invalid when there is none. `except` is skipped.
ShortcutId FindChordConflict(ImGuiKeyChord chord, ShortcutId except = {});

} // namespace facet
