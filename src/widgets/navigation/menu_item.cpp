// MenuItem, MenuCheckItem and MenuRadioItem (navigation.h): rows of the menu level being submitted
// (a menu, a context menu, a popover), built on the menu row block (popup_menu.h).
//
// The shortcut at the end is the registered chord of `command` (core/input.h) when it is set,
// else `shortcut`; a command whose shortcut is disabled draws its row disabled, because the action
// it would trigger cannot run. Activation (a click, Enter or Space) closes the menu's whole chain
// and, with a command, triggers it, so the command's ShortcutPressed handler runs the action in
// the next frame exactly as when the chord is pressed with the menu closed.
//
// Outside any popup level a row still draws and activates in the current window, so a menu's rows
// can also be laid out inline (the activation then closes nothing).

#include "facet/widgets/navigation.h"

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/input.h"
#include "internal/control.h"
#include "internal/popup_host.h"
#include "internal/popup_menu.h"

namespace facet
{
namespace
{

using namespace internal;

// Text of a chord at the end of a row: the longest chord text ("Ctrl+Shift+Alt+Win+Page Down").
constexpr int kMenuShortcutText = 64;

struct MenuItemRow
{
    MenuRowDesc desc;
    char shortcut[kMenuShortcutText] = {};
};

MenuItemRow BuildMenuItemRow(const MenuItemOptions& options, bool disabled)
{
    MenuItemRow row;
    row.desc.icon = options.icon;
    row.desc.destructive = options.destructive;
    row.desc.disabled = disabled;
    ImGuiKeyChord chord = options.shortcut;
    if (options.command.IsValid())
    {
        ShortcutInfo info;
        if (GetShortcut(options.command, &info))
        {
            chord = info.primary != ImGuiKey_None ? info.primary : info.alternate;
            row.desc.disabled = row.desc.disabled || !info.enabled;
        }
    }
    if (chord != ImGuiKey_None)
    {
        ChordToText(chord, row.shortcut, kMenuShortcutText);
        row.desc.shortcut = row.shortcut;
    }
    return row;
}

// The level the rows belong to: the popup level being submitted, else the current window.
ImGuiID MenuItemLevel(ImGuiWindow* window)
{
    const ImGuiID level = CurrentPopupLevel();
    return level != 0 ? level : window->ID;
}

bool MenuItemPressed(const char* label, MenuItemRow& row, const MenuItemOptions& options)
{
    ImGuiWindow* window = ControlWindow();
    if (window == nullptr)
        return false;
    const MenuRowResult result = MenuRow(MenuItemLevel(window), label != nullptr ? label : "", row.desc);
    if (!result.pressed)
        return false;
    if (options.command.IsValid())
        TriggerShortcut(options.command);
    return true;
}

bool PushedDisabled()
{
    return (GImGui->CurrentItemFlags & ImGuiItemFlags_Disabled) != 0;
}

} // namespace

bool MenuItem(const char* label, const MenuItemOptions& options)
{
    if (ControlWindow() == nullptr)
        return false;
    MenuItemRow row = BuildMenuItemRow(options, PushedDisabled());
    return MenuItemPressed(label, row, options);
}

bool MenuCheckItem(const char* label, bool* checked, const MenuItemOptions& options)
{
    if (ControlWindow() == nullptr)
        return false;
    // A null value pointer shows the row and never writes (rule 14): it cannot be activated.
    MenuItemRow row = BuildMenuItemRow(options, PushedDisabled() || checked == nullptr);
    row.desc.checked = checked != nullptr && *checked;
    row.desc.slot = true;
    if (!MenuItemPressed(label, row, options) || checked == nullptr)
        return false;
    *checked = !*checked;
    ImGui::MarkItemEdited(GImGui->LastItemData.ID);
    return true;
}

bool MenuRadioItem(const char* label, int* selection, int value, const MenuItemOptions& options)
{
    if (ControlWindow() == nullptr)
        return false;
    MenuItemRow row = BuildMenuItemRow(options, PushedDisabled() || selection == nullptr);
    row.desc.checked = selection != nullptr && *selection == value;
    row.desc.radio = true;
    row.desc.slot = true;
    if (!MenuItemPressed(label, row, options) || selection == nullptr)
        return false;
    *selection = value;
    ImGui::MarkItemEdited(GImGui->LastItemData.ID);
    return true;
}

} // namespace facet
