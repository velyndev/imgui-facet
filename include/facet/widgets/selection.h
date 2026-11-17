#pragma once

// Selection: toggles, checkboxes and checkbox groups, radio groups, segmented controls, chips,
// cycle selectors, option cards and the key bind mode selector.
//
// The declarations follow docs/api-conventions.md; widgets/common.h holds the types shared
// with other families. The family is planned: its widgets are declared here and implemented in
// Phase 2 (docs/widgets/selection.md). Until then every function draws a placeholder of the
// widget's default size, returns false and writes nothing.
//
// Common to every widget here:
//   - The bool result is true on the frame the widget wrote its value or selection.
//   - Clicks, Space and Enter write at once; Escape does not undo a choice.
//   - A null value pointer draws the widget read-only and returns false.
//   - Disabled: ImGui::BeginDisabled. readOnly shows and focuses the value but never writes.
//   - Lengths are design units (multiplied by Px()).
//   - Item lists are spans read during the call. A ChoiceItem's label follows the "##" rules;
//     entries get ids under the widget id (ChoiceItem::id, or the index).
//   - Overloads taking std::span<const char* const> behave like the ChoiceItem ones with only
//     labels set.

#include <span>

#include <imgui.h>

#include "facet/core/icons.h"
#include "facet/core/image.h"
#include "facet/core/input.h"
#include "facet/widgets/common.h"

namespace facet
{

// --- Toggle -----------------------------------------------------------------------------------

struct ToggleOptions
{
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;                 // units; 0 fits label and switch; wider puts the switch at the end
    bool readOnly = false;
    bool loading = false;               // the knob shows a spinner; input ignored
    bool mixed = false;                 // knob in the middle until the first edit
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// An on/off switch with the visible label in front of it. A click on the label or the switch,
// Space or Enter flips *value; dragging the knob past the middle sets it. Returns true when
// *value was written. Default width: label, spacing.sm and the toggle tokens. Keeps the knob
// animation under its id.
bool Toggle(const char* label, bool* value, const ToggleOptions& options = {});

// --- Checkbox ---------------------------------------------------------------------------------

struct CheckboxOptions
{
    ControlSize size = ControlSize::Inherit;
    bool readOnly = false;
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// A box (checkboxSize token) then the visible label; the label is part of the hit area. A
// click, Space or Enter flips *value. Dragging across several checkboxes with the button held
// paints the first box's new value onto each. Returns true when *value was written. For the
// mixed look use the CheckState overload (docs/api-conventions.md, rule 7).
bool Checkbox(const char* label, bool* value, const CheckboxOptions& options = {});
// Tri-state: activating Off or Mixed writes On, activating On writes Off. Only the caller sets
// Mixed (for example a select-all box over a partly checked list). Returns true when *state
// was written.
bool Checkbox(const char* label, CheckState* state, const CheckboxOptions& options = {});

// --- CheckboxGroup ----------------------------------------------------------------------------

struct CheckboxGroupOptions
{
    bool selectAll = false;             // a parent box with Strings::selectAll that shows Mixed when partly checked
    int columns = 0;                    // 0 chooses one or more columns from the available width
    ControlSize size = ControlSize::Inherit;
    bool readOnly = false;
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// A labelled list of checkboxes, one per item; `checked` holds one flag per item (extra flags
// are ignored, items without a flag are drawn disabled). The visible label part is drawn above
// the boxes. Each box is a Tab stop. Returns true when any flag was written (the select-all box
// writes every enabled item). Default width: the available width, divided into columns.
bool CheckboxGroup(const char* label, std::span<bool> checked, std::span<const ChoiceItem> items,
                   const CheckboxGroupOptions& options = {});
bool CheckboxGroup(const char* label, std::span<bool> checked, std::span<const char* const> items,
                   const CheckboxGroupOptions& options = {});

// --- RadioGroup -------------------------------------------------------------------------------

struct RadioGroupOptions
{
    Orientation orientation = Orientation::Vertical;  // Horizontal wraps when the row is full
    ControlSize size = ControlSize::Inherit;
    bool readOnly = false;
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// Mutually exclusive options with a dot each; ChoiceItem::detail shows as a second line. The
// group is one Tab stop on the selected option (or the first); arrow keys move the selection and
// write at once. *index -1 means none. The visible label part is drawn above the group. Returns
// true when *index was written.
bool RadioGroup(const char* label, int* index, std::span<const ChoiceItem> items,
                const RadioGroupOptions& options = {});
bool RadioGroup(const char* label, int* index, std::span<const char* const> items,
                const RadioGroupOptions& options = {});

// --- SegmentedControl -------------------------------------------------------------------------

struct SegmentedControlOptions
{
    bool iconOnly = false;              // segments show item icons; labels become tooltips
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;                 // units; 0 fits the segments, kFill gives equal segments
    bool readOnly = false;
    bool mixed = false;                 // no segment highlighted until an edit
};

// Joined segments with the sliding selection pill. One Tab stop; arrow keys, a click or a drag
// across the segments move the selection. Labels collapse to icons when the width is too small
// and every item has one. The visible label part is the accessible name and is not drawn.
// Returns true when *index was written. Keeps the pill position under its id.
bool SegmentedControl(const char* label, int* index, std::span<const ChoiceItem> items,
                      const SegmentedControlOptions& options = {});
bool SegmentedControl(const char* label, int* index, std::span<const char* const> items,
                      const SegmentedControlOptions& options = {});

// --- Chip -------------------------------------------------------------------------------------

struct ChipOptions
{
    bool selected = false;              // shows the on look; the caller flips it on activation
    bool removable = false;             // adds a remove button; read ChipRemovePressed after the call
    Icon icon;                          // leading icon; none by default
    ImVec4 color = {0.0f, 0.0f, 0.0f, 0.0f};  // leading dot in this data color; alpha 0 for none
    ControlSize size = ControlSize::Inherit;
};

// A pill-shaped tag with the visible label. Returns true when the chip body is activated (a
// click, Space or Enter); a chip used as a filter flips its own flag then. The remove button is
// a separate target with Strings::remove as its name; Delete or Backspace on the focused chip
// presses it too. Default width: its content.
bool Chip(const char* label, const ChipOptions& options = {});

// True when the remove button of the last Chip call this frame was activated (as
// TabBarClosedTab works for tab bars). The caller removes the chip from its data.
bool ChipRemovePressed();

// --- ChipGroup --------------------------------------------------------------------------------

struct ChipGroupOptions
{
    bool removable = false;             // a remove button per chip; read ChipGroupRemovedItem after the call
    bool clearAll = false;              // multi overload: a trailing chip with Strings::clear unsets every flag
    bool singleLine = false;            // one row; chips that do not fit move into a "more" chip
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;                 // units; 0 fills the available width
    bool readOnly = false;
};

// Chips that wrap into rows, one per item; ChoiceItem::detail shows as a count after the label.
// Each chip is a Tab stop. The visible label part is the accessible name and is not drawn.
//
// Filter (multi) form: `selected` holds one flag per item; activating a chip flips its flag.
// Returns true when a flag was written.
bool ChipGroup(const char* label, std::span<bool> selected, std::span<const ChoiceItem> items,
               const ChipGroupOptions& options = {});
bool ChipGroup(const char* label, std::span<bool> selected, std::span<const char* const> items,
               const ChipGroupOptions& options = {});
// Choice (single) form: activating a chip writes its index to *index; -1 means none. Returns
// true when *index was written.
bool ChipGroup(const char* label, int* index, std::span<const ChoiceItem> items,
               const ChipGroupOptions& options = {});
bool ChipGroup(const char* label, int* index, std::span<const char* const> items,
               const ChipGroupOptions& options = {});

// The item whose remove button was activated in the last ChipGroup call this frame, or -1. The
// caller removes the item from its data.
int ChipGroupRemovedItem();

// --- CycleSelector ----------------------------------------------------------------------------

struct CycleSelectorOptions
{
    bool wrap = true;                   // stepping past the last item returns to the first
    bool pageDots = false;              // a dot per item under the value
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;                 // units; 0 fills the available width
    bool readOnly = false;
    bool mixed = false;
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// The visible label, then a value between previous and next arrow buttons. Left and Right (or
// the buttons, or the wheel while hovered) step *index and write at once; disabled items are
// skipped. Returns true when *index was written. A value out of range shows the placeholder
// Strings::select.
bool CycleSelector(const char* label, int* index, std::span<const ChoiceItem> items,
                   const CycleSelectorOptions& options = {});
bool CycleSelector(const char* label, int* index, std::span<const char* const> items,
                   const CycleSelectorOptions& options = {});

// --- KeybindModeSelector ----------------------------------------------------------------------

struct KeybindModeSelectorOptions
{
    bool allowAlways = true;            // offers BindMode::Always
    bool allowOff = true;               // offers BindMode::Off
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;                 // units; 0 fits the segments; too narrow becomes a dropdown
    bool readOnly = false;
    bool mixed = false;
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// Chooses how a key bind activates (BindMode in core/input.h): Hold, Toggle, Always and Off
// with the Strings bind* names, drawn as a segmented control. Keyboard as SegmentedControl.
// Returns true when *mode was written. A mode that is not offered is shown but cannot be
// chosen again once left.
bool KeybindModeSelector(const char* label, BindMode* mode, const KeybindModeSelectorOptions& options = {});

// --- OptionCardGroup --------------------------------------------------------------------------

// One card of an OptionCardGroup. Label first, so {"Option 1"} works.
struct OptionCard
{
    const char* label = nullptr;        // title line; required
    const char* description = nullptr;  // wrapped text under the title; null for none
    Icon icon;                          // shown left of the title when no image is set
    ImageHandle image;                  // picture across the top of the card; invalid for none
    const char* badge = nullptr;        // short status text in the corner, for example a real tier name
    bool disabled = false;
    ImGuiID id = 0;                     // stable id for reordered cards; 0 uses the index
};

struct OptionCardGroupOptions
{
    int columns = 0;                    // 0 fits as many cards of cardWidth as the width allows
    float cardWidth = 0.0f;             // units; 0 for the default card width
    float width = 0.0f;                 // units; 0 fills the available width
    bool readOnly = false;
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// Large selectable cards in a grid, for choices that need a picture or a description. The
// selected card shows the accent outline and a check. The single form is one Tab stop with
// arrow-key movement (like RadioGroup); the multi form makes each card a Tab stop that Space
// flips (like CheckboxGroup). The visible label part is the accessible name and is not drawn.
bool OptionCardGroup(const char* label, int* index, std::span<const OptionCard> cards,
                     const OptionCardGroupOptions& options = {});
bool OptionCardGroup(const char* label, std::span<bool> selected, std::span<const OptionCard> cards,
                     const OptionCardGroupOptions& options = {});

} // namespace facet
