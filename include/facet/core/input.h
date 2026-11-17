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
// "Page Up", "Esc", ";", "Num 1", "Mouse 4", "Gamepad A". Left and right modifier keys share
// one name ("Ctrl"), and a single ImGuiMod_* flag gives its modifier's name. English;
// built-in strings, valid for the whole process. "" for ImGuiKey_None and values that are not
// keys. The names do not follow the keyboard layout: a punctuation key is named by its US
// label (docs/input.md, "Keyboard layouts").
const char* KeyNameText(ImGuiKey key);

// Writes the chord for display, modifiers first in the order Ctrl, Shift, Alt, Win, joined by
// "+" ("Ctrl+Shift+S"), into buffer (bufferSize bytes, always zero terminated when
// bufferSize > 0) and returns the full length, which may exceed bufferSize - 1 when the text
// was cut. ImGuiKey_None writes "". A modifier key is folded into the modifiers, so LeftShift
// with Ctrl reads "Ctrl+Shift" whichever side it is. The text parses back with ChordFromText
// to a chord that matches the same key presses, except that a sided modifier key comes back
// as its modifier flag (either side); ChordToConfigText keeps the side.
int ChordToText(ImGuiKeyChord chord, char* buffer, int bufferSize);

// Writes the chord in its configuration form, for files that store bindings: modifiers Ctrl,
// Shift, Alt, Super, then a key identifier without spaces or punctuation that never changes
// ("Ctrl+Shift+K", "LeftShift", "Ctrl+Semicolon", "Num0", "MouseX1", "GamepadFaceDown").
// ChordFromText reads it back to exactly the same chord (a sided modifier key with or without
// its own flag reads back with it). Same buffer contract as ChordToText.
int ChordToConfigText(ImGuiKeyChord chord, char* buffer, int bufferSize);

// Parses text written by ChordToText or ChordToConfigText, case-insensitive, with optional
// spaces around "+". Modifiers come first, in any order; "Control" and "Super" are accepted
// for Ctrl and Win. Modifiers alone ("Ctrl+Shift") give their flags without a key. Returns
// false and leaves *chord unchanged when the text names an unknown key, ends in "+", or
// `text` or `chord` is null; "" parses as ImGuiKey_None. Needs no Dear ImGui context.
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
// app stores and saves (for example as ChordToConfigText text).
struct Keybind
{
    ImGuiKeyChord primary = ImGuiKey_None;    // ImGuiKey_None: unbound
    ImGuiKeyChord alternate = ImGuiKey_None;  // ImGuiKey_None: unbound
    BindMode mode = BindMode::Hold;
    // While a text field has the keyboard (io.WantTextInput), keyboard chords are ignored so
    // typing never triggers the bind; true keeps them working. Mouse and gamepad chords work
    // either way.
    bool whileTextInput = false;
};

// Whether the bind is active this frame. Hold: IsChordDown of either chord, so the state
// follows the keys. Toggle: a press of either chord (IsChordPressed without repeat, so a held
// key flips once) flips *toggleState, once even when both chords go down in the same frame,
// and the result is *toggleState; the caller owns the latch, so one Keybind can drive several
// states. Always: true. Off: false, and the latch is left alone. `toggleState` may be null for
// the modes other than Toggle; Toggle with a null latch returns whether a chord was pressed
// this frame. Unbound chords never match, and keys locked by a chord capture never count.
// A latch flips at most once per frame, so asking for the same bind and latch from two places
// in one frame gives the same answer (the latches flipped in a frame are remembered by
// address, up to 64 per frame).
bool IsBindActive(const Keybind& bind, bool* toggleState);

// Either chord of the bind was pressed this frame (no repeat), whatever the mode (Off
// excepted), with the same text field rule as IsBindActive.
bool IsBindPressed(const Keybind& bind);

// --- Shortcut registry ----------------------------------------------------------------------

// Handle of a registered shortcut; 0 is invalid.
struct ShortcutId
{
    std::uint32_t value = 0;
    constexpr bool IsValid() const { return value != 0; }
    friend constexpr bool operator==(const ShortcutId&, const ShortcutId&) = default;
};

// Where a shortcut fires. The routing is Dear ImGui's shortcut routing (ImGui::Shortcut with an
// owner id per shortcut chord), decided where ShortcutPressed is called. When several scopes
// want the same chord in a frame, the innermost one wins: the focused item, then the focused
// window (the deepest child first), then global, whatever order ShortcutPressed is called in.
// An active item (a text field being edited, a slider being dragged) keeps the chords it asks
// Dear ImGui for against every scope. A text field that is being edited keeps the keys it
// uses: chords that type a character (a letter, digit or punctuation key without Ctrl) never
// fire while one is active, and the field's own chords (Ctrl+A, Ctrl+Z) win. Shortcuts never
// fire inside ImGui::BeginDisabled, not even when triggered.
enum class ShortcutScope
{
    Global,       // anywhere in the app
    Window,       // only while the window that calls ShortcutPressed, or a child of it, is focused
    FocusedItem,  // only while the item submitted right before ShortcutPressed has keyboard focus
};

struct ShortcutOptions
{
    const char* label = nullptr;        // shown in lists and menus; null uses the name
    const char* group = nullptr;        // heading that groups shortcuts in lists; null: none
    ImGuiKeyChord alternate = ImGuiKey_None;  // a second chord
    ShortcutScope scope = ShortcutScope::Global;
    bool repeat = false;                // fires again while the key repeats
    bool enabled = true;
    // Called with the shortcut's id and `user` on the frame it fires, from ShortcutPressed, or
    // from facet::EndFrame for a Global shortcut that ShortcutPressed was not called for in
    // that frame. Window and FocusedItem shortcuts need their ShortcutPressed call, which
    // tells Facet where they belong. A callback may register, replace or unregister
    // shortcuts, its own included. Null: no callback.
    void (*callback)(ShortcutId id, void* user) = nullptr;
    void* user = nullptr;
};

// Registers a shortcut under a unique, stable `name` ("file.save"), which also keys saved
// rebinds. Strings are copied. The chord given here is the default the shortcut returns to
// with ResetShortcut. Registering a name that exists replaces its entry and keeps its id.
// Returns an invalid id for a null or empty name, or when memory runs out. Works before
// facet::Init and outside frames; the registry lives for the whole process.
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
// TriggerShortcut asked for it. Call it once per frame where the app handles the action (a
// second call in the same frame gives the same answer); for ShortcutScope::Window, call it
// inside the window it belongs to, for FocusedItem right after the item. The routing of a
// chord is requested in each call and granted from the next frame on, as with ImGui::Shortcut,
// so a shortcut that was not asked for in the previous frame cannot fire. A chord of two or
// more modifiers without a key cannot be routed and never fires (ChordFromText can produce
// one). False outside a frame and for disabled or unknown shortcuts.
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
// marks; invalid when there is none (or `chord` is ImGuiKey_None). `except` is skipped, and its
// scope is the one compared; without `except` the chord is checked as a Global one. Scopes
// overlap when either is Global; two Window or FocusedItem shortcuts overlap when they were
// last asked for in the same window or in a window and one of its children, or when either has
// not been asked for yet (its window is unknown); two FocusedItem shortcuts on different items
// never overlap. Chords compare after folding a sided modifier key's flag in (LeftShift equals
// LeftShift + Shift); LeftCtrl and RightCtrl stay different keys. The first conflicting entry
// in registration order is returned.
ShortcutId FindChordConflict(ImGuiKeyChord chord, ShortcutId except = {});

} // namespace facet
