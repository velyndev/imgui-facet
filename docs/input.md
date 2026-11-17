# Keys, binds and shortcuts

`facet/core/input.h` covers three things: key chords and their text, key binds that an app queries every frame, and a registry of named shortcuts. Everything reads input from `ImGuiIO` only, so scripted input (tests, the demo's selftest) drives it exactly like a keyboard. Facet never changes io settings. All functions run on the thread of the Dear ImGui frame; `Keybind` and `ShortcutOptions` are plain data and can be built anywhere.

## Chords

A chord is Dear ImGui's `ImGuiKeyChord`: one `ImGuiKey` combined with `ImGuiMod_Ctrl`, `ImGuiMod_Shift`, `ImGuiMod_Alt` and `ImGuiMod_Super`. The key may be a keyboard key, a mouse button (`ImGuiKey_MouseLeft` to `ImGuiKey_MouseX2`) or a gamepad button. `ImGuiKey_None` is no chord. A chord of modifiers alone (`ImGuiMod_Ctrl | ImGuiMod_Shift`) is valid for queries and binds.

| Call | Meaning |
|---|---|
| `IsChordDown(chord)` | The key and every modifier the chord names are down. Extra modifiers may be down too, so a held bind survives an extra Shift. |
| `IsChordPressed(chord, repeat)` | The key went down this frame (or repeated, with `repeat`) while exactly the chord's modifiers are down: Ctrl+S does not fire for Ctrl+Shift+S. For modifiers alone, the frame the last of them goes down. |
| `ChordKey`, `ChordMods` | The two halves of a chord. |

A sided modifier key (`ImGuiKey_LeftShift`) as the key of a chord binds that key alone; its own modifier flag is implied. Both queries return false without a Dear ImGui context or outside a frame. Keys locked by a chord capture (below) never count.

## Chord text

There are two text forms.

| Form | Function | Example | Use |
|---|---|---|---|
| Display | `ChordToText` | `Ctrl+Shift+K`, `Ctrl+;`, `Page Up`, `Mouse 4`, `Gamepad A` | Menus, key caps, lists |
| Configuration | `ChordToConfigText` | `Ctrl+Shift+K`, `Ctrl+Semicolon`, `PageUp`, `MouseX1`, `GamepadFaceDown` | Config files |

Both write the modifiers first, in the order Ctrl, Shift, Alt, Win (`Super` in the configuration form), joined by `+`. `KeyNameText(key)` gives the display name of one key; every named key has one.

The display form folds left and right modifiers: `LeftCtrl` and `RightCtrl` both read `Ctrl`, and a sided modifier key joins the modifiers (`LeftShift` with Ctrl reads `Ctrl+Shift`). Parsing it back gives a chord for the same key presses, with a sided modifier key turned into its flag (either side).

The configuration form keeps everything: key names are identifiers that never change, without spaces or punctuation (a `;` would start a comment in many config formats), and sided modifier keys keep their side (`RightCtrl`). `ChordFromText` reads it back to exactly the same chord.

`ChordFromText(text, &chord)` reads both forms: case-insensitive, spaces around `+` allowed, modifiers in any order, `Control` for Ctrl and `Win` or `Super` for the Windows key. Modifiers alone give their flags. It returns false and leaves the chord unchanged for an unknown key, a text that ends in `+`, or a null argument; `""` is `ImGuiKey_None`. It needs no Dear ImGui context.

```cpp
char text[64];
facet::ChordToConfigText(bind.primary, text, sizeof(text));   // save
ImGuiKeyChord loaded = ImGuiKey_None;
if (facet::ChordFromText(savedText, &loaded))                  // load
    bind.primary = loaded;
```

The buffer functions always zero-terminate (for a size above 0) and return the full length, which may exceed the buffer.

## Key binds

A `Keybind` holds a primary and an alternate chord and a `BindMode`:

| Mode | `IsBindActive` |
|---|---|
| `Hold` | Either chord is held (`IsChordDown`): the state follows the keys. |
| `Toggle` | A press of either chord flips the caller's latch and returns it. A held key flips once (no repeat); both chords pressed in the same frame flip once. |
| `Always` | True. |
| `Off` | False; the latch is left alone. |

The caller owns the toggle latch, so one bind can drive several states. A latch flips at most once per frame, so asking twice in a frame gives the same answer. With a null latch, Toggle returns whether a chord was pressed this frame. `IsBindPressed` reports a press of either chord in any mode except Off.

While a text field has the keyboard (`io.WantTextInput`), keyboard chords of a bind are ignored, so typing a G never triggers a bind on G. Set `whileTextInput` to keep them working; mouse and gamepad chords work either way.

```cpp
static facet::Keybind menuKey{.primary = ImGuiKey_Insert, .mode = facet::BindMode::Toggle};
static bool menuOpen = false;
facet::IsBindActive(menuKey, &menuOpen);   // once per frame
```

## Shortcut registry

The registry holds named commands with chords, for menus, the command palette and a key binding list. It lives for the whole process: shortcuts can be registered before `facet::Init` and outside frames.

```cpp
const facet::ShortcutId save = facet::RegisterShortcut(
    "file.save", ImGuiMod_Ctrl | ImGuiKey_S, {.label = "Save", .group = "File"});

// Every frame, where the app handles the command:
if (facet::ShortcutPressed(save))
    SaveDocument();
```

**Names and ids.** The name is unique and stable (`"file.save"`); it is the key for saved rebinds. Strings are copied. Registering an existing name replaces that entry in place and keeps its id. `UnregisterShortcut` removes one; `FindShortcut` looks a name up.

**Scopes and routing.** Each call of `ShortcutPressed` requests the routes of the shortcut's chords through `ImGui::Shortcut`, with one owner id per shortcut chord, and Dear ImGui grants each chord to one requester from the next frame on. So a shortcut must be asked for every frame, and one that was not asked for in the previous frame cannot fire.

| Scope | Fires while | Route |
|---|---|---|
| `Global` | always | `ImGuiInputFlags_RouteGlobal` |
| `Window` | the window that calls `ShortcutPressed`, or a child of it, has the focus | `ImGuiInputFlags_RouteFocused` |
| `FocusedItem` | the item submitted right before `ShortcutPressed` has keyboard focus | `RouteFocused`, with `RouteOverActive` while no item is active |

When several scopes want a chord in the same frame, the innermost wins: the focused item, then the focused window (the deepest child first), then global. A `Window` shortcut steps aside for a chord that a `FocusedItem` shortcut in the same window holds while its item has keyboard focus, so the order of the `ShortcutPressed` calls does not matter.

**Active items and text fields.** An active item (a text field being edited, a slider being dragged) keeps the chords it asks Dear ImGui for, against every scope: a `FocusedItem` shortcut gives up `RouteOverActive` while any item is active. While a text field is being edited, chords that type a character (a letter, digit or punctuation key without Ctrl) never fire, and the field's own chords (Ctrl+A, Ctrl+Z and the others it uses) win. Function keys and Ctrl chords the field does not use still fire.

**Disabled.** `SetShortcutEnabled(id, false)` keeps an entry listed but stops it firing. Inside `ImGui::BeginDisabled`, `ShortcutPressed` returns false and requests no route, also on a frame that `TriggerShortcut` asked for, and the call still counts as the shortcut's call for that frame.

**Triggers and callbacks.** `TriggerShortcut(id)` makes `ShortcutPressed` return true in the next frame without a key, as the command palette will do. An optional `callback` with `user` runs on the frame a shortcut fires, once even when `ShortcutPressed` is asked twice. For a `Global` shortcut the app does not ask for, `facet::EndFrame` asks itself and runs the callback; `Window` and `FocusedItem` shortcuts need their `ShortcutPressed` call, which tells Facet where they belong. A callback may register, replace or unregister shortcuts, its own included; every other shortcut that fired in that frame still gets its callback, and one registered during the frame is first asked in the next frame.

**Rebinding.** `SetShortcutChords(id, primary, alternate)` changes the chords at run time; `ImGuiKey_None` clears one. `ResetShortcut` returns to the chords given at registration, which `ShortcutInfo` reports as `defaultPrimary` and `defaultAlternate`.

**Conflicts.** `FindChordConflict(chord, except)` returns another enabled shortcut that uses the chord in a scope overlapping the scope of `except` (without `except`: any scope). Global overlaps everything. Two `Window` or `FocusedItem` shortcuts overlap when they were last asked for in the same window or a window and one of its children, or when either has not been asked for yet; two `FocusedItem` shortcuts on different items never overlap. A sided modifier key equals itself with its flag, but `LeftCtrl` and `RightCtrl` stay different keys. A conflict is not an error: routing still decides which entry gets the chord. The keys demo page shows the message an app can build from it.

**Listing.** `ShortcutCount` and `GetShortcut(index, &info)` list the entries in registration order; `GetShortcut(id, &info)` reads one. The strings in `ShortcutInfo` belong to the registry and stay valid until that entry is unregistered or replaced.

**Limits.** A chord of two or more modifiers without a key cannot be routed by Dear ImGui and never fires as a shortcut (key binds accept it). Calls outside a frame, for disabled or unknown ids, return false.

## Chord capture (for widget authors)

`src/internal/keys.h` has the capture session that the `KeybindCapture` and `KeybindList` widgets are built on; the keys demo page uses it as a prototype.

- `Start(owner, options)` begins listening for the item `owner`, which should be the active item while it listens. Keys that are already down are ignored until released, so the click or Enter that started it is not recorded.
- `Update()` runs once per frame while listening. The chord is reported (`Captured`) when its last key is released: the modifiers held when the main key went down plus that key, the last non-modifier key pressed. Escape cancels on its press, so Escape cannot be captured. Options accept mouse buttons, gamepad buttons, and modifiers alone; without mouse buttons a mouse press cancels.
- `Clear()` and `Cancel()` end the session from the widget.
- Only one session listens at a time. Starting one ends every older session: its next `Update()` returns `Canceled` and records nothing, also when both use the same owner id.

While it listens, every accepted key that is down is locked to the owner with Dear ImGui's key ownership (`SetKeyOwner` with `ImGuiInputFlags_LockUntilRelease`), so `ImGui::Shortcut`, the registry, key binds and the stock `IsKeyPressed` see nothing. When the owner is the active item it also claims every keyboard key (`SetActiveIdUsingAllKeyboardKeys`), which keeps Tab, the arrows and Escape away from keyboard navigation. `facet::NewFrame` repeats the locks for keys pressed since the last frame, before app code runs. Code that queries keys between `ImGui::NewFrame` and `facet::NewFrame` can still see the frame in which a key goes down.
