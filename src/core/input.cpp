// Key chords, key binds and the shortcut registry (input.h).
//
// Stub: every function returns its neutral result. Owner from Phase 1c part A on: the data
// blocks package (keyboard and chord utilities, shortcut registry), which replaces these
// bodies and fills the subsystem hooks below.

#include "facet/core/input.h"

#include "internal/subsystem.h"

namespace facet
{

bool IsChordDown(ImGuiKeyChord)
{
    return false;
}

bool IsChordPressed(ImGuiKeyChord, bool)
{
    return false;
}

const char* KeyNameText(ImGuiKey)
{
    return "";
}

int ChordToText(ImGuiKeyChord, char* buffer, int bufferSize)
{
    if (buffer != nullptr && bufferSize > 0)
        buffer[0] = '\0';
    return 0;
}

bool ChordFromText(const char*, ImGuiKeyChord*)
{
    return false;
}

bool IsBindActive(const Keybind&, bool*)
{
    return false;
}

bool IsBindPressed(const Keybind&)
{
    return false;
}

ShortcutId RegisterShortcut(const char*, ImGuiKeyChord, const ShortcutOptions&)
{
    return {};
}

void UnregisterShortcut(ShortcutId) {}

ShortcutId FindShortcut(const char*)
{
    return {};
}

void SetShortcutEnabled(ShortcutId, bool) {}

void SetShortcutChords(ShortcutId, ImGuiKeyChord, ImGuiKeyChord) {}

void ResetShortcut(ShortcutId) {}

bool ShortcutPressed(ShortcutId)
{
    return false;
}

void TriggerShortcut(ShortcutId) {}

int ShortcutCount()
{
    return 0;
}

bool GetShortcut(int, ShortcutInfo*)
{
    return false;
}

bool GetShortcut(ShortcutId, ShortcutInfo*)
{
    return false;
}

ShortcutId FindChordConflict(ImGuiKeyChord, ShortcutId)
{
    return {};
}

namespace internal
{

// Per-frame work of the key and shortcut module: chord state and shortcut triggers. No hooks
// yet (stub).
Subsystem InputSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "input";
    return subsystem;
}

} // namespace internal

} // namespace facet
