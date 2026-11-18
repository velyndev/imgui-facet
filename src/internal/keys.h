#pragma once

// Key helpers behind core/input.h and the chord capture session that KeybindCapture and
// KeybindList build on.
//
// Chord capture, as a widget uses it:
//
//     // State kept under the widget id (for example in state_storage.h):
//     facet::internal::ChordCapture capture;
//
//     // The widget's item is active while it listens (ImGui::SetActiveID on the click that
//     // starts it), so Dear ImGui keeps the keys away from navigation:
//     if (startClicked)
//         capture.Start(id, {.allowMouse = options.allowMouse});
//     if (capture.Listening())
//     {
//         switch (capture.Update())
//         {
//         case ChordCaptureEvent::Captured: bind->primary = capture.Result(); written = true; break;
//         case ChordCaptureEvent::Canceled: break;          // Escape or a left click: keep the old chord
//         default: break;                                   // still listening; draw capture.Held()
//         }
//     }
//     // A left press cancels the session without taking the press, so the clear button
//     // receives the click even while the field listens:
//     if (clearClicked) { capture.Clear(); bind->primary = ImGuiKey_None; written = true; }
//
// While a capture listens, its keys belong to it: every key it accepts that is down is locked
// to the capture's owner id through Dear ImGui's key ownership (SetKeyOwner with
// ImGuiInputFlags_LockUntilRelease), so ImGui::Shortcut, the shortcut registry, IsChordPressed,
// key binds and Dear ImGui's own queries (IsKeyPressed and IsKeyDown without an owner) all
// see nothing. When the owner is the active item, the capture also claims every keyboard key
// for it (SetActiveIdUsingAllKeyboardKeys), which keeps Tab, the arrows and Escape away from
// keyboard navigation and popups even in the frame a key goes down. facet::NewFrame repeats
// the locks for keys pressed since the last frame before any app code runs.

#include <cstdint>

#include <imgui.h>

namespace facet::internal
{

// --- Keys -----------------------------------------------------------------------------------

// The four modifier flags of ImGuiKeyChord, in display order.
inline constexpr ImGuiKeyChord kChordModifiers[4] = {ImGuiMod_Ctrl, ImGuiMod_Shift, ImGuiMod_Alt, ImGuiMod_Super};

// ImGuiKey_LeftCtrl ... ImGuiKey_RightSuper.
bool IsSidedModifierKey(ImGuiKey key);
// The ImGuiMod_* flag of a sided modifier key or of a single ImGuiMod_* flag, else 0.
ImGuiKeyChord ModifierFlagOf(ImGuiKey key);
// The chord with the flag of its sided modifier key added (LeftShift becomes
// LeftShift | ImGuiMod_Shift), the form Dear ImGui compares; other chords unchanged.
ImGuiKeyChord NormalizeChord(ImGuiKeyChord chord);
// Every modifier flag the chord needs held: its flags plus the flag of a sided modifier key.
ImGuiKeyChord RequiredModifiers(ImGuiKeyChord chord);

// The display name of a named key (facet::KeyNameText adds the modifier flags), "" otherwise.
const char* KeyDisplayName(ImGuiKey key);

// The configuration name of a key: an identifier without spaces or punctuation that stays the
// same in every release ("LeftCtrl", "PageUp", "Semicolon", "Num0", "MouseX1",
// "GamepadFaceDown"). "" for ImGuiKey_None and keys without a name.
const char* KeyConfigName(ImGuiKey key);

// facet::ChordToConfigText (input.h) writes chords with these names.

// Looks a key name up, case-insensitively, among the display names (KeyNameText) and the
// configuration names. Modifier names are not keys here; ChordFromText handles them.
ImGuiKey FindKeyByName(const char* begin, const char* end);

// --- Chord capture --------------------------------------------------------------------------

struct ChordCaptureOptions
{
    bool allowMouse = false;         // the right, middle, 4 and 5 buttons can be the key; the
                                     // left button never is: it cancels
    bool allowGamepad = false;       // gamepad buttons and stick directions can be the key
    bool allowModifierOnly = false;  // a chord of modifiers alone ("Ctrl+Shift") is accepted
};

enum class ChordCaptureEvent
{
    None,      // not listening, or still listening
    Captured,  // the last key of a chord was released; Result() holds the chord
    Canceled,  // Escape was pressed, the left button or a mouse button that is not accepted,
               // another session started, or a frame passed without an Update
    Cleared,   // Clear() ended the session
};

// One capture session. Plain data: keep it in the widget's state under its id. Only one
// session listens at a time; starting one ends any other: the older session's next Update
// returns Canceled and records nothing.
class ChordCapture
{
public:
    // Starts listening for `owner`, the id of the item that shows the capture (0 uses an id
    // of its own). Keys that are already down are ignored until released, so the click or
    // Enter that started the capture is not recorded.
    void Start(ImGuiID owner, const ChordCaptureOptions& options = {});

    // Call once per frame while Listening(), inside the frame, before the widget draws. A
    // session that missed a frame (its widget was not submitted) ends as Canceled on the next
    // call. Returns Captured, Canceled or Cleared on the frame the session ends, None otherwise.
    // A chord is captured when its last key is released: the modifiers held when the main key
    // went down, plus that key (the last non-modifier key pressed). With allowModifierOnly, a
    // press and release of modifiers alone captures their flags (ImGuiMod_Ctrl | ImGuiMod_Shift,
    // no key); without it such a gesture is ignored and listening goes on. Escape cancels on
    // its press, so Escape cannot be captured.
    ChordCaptureEvent Update();

    // Ends the session as canceled or cleared (a widget's clear button). The next Update, if
    // any, returns the event; Last() keeps it too.
    void Cancel();
    void Clear();

    bool Listening() const { return listening; }
    ImGuiID Owner() const { return owner; }
    // The chord held right now (for a live preview while listening), 0 when nothing is held.
    ImGuiKeyChord Held() const { return held; }
    // The captured chord after Captured; 0 otherwise.
    ImGuiKeyChord Result() const { return result; }
    // The event that ended the last session.
    ChordCaptureEvent Last() const { return last; }

private:
    void Finish(ChordCaptureEvent event);

    ImGuiID owner = 0;
    ChordCaptureOptions options;
    bool listening = false;
    std::uint32_t generation = 0;       // the Start count of this session (see keys.cpp)
    int lastFrame = -1;                 // ImGui frame of the last Start or Update
    ChordCaptureEvent pendingEnd = ChordCaptureEvent::None;
    ChordCaptureEvent last = ChordCaptureEvent::None;
    ImGuiKeyChord held = 0;
    ImGuiKeyChord result = 0;
    ImGuiKey mainKey = ImGuiKey_None;   // last non-modifier key pressed in the gesture
    ImGuiKeyChord mainMods = 0;         // modifiers held when mainKey went down
    ImGuiKeyChord peakMods = 0;         // every modifier held during the gesture
    bool gesture = false;               // a key was pressed since the last chord or start
    // Keys down at Start, ignored until released: one bit per named key.
    std::uint64_t ignored[(ImGuiKey_NamedKey_COUNT + 63) / 64] = {};
};

// The owner of the session listening this frame (or the last one), 0 when none: for tests.
ImGuiID ActiveChordCaptureOwner();

// Forgets the listening session (facet::Shutdown): no owner, and every existing session ends
// as Canceled on its next Update.
void ResetChordCaptureState();

// Called by the input module in facet::NewFrame: repeats the locks of the session that
// listened in the previous frame, and forgets a session that was not updated then.
void ChordCaptureNewFrame();

} // namespace facet::internal
