// Key names, chord text in configuration form and the chord capture session (keys.h).

#include "internal/keys.h"

#include <algorithm>
#include <cstring>

#include <imgui_internal.h>

#include "facet/core/input.h"

namespace facet::internal
{
namespace
{

constexpr int kNamedKeyCount = ImGuiKey_NamedKey_COUNT;

// Display names, one per named key in ImGuiKey order. Left and right modifiers share a name.
constexpr const char* kDisplayNames[] = {
    "Tab", "Left", "Right", "Up", "Down", "Page Up", "Page Down", "Home", "End", "Insert", "Delete",
    "Backspace", "Space", "Enter", "Esc",
    "Ctrl", "Shift", "Alt", "Win", "Ctrl", "Shift", "Alt", "Win", "Menu",
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U",
    "V", "W", "X", "Y", "Z",
    "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12",
    "F13", "F14", "F15", "F16", "F17", "F18", "F19", "F20", "F21", "F22", "F23", "F24",
    "'", ",", "-", ".", "/", ";", "=", "[", "\\", "]", "`",
    "Caps Lock", "Scroll Lock", "Num Lock", "Print Screen", "Pause",
    "Num 0", "Num 1", "Num 2", "Num 3", "Num 4", "Num 5", "Num 6", "Num 7", "Num 8", "Num 9",
    "Num .", "Num /", "Num *", "Num -", "Num +", "Num Enter", "Num =",
    "Back", "Forward", "OEM 102",
    "Gamepad Start", "Gamepad Back", "Gamepad X", "Gamepad B", "Gamepad Y", "Gamepad A",
    "D-pad Left", "D-pad Right", "D-pad Up", "D-pad Down",
    "Gamepad L1", "Gamepad R1", "Gamepad L2", "Gamepad R2", "Gamepad L3", "Gamepad R3",
    "Left Stick Left", "Left Stick Right", "Left Stick Up", "Left Stick Down",
    "Right Stick Left", "Right Stick Right", "Right Stick Up", "Right Stick Down",
    "Mouse Left", "Mouse Right", "Mouse Middle", "Mouse 4", "Mouse 5", "Wheel X", "Wheel Y",
    "Ctrl", "Shift", "Alt", "Win",
};
static_assert(sizeof(kDisplayNames) / sizeof(kDisplayNames[0]) == kNamedKeyCount, "one display name per named key");

// Configuration names: identifiers that never change, with the side of every modifier and
// without punctuation (a ';' would start a comment in many config formats).
constexpr const char* kConfigNames[] = {
    "Tab", "LeftArrow", "RightArrow", "UpArrow", "DownArrow", "PageUp", "PageDown", "Home", "End", "Insert",
    "Delete", "Backspace", "Space", "Enter", "Escape",
    "LeftCtrl", "LeftShift", "LeftAlt", "LeftSuper", "RightCtrl", "RightShift", "RightAlt", "RightSuper", "Menu",
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U",
    "V", "W", "X", "Y", "Z",
    "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12",
    "F13", "F14", "F15", "F16", "F17", "F18", "F19", "F20", "F21", "F22", "F23", "F24",
    "Apostrophe", "Comma", "Minus", "Period", "Slash", "Semicolon", "Equal", "LeftBracket", "Backslash",
    "RightBracket", "GraveAccent",
    "CapsLock", "ScrollLock", "NumLock", "PrintScreen", "Pause",
    "Num0", "Num1", "Num2", "Num3", "Num4", "Num5", "Num6", "Num7", "Num8", "Num9",
    "NumDecimal", "NumDivide", "NumMultiply", "NumSubtract", "NumAdd", "NumEnter", "NumEqual",
    "AppBack", "AppForward", "Oem102",
    "GamepadStart", "GamepadBack", "GamepadFaceLeft", "GamepadFaceRight", "GamepadFaceUp", "GamepadFaceDown",
    "GamepadDpadLeft", "GamepadDpadRight", "GamepadDpadUp", "GamepadDpadDown",
    "GamepadL1", "GamepadR1", "GamepadL2", "GamepadR2", "GamepadL3", "GamepadR3",
    "GamepadLStickLeft", "GamepadLStickRight", "GamepadLStickUp", "GamepadLStickDown",
    "GamepadRStickLeft", "GamepadRStickRight", "GamepadRStickUp", "GamepadRStickDown",
    "MouseLeft", "MouseRight", "MouseMiddle", "MouseX1", "MouseX2", "MouseWheelX", "MouseWheelY",
    "Ctrl", "Shift", "Alt", "Super",
};
static_assert(sizeof(kConfigNames) / sizeof(kConfigNames[0]) == kNamedKeyCount, "one config name per named key");

// Configuration names of the modifier flags, in kChordModifiers order.
constexpr const char* kModifierConfigNames[4] = {"Ctrl", "Shift", "Alt", "Super"};

bool IsNamed(ImGuiKey key)
{
    return key >= ImGuiKey_NamedKey_BEGIN && key < ImGuiKey_NamedKey_END;
}

char Lower(char c)
{
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

bool EqualsNoCase(const char* begin, const char* end, const char* name)
{
    const std::size_t length = static_cast<std::size_t>(end - begin);
    if (std::strlen(name) != length)
        return false;
    for (std::size_t i = 0; i < length; ++i)
    {
        if (Lower(begin[i]) != Lower(name[i]))
            return false;
    }
    return true;
}

// Appends text to a fixed buffer, counting the full length.
struct TextOut
{
    char* buffer;
    int size;
    int length = 0;

    void Append(const char* text)
    {
        for (; *text != '\0'; ++text, ++length)
        {
            if (buffer != nullptr && length < size - 1)
                buffer[length] = *text;
        }
    }
    int Finish()
    {
        if (buffer != nullptr && size > 0)
            buffer[std::min(length, size - 1)] = '\0';
        return length;
    }
};

// --- Chord capture: the session listening now ---------------------------------------------------

struct CaptureState
{
    ImGuiID owner = 0;
    ChordCaptureOptions options;
    int frame = -1;  // ImGui frame count of its last Update
    // Counts Start calls. A session listens only while its own count is the latest, so a
    // newer Start ends every older session, even one with the same owner id.
    std::uint32_t generation = 0;
};

CaptureState& Capture()
{
    static CaptureState state;
    return state;
}

bool Accepts(const ChordCaptureOptions& options, ImGuiKey key)
{
    if (key >= ImGuiKey_Keyboard_BEGIN && key < ImGuiKey_Keyboard_END)
        return true;
    if (key >= ImGuiKey_MouseLeft && key <= ImGuiKey_MouseX2)
        return options.allowMouse;
    if (key >= ImGuiKey_Gamepad_BEGIN && key < ImGuiKey_Gamepad_END)
        return options.allowGamepad;
    return false;
}

// Locks every accepted key that is down (and the modifier flags) to the owner.
void LockHeldKeys(ImGuiID owner, const ChordCaptureOptions& options)
{
    ImGuiContext& g = *GImGui;
    if (g.ActiveId == owner)
        ImGui::SetActiveIdUsingAllKeyboardKeys();
    for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; ++k)
    {
        const ImGuiKey key = static_cast<ImGuiKey>(k);
        if (!Accepts(options, key) && !(key >= ImGuiKey_ReservedForModCtrl && key <= ImGuiKey_ReservedForModSuper))
            continue;
        if (ImGui::GetKeyData(key)->Down)
            ImGui::SetKeyOwner(key, owner, ImGuiInputFlags_LockUntilRelease);
    }
}

ImGuiID DefaultOwner()
{
    return ImHashStr("##facet.chord_capture");
}

} // namespace

bool IsSidedModifierKey(ImGuiKey key)
{
    return key >= ImGuiKey_LeftCtrl && key <= ImGuiKey_RightSuper;
}

ImGuiKeyChord ModifierFlagOf(ImGuiKey key)
{
    switch (key)
    {
    case ImGuiKey_LeftCtrl:
    case ImGuiKey_RightCtrl:
    case ImGuiKey_ReservedForModCtrl:
    case ImGuiMod_Ctrl:
        return ImGuiMod_Ctrl;
    case ImGuiKey_LeftShift:
    case ImGuiKey_RightShift:
    case ImGuiKey_ReservedForModShift:
    case ImGuiMod_Shift:
        return ImGuiMod_Shift;
    case ImGuiKey_LeftAlt:
    case ImGuiKey_RightAlt:
    case ImGuiKey_ReservedForModAlt:
    case ImGuiMod_Alt:
        return ImGuiMod_Alt;
    case ImGuiKey_LeftSuper:
    case ImGuiKey_RightSuper:
    case ImGuiKey_ReservedForModSuper:
    case ImGuiMod_Super:
        return ImGuiMod_Super;
    default:
        return 0;
    }
}

ImGuiKeyChord NormalizeChord(ImGuiKeyChord chord)
{
    const ImGuiKey key = ChordKey(chord);
    return IsSidedModifierKey(key) ? (chord | ModifierFlagOf(key)) : chord;
}

ImGuiKeyChord RequiredModifiers(ImGuiKeyChord chord)
{
    return ChordMods(NormalizeChord(chord));
}

const char* KeyDisplayName(ImGuiKey key)
{
    if (!IsNamed(key))
        return "";
    return kDisplayNames[key - ImGuiKey_NamedKey_BEGIN];
}

const char* KeyConfigName(ImGuiKey key)
{
    if (!IsNamed(key))
        return "";
    return kConfigNames[key - ImGuiKey_NamedKey_BEGIN];
}

} // namespace facet::internal

namespace facet
{

int ChordToConfigText(ImGuiKeyChord chord, char* buffer, int bufferSize)
{
    using namespace facet::internal;
    TextOut out{buffer, bufferSize};
    const ImGuiKey key = ChordKey(chord);
    // The flag of a sided modifier key is implied by the key's own name.
    const ImGuiKeyChord mods = ChordMods(chord) & ~ModifierFlagOf(key);
    if (key == ImGuiKey_None || IsNamed(key))
    {
        bool first = true;
        for (int i = 0; i < 4; ++i)
        {
            if ((mods & kChordModifiers[i]) == 0)
                continue;
            if (!first)
                out.Append("+");
            out.Append(kModifierConfigNames[i]);
            first = false;
        }
        if (key != ImGuiKey_None)
        {
            if (!first)
                out.Append("+");
            out.Append(KeyConfigName(key));
        }
    }
    return out.Finish();
}

} // namespace facet

namespace facet::internal
{

ImGuiKey FindKeyByName(const char* begin, const char* end)
{
    if (begin == nullptr || end <= begin)
        return ImGuiKey_None;
    for (int i = 0; i < kNamedKeyCount; ++i)
    {
        const ImGuiKey key = static_cast<ImGuiKey>(ImGuiKey_NamedKey_BEGIN + i);
        if (ModifierFlagOf(key) != 0 && !IsSidedModifierKey(key))
            continue;  // the flag names belong to the modifiers
        if (IsSidedModifierKey(key))
        {
            // Only the configuration names say which side; "Ctrl" is the modifier.
            if (EqualsNoCase(begin, end, kConfigNames[i]))
                return key;
            continue;
        }
        if (EqualsNoCase(begin, end, kDisplayNames[i]) || EqualsNoCase(begin, end, kConfigNames[i]))
            return key;
    }
    return ImGuiKey_None;
}

// --- ChordCapture ------------------------------------------------------------------------------

void ChordCapture::Start(ImGuiID ownerId, const ChordCaptureOptions& captureOptions)
{
    owner = ownerId != 0 ? ownerId : DefaultOwner();
    options = captureOptions;
    listening = true;
    pendingEnd = ChordCaptureEvent::None;
    held = 0;
    result = 0;
    mainKey = ImGuiKey_None;
    mainMods = 0;
    peakMods = 0;
    gesture = false;
    std::fill(std::begin(ignored), std::end(ignored), 0ull);
    CaptureState& state = Capture();
    generation = ++state.generation;
    state.owner = owner;
    state.options = options;
    if (GImGui == nullptr)
        return;
    for (int i = 0; i < kNamedKeyCount; ++i)
    {
        if (ImGui::GetKeyData(static_cast<ImGuiKey>(ImGuiKey_NamedKey_BEGIN + i))->Down)
            ignored[i / 64] |= 1ull << (i % 64);
    }
    state.frame = GImGui->FrameCount;
}

void ChordCapture::Finish(ChordCaptureEvent event)
{
    listening = false;
    last = event;
    held = 0;
    gesture = false;
    CaptureState& state = Capture();
    if (state.generation == generation)
        state.owner = 0;
}

void ChordCapture::Cancel()
{
    if (listening)
        Finish(ChordCaptureEvent::Canceled);
    pendingEnd = ChordCaptureEvent::Canceled;
    last = ChordCaptureEvent::Canceled;
}

void ChordCapture::Clear()
{
    if (listening)
        Finish(ChordCaptureEvent::Cleared);
    result = 0;
    pendingEnd = ChordCaptureEvent::Cleared;
    last = ChordCaptureEvent::Cleared;
}

ChordCaptureEvent ChordCapture::Update()
{
    if (pendingEnd != ChordCaptureEvent::None)
    {
        const ChordCaptureEvent event = pendingEnd;
        pendingEnd = ChordCaptureEvent::None;
        return event;
    }
    if (!listening || GImGui == nullptr)
        return ChordCaptureEvent::None;

    CaptureState& state = Capture();
    if (state.generation != generation)
    {
        // A newer session started: this one ends without recording anything.
        Finish(ChordCaptureEvent::Canceled);
        return ChordCaptureEvent::Canceled;
    }
    state.owner = owner;
    state.options = options;
    state.frame = GImGui->FrameCount;
    LockHeldKeys(owner, options);

    const auto isIgnored = [&](int i) { return (ignored[i / 64] >> (i % 64)) & 1ull; };
    ImGuiKeyChord mods = 0;
    bool anyDown = false;
    for (int i = 0; i < kNamedKeyCount; ++i)
    {
        const ImGuiKey key = static_cast<ImGuiKey>(ImGuiKey_NamedKey_BEGIN + i);
        const ImGuiKeyData* data = ImGui::GetKeyData(key);
        if (isIgnored(i))
        {
            if (!data->Down)
                ignored[i / 64] &= ~(1ull << (i % 64));
            continue;
        }
        const bool pressed = data->Down && data->DownDuration == 0.0f;
        const bool isMouseButton = key >= ImGuiKey_MouseLeft && key <= ImGuiKey_MouseX2;
        if (key == ImGuiKey_Escape && pressed)
        {
            Finish(ChordCaptureEvent::Canceled);
            return ChordCaptureEvent::Canceled;
        }
        if (isMouseButton && !options.allowMouse && pressed)
        {
            Finish(ChordCaptureEvent::Canceled);
            return ChordCaptureEvent::Canceled;
        }
        // The modifier state comes from the four modifier entries (ImGuiMod_Ctrl ...), which
        // every backend feeds; the left and right keys only count as held.
        const bool modifierEntry = key >= ImGuiKey_ReservedForModCtrl && key <= ImGuiKey_ReservedForModSuper;
        if ((!Accepts(options, key) && !modifierEntry) || !data->Down)
            continue;
        anyDown = true;
        if (modifierEntry)
        {
            mods |= ModifierFlagOf(key);
            gesture = gesture || pressed;
            continue;
        }
        if (IsSidedModifierKey(key))
            continue;
        if (pressed)
        {
            mainKey = key;
            gesture = true;
        }
    }
    // The modifiers of the main key are those held when it went down (read after the loop, so
    // a modifier pressed in the same frame counts).
    const bool mainDown = mainKey != ImGuiKey_None && ImGui::GetKeyData(mainKey)->Down;
    if (mainDown && ImGui::GetKeyData(mainKey)->DownDuration == 0.0f)
        mainMods = mods;
    peakMods |= mods;
    held = mainDown ? (mainKey | mods) : mods;

    if (anyDown || !gesture)
        return ChordCaptureEvent::None;
    if (mainKey != ImGuiKey_None)
    {
        result = mainKey | mainMods;
        Finish(ChordCaptureEvent::Captured);
        return ChordCaptureEvent::Captured;
    }
    if (options.allowModifierOnly && peakMods != 0)
    {
        result = peakMods;
        Finish(ChordCaptureEvent::Captured);
        return ChordCaptureEvent::Captured;
    }
    // Modifiers alone are not a chord here: listen on.
    gesture = false;
    peakMods = 0;
    return ChordCaptureEvent::None;
}

ImGuiID ActiveChordCaptureOwner()
{
    return Capture().owner;
}

void ChordCaptureNewFrame()
{
    CaptureState& state = Capture();
    if (state.owner == 0 || GImGui == nullptr)
        return;
    if (state.frame != GImGui->FrameCount - 1)
    {
        state.owner = 0;  // the session was not updated in the last frame
        return;
    }
    LockHeldKeys(state.owner, state.options);
}

} // namespace facet::internal
