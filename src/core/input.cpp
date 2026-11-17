// Key chords, key binds and the shortcut registry (input.h).
//
// Chord queries read Dear ImGui's key state with ImGuiKeyOwner_Any, the same view a stock
// IsKeyDown has: keys owned by a widget still count, keys locked by a chord capture
// (internal/keys.h) do not. Shortcuts go through ImGui::Shortcut with one owner id per
// shortcut chord, so Dear ImGui's routing decides which scope gets a chord.

#include "facet/core/input.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <vector>

#include <imgui_internal.h>

#include "internal/keys.h"
#include "internal/subsystem.h"

namespace facet
{
namespace
{

using internal::IsSidedModifierKey;
using internal::kChordModifiers;
using internal::ModifierFlagOf;
using internal::NormalizeChord;
using internal::RequiredModifiers;

constexpr const char* kModifierDisplayNames[4] = {"Ctrl", "Shift", "Alt", "Win"};

bool InImGuiFrame()
{
    const ImGuiContext* g = GImGui;
    return g != nullptr && g->WithinFrameScope;
}

int ModifierCount(ImGuiKeyChord mods)
{
    int count = 0;
    for (const ImGuiKeyChord mod : kChordModifiers)
        count += (mods & mod) != 0 ? 1 : 0;
    return count;
}

bool IsKeyboardChord(ImGuiKeyChord chord)
{
    const ImGuiKey key = ChordKey(chord);
    return key == ImGuiKey_None || (key >= ImGuiKey_Keyboard_BEGIN && key < ImGuiKey_Keyboard_END);
}

// --- Key binds: latches flipped in the current frame ------------------------------------------

struct FlipLog
{
    static constexpr int kCapacity = 64;
    int frame = -1;
    int count = 0;
    const bool* latches[kCapacity] = {};
};

FlipLog& Flips()
{
    static FlipLog log;
    return log;
}

// True when `latch` may flip in this frame; records it.
bool TakeFlip(const bool* latch)
{
    FlipLog& log = Flips();
    const int frame = GImGui->FrameCount;
    if (log.frame != frame)
    {
        log.frame = frame;
        log.count = 0;
    }
    for (int i = 0; i < log.count; ++i)
    {
        if (log.latches[i] == latch)
            return false;
    }
    if (log.count < FlipLog::kCapacity)
        log.latches[log.count++] = latch;
    return true;
}

bool BindChordBlocked(const Keybind& bind, ImGuiKeyChord chord)
{
    return !bind.whileTextInput && GImGui->IO.WantTextInput && IsKeyboardChord(chord);
}

bool BindChordDown(const Keybind& bind, ImGuiKeyChord chord)
{
    return chord != ImGuiKey_None && !BindChordBlocked(bind, chord) && IsChordDown(chord);
}

bool BindChordPressed(const Keybind& bind, ImGuiKeyChord chord)
{
    return chord != ImGuiKey_None && !BindChordBlocked(bind, chord) && IsChordPressed(chord, false);
}

// --- Shortcut registry --------------------------------------------------------------------------

struct Entry
{
    ShortcutId id;
    std::string name;
    std::string label;
    std::string group;
    ImGuiKeyChord chords[2] = {ImGuiKey_None, ImGuiKey_None};
    ImGuiKeyChord defaults[2] = {ImGuiKey_None, ImGuiKey_None};
    ImGuiID routeOwners[2] = {0, 0};  // owner ids of the two chords in Dear ImGui's routing
    ShortcutScope scope = ShortcutScope::Global;
    bool repeat = false;
    bool enabled = true;
    void (*callback)(ShortcutId, void*) = nullptr;
    void* user = nullptr;

    bool pendingTrigger = false;  // TriggerShortcut: fire in the next frame
    bool triggered = false;       // fires in this frame without a key
    int queryFrame = -1;          // ImGui frame of the last ShortcutPressed
    ImGuiID windowId = 0;         // window of the last ShortcutPressed
    ImGuiID itemId = 0;           // item before the last ShortcutPressed (FocusedItem)
    bool itemFocused = false;     // that item had keyboard focus then (FocusedItem)
    int createdFrame = -2;        // ImGui frame in which the entry was registered
};

struct Registry
{
    // Entries live on the heap so the strings handed out by GetShortcut keep their address
    // when other entries come and go.
    std::vector<std::unique_ptr<Entry>> entries;
    std::uint32_t nextId = 1;
    bool inFrame = false;  // between the input module's NewFrame and EndFrame hooks
};

Registry& Shortcuts()
{
    static Registry registry;
    return registry;
}

Entry* FindEntry(ShortcutId id)
{
    if (!id.IsValid())
        return nullptr;
    for (const std::unique_ptr<Entry>& entry : Shortcuts().entries)
    {
        if (entry->id == id)
            return entry.get();
    }
    return nullptr;
}

Entry* FindEntryByName(const char* name)
{
    for (const std::unique_ptr<Entry>& entry : Shortcuts().entries)
    {
        if (entry->name == name)
            return entry.get();
    }
    return nullptr;
}

ImGuiID RouteOwner(const char* name, int slot)
{
    // A seed per slot keeps the primary and the alternate chord apart in the routing table.
    ImGuiID owner = ImHashStr(name, 0, 0x9E3779B9u + static_cast<ImGuiID>(slot));
    if (owner == 0 || owner == ImGuiKeyOwner_NoOwner)
        owner = 1u + static_cast<ImGuiID>(slot);
    return owner;
}

// Dear ImGui routes a key with any modifiers, or one modifier alone.
bool IsRoutable(ImGuiKeyChord chord)
{
    const ImGuiKey key = ChordKey(chord);
    if (key == ImGuiKey_None)
        return ModifierCount(ChordMods(chord)) == 1;
    return key >= ImGuiKey_NamedKey_BEGIN && key < ImGuiKey_NamedKey_END;
}

bool SameChord(const Entry& entry, ImGuiKeyChord chord)
{
    const ImGuiKeyChord wanted = NormalizeChord(chord);
    return (entry.chords[0] != ImGuiKey_None && NormalizeChord(entry.chords[0]) == wanted) ||
           (entry.chords[1] != ImGuiKey_None && NormalizeChord(entry.chords[1]) == wanted);
}

// A FocusedItem shortcut in the window of `window` has `chord` and its item had keyboard
// focus when it was last asked, in this frame or the one before. The window's shortcut then
// steps aside for that chord: both would score the same in Dear ImGui's routing while an item
// is active, and the inner scope must win whatever order they are asked in.
bool FocusedItemTakes(const Entry& window, ImGuiKeyChord chord)
{
    const int frame = GImGui->FrameCount;
    for (const std::unique_ptr<Entry>& other : Shortcuts().entries)
    {
        if (other.get() != &window && other->enabled && other->scope == ShortcutScope::FocusedItem &&
            other->itemFocused && other->queryFrame >= frame - 1 && other->windowId == window.windowId &&
            SameChord(*other, chord))
            return true;
    }
    return false;
}

// Requests the routes of the entry's chords for this frame and reports whether one fired.
bool PollEntry(Entry& entry)
{
    ImGuiContext& g = *GImGui;
    entry.queryFrame = g.FrameCount;
    entry.windowId = g.CurrentWindow != nullptr ? g.CurrentWindow->ID : 0;
    if (entry.scope == ShortcutScope::FocusedItem)
    {
        entry.itemId = g.LastItemData.ID;
        entry.itemFocused = entry.itemId != 0 && g.NavId == entry.itemId && g.NavWindow == g.CurrentWindow;
    }
    // Inside a disabled block nothing fires, a trigger neither, and no route is requested
    // (as ImGui::Shortcut does). The entry still counts as asked for this frame.
    if ((g.CurrentItemFlags & ImGuiItemFlags_Disabled) != 0)
    {
        entry.itemFocused = false;
        return false;
    }

    bool fired = entry.triggered;
    bool route = true;
    ImGuiInputFlags flags = entry.repeat ? ImGuiInputFlags_Repeat : ImGuiInputFlags_None;
    switch (entry.scope)
    {
    case ShortcutScope::Global:
        flags |= ImGuiInputFlags_RouteGlobal;
        break;
    case ShortcutScope::Window:
        flags |= ImGuiInputFlags_RouteFocused;
        break;
    case ShortcutScope::FocusedItem:
        // Only while the item has keyboard focus. With no active item, RouteOverActive ranks
        // it above the window's own shortcuts. While an item is active (a text field being
        // edited, a slider being dragged) it is left out: the active item keeps its own chords
        // (Ctrl+A, Ctrl+Z) and an item that takes every key (a chord capture) keeps them all.
        flags |= ImGuiInputFlags_RouteFocused;
        if (g.ActiveId == 0)
            flags |= ImGuiInputFlags_RouteOverActive;
        route = entry.itemFocused;
        break;
    }
    if (route)
    {
        for (int slot = 0; slot < 2; ++slot)
        {
            const ImGuiKeyChord chord = entry.chords[slot];
            if (chord == ImGuiKey_None || !IsRoutable(chord))
                continue;
            if (entry.scope == ShortcutScope::Window && FocusedItemTakes(entry, chord))
                continue;
            // Both chords request their route every frame, so neither may short-circuit.
            if (ImGui::Shortcut(chord, flags, entry.routeOwners[slot]))
                fired = true;
        }
    }
    return fired;
}

bool WindowContains(ImGuiID outer, ImGuiID inner)
{
    ImGuiWindow* window = ImGui::FindWindowByID(inner);
    for (; window != nullptr; window = window->ParentWindow)
    {
        if (window->ID == outer)
            return true;
    }
    return false;
}

bool WindowsNested(ImGuiID a, ImGuiID b)
{
    if (a == b)
        return true;
    if (GImGui == nullptr || ImGui::FindWindowByID(a) == nullptr || ImGui::FindWindowByID(b) == nullptr)
        return true;  // a window that no longer exists may come back anywhere
    return WindowContains(a, b) || WindowContains(b, a);
}

bool ScopesOverlap(const Entry* reference, const Entry& other)
{
    if (reference == nullptr || reference->scope == ShortcutScope::Global || other.scope == ShortcutScope::Global)
        return true;
    if (reference->windowId == 0 || other.windowId == 0)
        return true;
    if (reference->scope == ShortcutScope::FocusedItem && other.scope == ShortcutScope::FocusedItem)
        return reference->itemId == other.itemId || reference->itemId == 0 || other.itemId == 0;
    return WindowsNested(reference->windowId, other.windowId);
}

void FillInfo(const Entry& entry, ShortcutInfo* info)
{
    info->id = entry.id;
    info->name = entry.name.c_str();
    info->label = entry.label.c_str();
    info->group = entry.group.c_str();
    info->primary = entry.chords[0];
    info->alternate = entry.chords[1];
    info->defaultPrimary = entry.defaults[0];
    info->defaultAlternate = entry.defaults[1];
    info->scope = entry.scope;
    info->enabled = entry.enabled;
}

// --- Text ----------------------------------------------------------------------------------------

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

char Lower(char c)
{
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

bool IsSpace(char c)
{
    return c == ' ' || c == '\t';
}

// The modifier word at `p` ("ctrl", "control", "shift", "alt", "win", "super"); returns its
// flag and the length of the word, or 0.
ImGuiKeyChord ModifierWordAt(const char* p, int* length)
{
    struct Word
    {
        const char* text;
        ImGuiKeyChord flag;
    };
    static constexpr Word kWords[] = {{"control", ImGuiMod_Ctrl}, {"ctrl", ImGuiMod_Ctrl}, {"shift", ImGuiMod_Shift},
                                      {"alt", ImGuiMod_Alt},       {"super", ImGuiMod_Super}, {"win", ImGuiMod_Super}};
    for (const Word& word : kWords)
    {
        const int n = static_cast<int>(std::strlen(word.text));
        int i = 0;
        while (i < n && p[i] != '\0' && Lower(p[i]) == word.text[i])
            ++i;
        if (i != n)
            continue;
        // The word must end here: at a "+", the end, or spaces before either.
        const char* after = p + n;
        while (IsSpace(*after))
            ++after;
        if (*after == '+' || *after == '\0')
        {
            *length = n;
            return word.flag;
        }
    }
    *length = 0;
    return 0;
}

} // namespace

// --- Chords ----------------------------------------------------------------------------------------

bool IsChordDown(ImGuiKeyChord chord)
{
    if (!InImGuiFrame())
        return false;
    const ImGuiKey key = ChordKey(chord);
    const ImGuiKeyChord mods = RequiredModifiers(chord);
    if (key == ImGuiKey_None && mods == 0)
        return false;
    if (key != ImGuiKey_None)
    {
        if (!ImGui::IsNamedKey(key) || !ImGui::IsKeyDown(key, ImGuiKeyOwner_Any))
            return false;
    }
    for (const ImGuiKeyChord mod : kChordModifiers)
    {
        if ((mods & mod) != 0 && !ImGui::IsKeyDown(static_cast<ImGuiKey>(mod), ImGuiKeyOwner_Any))
            return false;
    }
    return true;
}

bool IsChordPressed(ImGuiKeyChord chord, bool repeat)
{
    if (!InImGuiFrame())
        return false;
    const ImGuiKey key = ChordKey(chord);
    const ImGuiKeyChord mods = RequiredModifiers(chord);
    if (key == ImGuiKey_None && mods == 0)
        return false;
    if (key != ImGuiKey_None && !ImGui::IsNamedKey(key))
        return false;
    if (GImGui->IO.KeyMods != mods)
        return false;
    const ImGuiInputFlags flags = repeat ? ImGuiInputFlags_Repeat : ImGuiInputFlags_None;
    if (key != ImGuiKey_None)
        return ImGui::IsKeyPressed(key, flags, ImGuiKeyOwner_Any);
    // Modifiers alone: the set is complete in the frame its last modifier went down.
    for (const ImGuiKeyChord mod : kChordModifiers)
    {
        if ((mods & mod) != 0 && ImGui::IsKeyPressed(static_cast<ImGuiKey>(mod), flags, ImGuiKeyOwner_Any))
            return true;
    }
    return false;
}

const char* KeyNameText(ImGuiKey key)
{
    for (int i = 0; i < 4; ++i)
    {
        if (key == static_cast<ImGuiKey>(kChordModifiers[i]))
            return kModifierDisplayNames[i];
    }
    return internal::KeyDisplayName(key);
}

int ChordToText(ImGuiKeyChord chord, char* buffer, int bufferSize)
{
    TextOut out{buffer, bufferSize};
    const ImGuiKey key = ChordKey(chord);
    const ImGuiKeyChord mods = RequiredModifiers(chord);
    bool first = true;
    for (int i = 0; i < 4; ++i)
    {
        if ((mods & kChordModifiers[i]) == 0)
            continue;
        if (!first)
            out.Append("+");
        out.Append(kModifierDisplayNames[i]);
        first = false;
    }
    if (key != ImGuiKey_None && !IsSidedModifierKey(key))
    {
        const char* name = KeyNameText(key);
        if (name[0] != '\0')
        {
            if (!first)
                out.Append("+");
            out.Append(name);
        }
    }
    return out.Finish();
}

bool ChordFromText(const char* text, ImGuiKeyChord* chord)
{
    if (text == nullptr || chord == nullptr)
        return false;
    const char* p = text;
    while (IsSpace(*p))
        ++p;
    if (*p == '\0')
    {
        *chord = ImGuiKey_None;
        return true;
    }
    ImGuiKeyChord mods = 0;
    for (;;)
    {
        int length = 0;
        const ImGuiKeyChord flag = ModifierWordAt(p, &length);
        if (flag == 0)
            break;
        mods |= flag;
        p += length;
        while (IsSpace(*p))
            ++p;
        if (*p == '\0')
        {
            *chord = mods;  // modifiers alone
            return true;
        }
        ++p;  // the '+'
        while (IsSpace(*p))
            ++p;
        if (*p == '\0')
            return false;  // ends in "+"
    }
    const char* end = p + std::strlen(p);
    while (end > p && IsSpace(end[-1]))
        --end;
    const ImGuiKey key = internal::FindKeyByName(p, end);
    if (key == ImGuiKey_None)
        return false;
    *chord = NormalizeChord(key | mods);
    return true;
}

// --- Key binds ---------------------------------------------------------------------------------------

bool IsBindActive(const Keybind& bind, bool* toggleState)
{
    if (!InImGuiFrame())
        return false;
    switch (bind.mode)
    {
    case BindMode::Off:
        return false;
    case BindMode::Always:
        return true;
    case BindMode::Hold:
        return BindChordDown(bind, bind.primary) || BindChordDown(bind, bind.alternate);
    case BindMode::Toggle:
    {
        // Both chords are read, so a press of either counts even when both go down together.
        const bool primary = BindChordPressed(bind, bind.primary);
        const bool alternate = BindChordPressed(bind, bind.alternate);
        const bool pressed = primary || alternate;
        if (toggleState == nullptr)
            return pressed;
        if (pressed && TakeFlip(toggleState))
            *toggleState = !*toggleState;
        return *toggleState;
    }
    }
    return false;
}

bool IsBindPressed(const Keybind& bind)
{
    if (!InImGuiFrame() || bind.mode == BindMode::Off)
        return false;
    const bool primary = BindChordPressed(bind, bind.primary);
    const bool alternate = BindChordPressed(bind, bind.alternate);
    return primary || alternate;
}

// --- Shortcut registry -------------------------------------------------------------------------------

ShortcutId RegisterShortcut(const char* name, ImGuiKeyChord chord, const ShortcutOptions& options)
{
    if (name == nullptr || name[0] == '\0')
        return {};
    Registry& registry = Shortcuts();
    try
    {
        Entry* existing = FindEntryByName(name);
        // Built completely before anything changes, so a failed allocation leaves the
        // registry as it was.
        Entry entry = existing != nullptr ? *existing : Entry{};
        entry.name = name;
        entry.label = options.label != nullptr ? options.label : name;
        entry.group = options.group != nullptr ? options.group : "";
        entry.chords[0] = entry.defaults[0] = chord;
        entry.chords[1] = entry.defaults[1] = options.alternate;
        entry.routeOwners[0] = RouteOwner(name, 0);
        entry.routeOwners[1] = RouteOwner(name, 1);
        entry.scope = options.scope;
        entry.repeat = options.repeat;
        entry.enabled = options.enabled;
        entry.callback = options.callback;
        entry.user = options.user;
        entry.pendingTrigger = false;
        entry.triggered = false;
        if (existing != nullptr)
        {
            *existing = std::move(entry);
            return existing->id;
        }
        entry.id = ShortcutId{registry.nextId};
        entry.createdFrame = GImGui != nullptr ? GImGui->FrameCount : -2;
        auto created = std::make_unique<Entry>(std::move(entry));
        registry.entries.push_back(std::move(created));
        ++registry.nextId;
        if (registry.nextId == 0)
            registry.nextId = 1;
        return registry.entries.back()->id;
    }
    catch (const std::bad_alloc&)
    {
        return {};
    }
}

void UnregisterShortcut(ShortcutId id)
{
    std::vector<std::unique_ptr<Entry>>& entries = Shortcuts().entries;
    for (auto it = entries.begin(); it != entries.end(); ++it)
    {
        if ((*it)->id == id)
        {
            entries.erase(it);
            return;
        }
    }
}

ShortcutId FindShortcut(const char* name)
{
    if (name == nullptr)
        return {};
    const Entry* entry = FindEntryByName(name);
    return entry != nullptr ? entry->id : ShortcutId{};
}

void SetShortcutEnabled(ShortcutId id, bool enabled)
{
    if (Entry* entry = FindEntry(id))
    {
        entry->enabled = enabled;
        if (!enabled)
            entry->pendingTrigger = false;
    }
}

void SetShortcutChords(ShortcutId id, ImGuiKeyChord primary, ImGuiKeyChord alternate)
{
    if (Entry* entry = FindEntry(id))
    {
        entry->chords[0] = primary;
        entry->chords[1] = alternate;
    }
}

void ResetShortcut(ShortcutId id)
{
    if (Entry* entry = FindEntry(id))
    {
        entry->chords[0] = entry->defaults[0];
        entry->chords[1] = entry->defaults[1];
    }
}

bool ShortcutPressed(ShortcutId id)
{
    if (!InImGuiFrame() || !Shortcuts().inFrame)
        return false;
    Entry* entry = FindEntry(id);
    if (entry == nullptr || !entry->enabled)
        return false;
    const bool alreadyAsked = entry->queryFrame == GImGui->FrameCount;
    const bool fired = PollEntry(*entry);
    if (fired && !alreadyAsked && entry->callback != nullptr)
    {
        // The callback may change the registry, so nothing of the entry is used after it.
        const auto callback = entry->callback;
        void* user = entry->user;
        callback(id, user);
    }
    return fired;
}

void TriggerShortcut(ShortcutId id)
{
    Entry* entry = FindEntry(id);
    if (entry != nullptr && entry->enabled)
        entry->pendingTrigger = true;
}

int ShortcutCount()
{
    return static_cast<int>(Shortcuts().entries.size());
}

bool GetShortcut(int index, ShortcutInfo* info)
{
    const std::vector<std::unique_ptr<Entry>>& entries = Shortcuts().entries;
    if (info == nullptr || index < 0 || index >= static_cast<int>(entries.size()))
        return false;
    FillInfo(*entries[static_cast<std::size_t>(index)], info);
    return true;
}

bool GetShortcut(ShortcutId id, ShortcutInfo* info)
{
    const Entry* entry = FindEntry(id);
    if (info == nullptr || entry == nullptr)
        return false;
    FillInfo(*entry, info);
    return true;
}

ShortcutId FindChordConflict(ImGuiKeyChord chord, ShortcutId except)
{
    if (chord == ImGuiKey_None)
        return {};
    const Entry* reference = FindEntry(except);
    for (const std::unique_ptr<Entry>& entry : Shortcuts().entries)
    {
        if (entry->id == except || !entry->enabled)
            continue;
        if (SameChord(*entry, chord) && ScopesOverlap(reference, *entry))
            return entry->id;
    }
    return {};
}

namespace internal
{
namespace
{

void InputNewFrame()
{
    Registry& registry = Shortcuts();
    registry.inFrame = true;
    for (const std::unique_ptr<Entry>& entry : registry.entries)
    {
        entry->triggered = entry->pendingTrigger && entry->enabled;
        entry->pendingTrigger = false;
    }
    ChordCaptureNewFrame();
}

void InputEndFrame()
{
    Registry& registry = Shortcuts();
    if (GImGui != nullptr && GImGui->WithinFrameScope)
    {
        // Global shortcuts with a callback fire without a ShortcutPressed call of the app.
        // A callback may add or remove entries, which moves the others in the vector, so the
        // scan starts over after each callback. PollEntry marks an entry as asked for this
        // frame, so no entry is polled twice and the scan ends. Entries registered in this
        // frame are left out: their routes are granted from the next frame anyway, and a name
        // unregistered and registered again by a callback must not fire twice.
        const int frame = GImGui->FrameCount;
        for (std::size_t i = 0; i < registry.entries.size(); ++i)
        {
            Entry& entry = *registry.entries[i];
            if (entry.callback == nullptr || !entry.enabled || entry.scope != ShortcutScope::Global ||
                entry.queryFrame == frame || entry.createdFrame == frame)
                continue;
            if (PollEntry(entry))
            {
                const auto callback = entry.callback;
                void* user = entry.user;
                callback(entry.id, user);
                i = static_cast<std::size_t>(-1);  // ++i makes it 0
            }
        }
    }
    for (const std::unique_ptr<Entry>& entry : registry.entries)
        entry->triggered = false;
    registry.inFrame = false;
}

void InputShutdown()
{
    Shortcuts().inFrame = false;
}

} // namespace

// Per-frame work of the key and shortcut module: shortcut triggers, the Global callbacks and
// the key locks of a chord capture.
Subsystem InputSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "input";
    subsystem.newFrame = &InputNewFrame;
    subsystem.endFrame = &InputEndFrame;
    subsystem.shutdown = &InputShutdown;
    return subsystem;
}

} // namespace internal

} // namespace facet
