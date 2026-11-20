#pragma once

// Parts shared by the selection family and by ToggleButtonGroup (buttons): roving focus for
// groups that are one Tab stop, the latch behind the `mixed` option, and the check and radio
// marks. Names carry the family ("Selection") because every family links into one library.
//
// Roving focus, for a group whose entries are parts under the widget id:
//
//     const int stop = SelectionRovingStop(count, selected, enabled, idOf);  // the one Tab stop
//     for (each entry i)
//     {
//         itemOptions.tabStop = i == stop;
//         const ItemState entry = PartBehavior(idOf(i), rect, itemOptions);
//         if (entry.focused && entry.forced == ForcedItemState::None) focused = i;
//         ...
//     }
//     // After every entry ran, so the entry that receives the focus does not see the same press:
//     const int moved = SelectionRovingKey(idOf(focused), focused, count, keys, enabled);
//     if (moved >= 0) { SelectionMoveFocus(idOf(moved)); ... write the selection ... }
//
// `enabled(i)` says whether entry i can take the focus, `idOf(i)` gives its id.

#include <span>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/widgets/common.h"
#include "internal/item.h"

namespace facet::internal
{

// ---- Items -----------------------------------------------------------------------------------

// The items of a choice widget: a span of ChoiceItem, or of labels for the label-only overloads.
struct SelectionItems
{
    std::span<const ChoiceItem> choices;
    std::span<const char* const> labels;

    int Count() const { return static_cast<int>(labels.empty() ? choices.size() : labels.size()); }
    // Item `index` (in range); a null label reads as "".
    ChoiceItem At(int index) const
    {
        ChoiceItem item = labels.empty() ? choices[static_cast<std::size_t>(index)] : ChoiceItem{labels[static_cast<std::size_t>(index)]};
        if (item.label == nullptr)
            item.label = "";
        return item;
    }
};

// The id of entry `index` of a widget: ChoiceItem::id when it is set, else the index, hashed
// with the widget id (docs/api-conventions.md rule 2). The two kinds hash with different seeds,
// so an item id never equals the id of an index.
inline ImGuiID SelectionEntryId(ImGuiID widgetId, ImGuiID itemId, int index)
{
    if (itemId != 0)
        return ImHashData(&itemId, sizeof(itemId), ~widgetId);
    return ImHashData(&index, sizeof(index), widgetId);
}

// ---- Roving focus --------------------------------------------------------------------------

// The entry that is the group's one Tab stop: the focused entry while the focus is inside the
// group, else the selected entry when it is enabled, else the first enabled entry; -1 when none
// is enabled.
template <class Enabled, class IdOf>
int SelectionRovingStop(int count, int selected, Enabled enabled, IdOf idOf)
{
    const ImGuiID nav = GImGui->NavId;
    for (int i = 0; i < count; ++i)
        if (nav != 0 && idOf(i) == nav && enabled(i))
            return i;
    if (selected >= 0 && selected < count && enabled(selected))
        return selected;
    for (int i = 0; i < count; ++i)
        if (enabled(i))
            return i;
    return -1;
}

// The next enabled entry from `from` in direction `step` (+1 or -1), wrapping past the ends when
// `wrap`; -1 when there is none.
template <class Enabled>
int SelectionStepEnabled(int from, int step, int count, bool wrap, Enabled enabled)
{
    int i = from;
    for (int n = 0; n < count; ++n)
    {
        i += step;
        if (i < 0 || i >= count)
        {
            if (!wrap)
                return -1;
            i = (i + count) % count;
        }
        if (enabled(i))
            return i;
    }
    return -1;
}

// Which keys move the focus inside a group.
struct SelectionRovingKeys
{
    bool leftRight = true;  // Left and Right step back and forward
    bool upDown = false;    // Up and Down step back and forward
    bool homeEnd = true;    // Home and End go to the first and last enabled entry
    bool wrap = false;      // stepping past an end continues at the other end
};

// Takes the keys of `keys` for the focused entry `focusedId` this frame, through Dear ImGui's key
// ownership so navigation does not also move the focus, and reads them: *step is +1 or -1, and
// *toEnd says Home (-1) or End (+1). Returns 0 when a key was pressed, -1 otherwise.
int SelectionRovingKeyStep(ImGuiID focusedId, const SelectionRovingKeys& keys, int* step, bool* toEnd);

// The entry a key press moves the focus to, or -1 when no key moved it (an end without wrap, or
// no other enabled entry).

template <class Enabled>
int SelectionRovingKey(ImGuiID focusedId, int focused, int count, const SelectionRovingKeys& keys, Enabled enabled)
{
    if (focused < 0 || focused >= count)
        return -1;
    int step = 0;
    bool toEnd = false;
    if (SelectionRovingKeyStep(focusedId, keys, &step, &toEnd) < 0)
        return -1;
    // Home (step -1) looks for the first enabled entry from the start, End (+1) for the last.
    const int target = toEnd ? (step < 0 ? SelectionStepEnabled(-1, 1, count, false, enabled)
                                         : SelectionStepEnabled(count, -1, count, false, enabled))
                             : SelectionStepEnabled(focused, step, count, keys.wrap, enabled);
    return target == focused ? -1 : target;
}

// Moves the keyboard focus to `id` (an entry of the current window) with the focus ring shown,
// as navigation would.
void SelectionMoveFocus(ImGuiID id);

// ---- Mixed -----------------------------------------------------------------------------------

// The `mixed` option shows the indeterminate look until the first edit writes a definite value
// (docs/api-conventions.md rule 7). Returns whether the look shows this frame: `mixed` (the
// option or ImGuiItemFlags_MixedValue) and no edit since it was last switched on. `edited` is
// true on the frame the widget wrote its value. Keeps one flag under `id`.
bool SelectionMixedShown(ImGuiID id, bool mixed, bool edited);

// ---- Marks -----------------------------------------------------------------------------------

enum class SelectionMark
{
    Off,
    On,
    Mixed,
};

// The square of a check or radio mark (metrics.checkboxSize) with its left edge at `x`, centered
// on `centerY`, on whole pixels.
ImRect SelectionMarkRect(float x, float centerY);

// The corner radius of a checkbox box (pixels): radii.element, at most a quarter of the side, so a
// box never reads as the circle of a radio mark. The focus ring takes it too.
float SelectionCheckboxRadius(const ImRect& box);

// Draws a checkbox: a box with SelectionCheckboxRadius corners. Off is the hover fill of a neutral control
// with the 3:1 boundary hairline; On and Mixed fill it with the accent (labelSecondary while
// read-only) and draw the check or minus glyph in onAccent, so the mixed state differs from both
// others by its shape. `onAmount` (0 to 1) fades between Off and the mark; `shown` is the glyph
// faded in or out.
void DrawSelectionCheckbox(ImDrawList* drawList, const ImRect& box, SelectionMark shown, float onAmount,
                           const ItemState& item);

// Draws a radio mark: a circle with the boundary hairline; selected, an accent disc (labelSecondary
// while read-only) with an onAccent dot in the middle.
void DrawSelectionRadio(ImDrawList* drawList, const ImRect& box, float onAmount, const ItemState& item);

// The animated amount of a mark under `id`: 1 while on, 0 while off, over motion.durationFast.
float SelectionMarkAmount(ImGuiID id, bool on);

// ---- The checkbox item -----------------------------------------------------------------------

// What a checkbox shows besides its label and mark (Checkbox, CheckboxGroup).
struct SelectionCheckboxDesc
{
    ControlSize size = ControlSize::Inherit;
    bool readOnly = false;          // shown and focusable, never written
    bool disabled = false;          // in addition to ImGui::BeginDisabled
    const char* error = nullptr;    // the message line under the box
    const char* helpText = nullptr;
};

// One checkbox item under `id`: the box, then the visible part of `label`; the label is part of
// the hit area. Width by rule 6 from `next` (its content by default); a narrower width cuts the
// label with an ellipsis and a tooltip. A click, Space or Enter flips *mark (Off and Mixed to On,
// On to Off). A mouse press that then moves over other checkboxes of the same window with the
// button held paints the first box's new value onto each of them (and onto the first). Returns
// true when *mark was written; a null `mark` is read-only.
bool SelectionCheckbox(const char* label, ImGuiID id, SelectionMark* mark, const NextItemData& next,
                       const SelectionCheckboxDesc& desc);

} // namespace facet::internal
