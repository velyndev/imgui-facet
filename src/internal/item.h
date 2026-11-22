#pragma once

// Item behaviour: the hover, press, focus and disabled logic shared by every Facet widget.
//
// One call lays out a control (ItemSize), registers it with Dear ImGui (ItemAdd) and runs the
// button behaviour, and returns one ItemState with everything a widget needs to draw itself:
//
//     const ImGuiID id = ImGui::GetID(label);
//     const facet::internal::ItemState item = facet::internal::ItemBehavior(id, size);
//     if (item.visible)
//     {
//         const ImU32 fill = facet::ToU32(item.disabled ? c.fill[3] : facet::Mix(c.fill[1], c.fill[0], item.pressAmount));
//         facet::FillRect(drawList, item.rect.Min, item.rect.Max, fill, radius);
//         facet::internal::DrawItemFocusRing(item, radius);   // focus.h
//     }
//     return item.pressed;
//
// Parts of a widget made of several items (slider handles, segments) run PartBehavior on a
// rectangle inside the space the widget already laid out; it does the same without ItemSize.
//
// What it handles, so that every widget behaves the same way:
//   - Disabled: ImGui::BeginDisabled (the item flag) or ItemOptions::disabled. A disabled item
//     is not hovered, held, pressed or focusable, shows the not-allowed cursor while the pointer
//     is over it, and still answers ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled).
//     Facet draws the disabled colors itself; the style alpha of BeginDisabled is not used.
//   - Read-only and mixed: ItemOptions or the pushed ImGuiItemFlags_ReadOnly and
//     ImGuiItemFlags_MixedValue; reported in the state, the widget decides what they mean.
//   - The minimum hit target: the rectangle that takes the mouse grows to
//     T().metrics.minHitTarget in each direction where the visible rectangle is smaller,
//     centered on it. The item rectangle Dear ImGui reports (GetItemRectMin and Max) and the
//     keyboard navigation rectangle stay the visible one.
//   - Cursors: the hand cursor over an interactive item, the not-allowed cursor over a
//     disabled one.
//   - Focus: keyboard focus is Dear ImGui's NavId. Dear ImGui's own navigation highlight is
//     never drawn (Facet does not call RenderNavCursor) and a focused item is not reported as
//     hovered; focusVisible says when to draw the Facet focus ring (focus.h): only while the
//     last input was the keyboard or a gamepad.
//   - Repeat (pressed again while held, mouse and activation keys, at the key repeat rate) and
//     the mouse event that presses (PressMode).
//   - Forced states for captures and tests (below).
// After the call the item is Dear ImGui's last item, so ImGui::IsItemHovered, IsItemActive,
// IsItemFocused, SetItemTooltip, BeginDragDropSource and Target, SetItemDefaultFocus and
// keyboard navigation work for the caller as after a stock widget.
//
// Animated amounts (hoverAmount, pressAmount, focusAmount) come from the animation store under
// the item id with sub keys from kItemAnimSubBase up; widgets keep their own sub keys below it.
//
// Forced states: capture runs render pages without any input, yet the gallery has to show how
// a widget looks hovered, pressed, focused, while dragging and with its popup open. A page
// asks for such a look right before submitting the widget:
//
//     facet::internal::ForceNextItemState(facet::internal::ForcedItemState::Hovered);
//     facet::Button("Save");
//
// The item behaviour takes the request when the next widget runs it and reports the forced
// state as if the input had happened, for drawing only: no value is written, no result is
// returned and no focus moves. A widget made of several items takes the request once with
// TakeForcedItemState at its start and passes it to each part in ItemOptions::forced with the
// part's index; a request for part -1 goes to part -1 or, in a widget without one, part 0.
// Pages use the PageCtx helper (demo/pages/pages.h), which does nothing outside capture runs,
// so interactive and selftest runs keep real input. The disabled look needs no request: wrap
// the widget in ImGui::BeginDisabled as an app would.

#include <cstdint>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/widgets/common.h"

namespace facet::internal
{

enum class ForcedItemState : std::uint32_t
{
    None = 0,
    Hovered = 1u << 0,   // the pointer rests on the item
    Pressed = 1u << 1,   // the item is held down (active)
    Focused = 1u << 2,   // keyboard focus with the focus ring, as after keyboard input
    Dragging = 1u << 3,  // a drag of the item's value or handle is under way; implies Pressed
    Open = 1u << 4,      // a widget with a popup or a list shows it open
};
FACET_FLAGS(ForcedItemState)

struct ForcedItemRequest
{
    ForcedItemState state = ForcedItemState::None;
    int part = -1;  // sub-item of a widget made of several items (handle, segment); -1 the widget
};

// Asks the next widget that runs the item behaviour to show `state`. `part` selects one of
// its sub-items (a range slider's handle index, a segment); -1 is the widget itself. A second
// call before a widget took the first replaces it. A request no widget took is dropped at
// facet::EndFrame, so it never reaches a widget of a later frame. Not public API: only the
// demo capture and tests call it.
void ForceNextItemState(ForcedItemState state, int part = -1);

// For the item behaviour and widgets made of several items: returns the pending request and
// clears it. None when there is none.
ForcedItemRequest TakeForcedItemState();

// Drops a pending request (facet::EndFrame).
void ClearForcedItemState();

// Which mouse event presses the item.
enum class PressMode
{
    ClickRelease,  // press and release inside (buttons, toggles, rows); the default
    Click,         // the press itself (sliders, handles, drag starts, repeat buttons)
    Release,       // the release, even after a press elsewhere (menu items)
};

// Animation store sub keys used by the item behaviour, from here up. Widgets use sub keys
// below kItemAnimSubBase for their own values under the same id.
inline constexpr int kItemAnimSubBase = 0x7F00;

struct ItemOptions
{
    PressMode press = PressMode::ClickRelease;
    bool repeat = false;       // pressed on the click, then again while held (key repeat rate)
    bool disabled = false;     // disabled in addition to ImGui::BeginDisabled
    bool readOnly = false;     // reported as readOnly in addition to ImGuiItemFlags_ReadOnly
    bool mixed = false;        // reported as mixed in addition to ImGuiItemFlags_MixedValue
    bool tabStop = true;       // false: reached by the arrow keys but not by Tab
    bool navigable = true;     // false: never takes keyboard focus (a span between handles)
    bool hitTarget = true;     // grow the hit rectangle to the minimum hit target token
    bool allowOverlap = false; // a later item over this one may take the hover
    bool handCursor = true;    // false: keep the arrow cursor (text-like items)
    float baseline = -1.0f;    // pixels from the top to the text baseline, for SameLine text
    ImGuiButtonFlags buttonFlags = 0;  // extra ButtonBehavior flags (other mouse buttons)
    int part = -1;             // the index of this part for forced states
    // The request the widget took with TakeForcedItemState; null: the call takes the pending
    // request itself (widgets made of one item).
    const ForcedItemRequest* forced = nullptr;
};

struct ItemState
{
    ImGuiID id = 0;
    ImRect rect;       // the visible rectangle, in screen pixels
    ImRect hitRect;    // the rectangle that takes the mouse (at least the minimum hit target)
    bool visible = false;   // not clipped away: draw it (every other field is valid either way)

    bool hovered = false;   // the pointer is over the hit rectangle and the item may take it
    bool held = false;      // held down by the mouse or an activation key
    bool pressed = false;   // activated this frame (click, Space, Enter, gamepad, repeats)
    bool keyboardPressed = false;  // pressed came from an activation key or the gamepad
    bool released = false;  // the mouse button that held it went up this frame
    bool active = false;    // Dear ImGui's active item (ImGui::IsItemActive)
    bool activated = false;     // became active this frame
    bool deactivated = false;   // stopped being active this frame
    bool focused = false;       // has keyboard focus (Dear ImGui's NavId)
    bool focusVisible = false;  // focused while the keyboard modality is on: draw the focus ring
    bool dragging = false;      // held by the mouse (or a forced Dragging state)
    bool open = false;          // forced Open state, for widgets with a popup
    bool disabled = false;
    bool readOnly = false;
    bool mixed = false;
    // The forced state applied to this item (captures and tests). Input blocks built on the
    // item behaviour ignore real input while it is not None.
    ForcedItemState forced = ForcedItemState::None;

    // Animated 0 to 1 over T().motion.durationFast; they jump while motion is reduced.
    float hoverAmount = 0.0f;
    float pressAmount = 0.0f;
    float focusAmount = 0.0f;
};

// Lays out a control of `size` pixels at the cursor, registers it and runs its behaviour.
ItemState ItemBehavior(ImGuiID id, ImVec2 size, const ItemOptions& options = {});

// The same for a part of a widget at `rect` (screen pixels) inside space the widget already
// laid out: no ItemSize, so the cursor does not move.
ItemState PartBehavior(ImGuiID id, const ImRect& rect, const ItemOptions& options = {});

// The rectangle that takes the mouse for a visible rectangle: grown to `minSize` pixels in each
// direction where it is smaller, centered on it.
ImRect HitRect(const ImRect& rect, float minSize);

// ---- Widget entry and exit -----------------------------------------------------------------

// What the SetNextItem calls and pushed item flags asked for, captured at the start of a widget
// before its first sub-item: ItemAdd clears the next-item data, so a widget made of several
// items has to read it first (docs/api-conventions.md, rule 11). Apply it to the item that
// stands for the widget with ApplyNextItemData.
struct NextItemData
{
    ImGuiNextItemData data;     // Dear ImGui's next-item data at entry
    ImGuiItemFlags itemFlags = 0;  // the pushed item flags at entry (BeginDisabled included)
    bool hasWidth = false;      // ImGui::SetNextItemWidth was called
    float width = 0.0f;         // its value in pixels (Dear ImGui's meaning: < 0 from the right)
};

// Captures and clears the next-item data.
NextItemData TakeNextItemData();
// Puts captured next-item data back, so the next ItemAdd consumes it.
void ApplyNextItemData(const NextItemData& next);

// The width of a widget in pixels by the precedence of docs/api-conventions.md, rule 6:
// `optionsWidth` in design units (> 0 that many units, kFill the available width, other values
// < 0 the available width minus that many units), then ImGui::SetNextItemWidth (from `next`),
// then ImGui::PushItemWidth when the width stack is not empty, then `defaultWidth` in pixels
// (kFill: the available width). Never below 1 pixel.
float ResolveItemWidth(float optionsWidth, const NextItemData& next, float defaultWidth);

// After ImGui::EndGroup around the items of a widget: makes `widgetId` the last item id while
// none of its parts is active or was just deactivated (EndGroup reports that part's id then),
// keeping the group's rectangle and status. So the item queries after the call describe the
// whole widget.
void FinishWidgetGroup(ImGuiID widgetId);

// Marks the last item edited, as ImGui::MarkItemEdited(id) does, also when `id` is not Dear ImGui's
// active item: a widget whose value its popover wrote, or that Escape restored while a part inside
// the popover was active. ImGui::MarkItemEdited asserts in that case; here the last item's edited
// status is set the same way and the active item's edit flags are left alone. Every Facet widget and
// block reports its edits through this call, never through ImGui::MarkItemEdited directly (a test
// checks the sources), so an edit made while another item is active never asserts.
void MarkWidgetEdited(ImGuiID id);

} // namespace facet::internal
