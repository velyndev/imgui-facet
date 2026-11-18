#pragma once

// Buttons: push buttons with variants, icon buttons, copy buttons, links and toggle button
// groups.
//
// The declarations follow docs/api-conventions.md; widgets/common.h holds the types shared
// with other families. The family is planned: its widgets are declared here and implemented in
// Phase 2 (docs/widgets/buttons.md). Until then every widget function draws a placeholder of
// the widget's default size, returns false and writes nothing.
//
// Common to every widget here:
//   - Activation is a click released inside, Enter or Space while focused, or the gamepad
//     activate button; the bool result is true on that frame.
//   - Disabled: ImGui::BeginDisabled. The button is then not a Tab stop and returns false.
//   - Lengths are design units (multiplied by Px()); `size` picks the control height token.
//   - The label follows Dear ImGui's "##" and "###" rules; nothing else keys the id.

#include <span>

#include <imgui.h>

#include "facet/core/icons.h"
#include "facet/widgets/common.h"

namespace facet
{

// --- Button -----------------------------------------------------------------------------------

enum class ButtonVariant
{
    Secondary,    // neutral fill; the default
    Primary,      // accent fill; one primary action per view
    Plain,        // text only, a fill on hover and press
    Destructive,  // destructive tone, for actions that delete or reset
};

struct ButtonOptions
{
    ButtonVariant variant = ButtonVariant::Secondary;
    Icon icon;                          // leading icon; none by default
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;                 // units; 0 fits the label, kFill fills, < 0 fills minus that much
    bool loading = false;               // spinner in place of the icon, width kept, not activatable
};

// A push button showing the visible label part, with the icon in front of it. Returns true on
// the frame it is activated. Default width: its content; text in ControlTextStyle(size). An
// empty visible label with an icon draws an icon-only button of the same height (prefer
// IconButton, which also shows a tooltip). Keeps only the press and focus animations under
// its id. A null label is treated as "".
bool Button(const char* label, const ButtonOptions& options = {});

// --- IconButton -------------------------------------------------------------------------------

struct IconButtonOptions
{
    ButtonVariant variant = ButtonVariant::Plain;
    bool selected = false;              // shows the on look; the caller flips it on activation
    ControlSize size = ControlSize::Inherit;
    bool loading = false;               // spinner in place of the icon; not activatable
};

// A square button one control height wide showing only `icon`. The visible label part is
// required: it is the tooltip (on hover and on keyboard focus) and the accessible name; an
// empty visible part asserts. Returns true on activation. For a latched tool (pin, lock), pass
// `selected` and flip the caller's flag when the result is true.
bool IconButton(const char* label, Icon icon, const IconButtonOptions& options = {});

// --- CopyButton -------------------------------------------------------------------------------

struct CopyButtonOptions
{
    ButtonVariant variant = ButtonVariant::Plain;
    bool showLabel = false;             // draws the visible label after the icon; otherwise icon only
    ControlSize size = ControlSize::Inherit;
};

// A button that copies `text` (UTF-8, read during the call) to the clipboard through
// ImGui::SetClipboardText when activated. For a moment afterwards it shows the check icon and
// Strings::copied. The visible label part is the tooltip and the accessible name; an empty
// visible part uses Strings::copy. Returns true on the frame it copied. A null `text` draws the
// button disabled and returns false. Default width: one control height, or its content with
// showLabel. Keeps the time of the last copy under its id.
bool CopyButton(const char* label, const char* text, const CopyButtonOptions& options = {});

// --- Link -------------------------------------------------------------------------------------

struct LinkOptions
{
    const char* url = nullptr;          // read during the call; tooltip, and opened on activation
    bool visited = false;               // draws the visited look; the caller tracks visits
    ControlSize size = ControlSize::Inherit;
};

// The visible label in the accentText token, underlined on hover and focus, as one item the
// width of its text. Returns true on activation. With `url`, activation also asks Dear ImGui to
// open it (ImGui::GetPlatformIO().Platform_OpenInShellFn, as ImGui::TextLinkOpenURL does) and a
// trailing external-link icon marks it. Keeps no state.
bool Link(const char* label, const LinkOptions& options = {});

// --- ToggleButtonGroup ------------------------------------------------------------------------

struct ToggleButtonGroupOptions
{
    bool iconOnly = true;               // buttons show the item icons; labels become tooltips
    bool allowEmpty = false;            // index overload: activating the selected button writes -1
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;                 // units; 0 fits the buttons; buttons that do not fit move into a "more" menu
    bool readOnly = false;
    bool mixed = false;                 // no button shows the on look until an edit
};

// A row of latched toolbar buttons joined into one group, one button per item. Each button is
// its own Tab stop; Space or Enter toggles the focused one. Items with `disabled` are drawn
// disabled and skipped. The visible label part is drawn in front of the buttons inside the
// width (docs/api-conventions.md, rule 6); "##name" hides it, for example in a toolbar. The item
// queries describe the whole group (rule 11).
//
// Independent latches: `selected` holds one flag per item. Extra flags are ignored; items
// without a flag read as off and their buttons are drawn disabled. Activating a button flips
// its flag. Returns true when a flag was written.
bool ToggleButtonGroup(const char* label, std::span<bool> selected, std::span<const ChoiceItem> items,
                       const ToggleButtonGroupOptions& options = {});

// Exclusive latch: *index is the selected item, -1 for none. Activating a button writes its
// index, or -1 with allowEmpty when it was already selected. Returns true when *index was
// written. An out-of-range value shows no button on and is written only by an edit.
bool ToggleButtonGroup(const char* label, int* index, std::span<const ChoiceItem> items,
                       const ToggleButtonGroupOptions& options = {});

// The same two forms for text buttons with labels only. Items without icons have nothing else
// to show, so these overloads always draw the labels and ignore iconOnly.
bool ToggleButtonGroup(const char* label, std::span<bool> selected, std::span<const char* const> items,
                       const ToggleButtonGroupOptions& options = {});
bool ToggleButtonGroup(const char* label, int* index, std::span<const char* const> items,
                       const ToggleButtonGroupOptions& options = {});

} // namespace facet
