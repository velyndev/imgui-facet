#pragma once

// Focus ring and keyboard modality.
//
// The focus ring tells a keyboard user where the focus is. A mouse user does not need it, and
// a ring appearing on every clicked control is noise, so the ring shows only while the keyboard
// modality is on: the last input that counts was a navigation or activation key (Tab, the
// arrows, Space, Enter, Escape, Home, End, Page Up and Down) or a gamepad button, not a mouse
// click. Typing text, modifier keys alone and mouse movement keep the modality as it is; a
// mouse button press or a wheel turn switches it off. The focus subsystem updates the modality
// in facet::NewFrame from the input Dear ImGui received for the frame.
//
// The ring is for keyboard users only: a hover or a click never shows it. Every widget asks
// ItemState::focusVisible (item.h) or KeyboardModality(), never Dear ImGui's focus alone, and a
// list's current row that the pointer moved shows the row fill, not the ring.
//
// The ring: one stroke of T().metrics.focusRingWidth (2 design units, whole pixels) just outside
// the control, one hairline away from its edge, with corners concentric to the control's radius and
// of the control's corner shape (RingShape: the theme's corners for rectangles, circular arcs for
// knobs and capsules). No halo: the stroke alone marks the focus. Its color is colors.focusRing,
// which Theme::ApplyAccent derives to reach 3:1 against every surface of the theme, so the ring
// stays visible for any accent in both themes. It fades in and out with ItemState::focusAmount
// (motion.durationFast). It is drawn into the item's window draw list with the window's clip
// rectangle, so a control scrolled partly out of a child window has its ring cut at the same edge.
//
//     const ItemState item = ItemBehavior(id, size);
//     ... draw the control ...
//     DrawItemFocusRing(item, Px(T().radii.control));
//
// Widgets with inner parts that take focus (slider handles, segments) draw it around the part:
//
//     DrawFocusRing(drawList, handleRect, handleRadius, handle.focusAmount);
//
// A round knob takes the round ring, so the gap stays even:
//
//     DrawFocusRing(drawList, knobRect, knobRect.GetWidth() * 0.5f, knob.focusAmount, 0, RingShape::Round);
//
// Rows of lists, menus and tables, table cells and tiles of a grid sit too close to their neighbours
// and to the edge of their list for a ring outside: they draw it inside their own bounds, so it is
// never clipped and never grows the layout:
//
//     DrawFocusRingInside(drawList, rowFillRect, Px(T().radii.control), row.focusAmount);

#include <imgui.h>
#include <imgui_internal.h>

#include "internal/item.h"

namespace facet::internal
{

// True while the focus ring should show: the last counting input was the keyboard or a gamepad.
bool KeyboardModality();
// Sets the modality, for code that moves the focus on a key press of its own and for tests.
void SetKeyboardModality(bool keyboard);

// The color the ring is drawn in for the current theme (see the top of this file).
ImVec4 FocusRingColor();

// The rectangle the ring covers around a control rectangle.
ImRect FocusRingRect(const ImRect& rect);
// How far the ring reaches out from the control's edge, in pixels: one hairline of gap plus the
// ring width. Leave at least this much between a control and its neighbours.
float FocusRingOutset();
// The ring width in pixels: metrics.focusRingWidth snapped to whole pixels.
float FocusRingWidth();

// The corner shape of the ring, matching how the control draws its own outline, so the gap
// between the two stays even all around.
enum class RingShape
{
    Rect,   // corners as facet::FillRect draws them (continuous from the token radius on)
    Round,  // circular arcs: circles (a square with radius half its side) and capsules
};

// Draws the ring around `rect` (screen pixels) whose corners have `radius` pixels, at
// `amount` (0 nothing, 1 fully shown). `corners` takes ImDrawFlags_RoundCorners* as in draw.h.
void DrawFocusRing(ImDrawList* drawList, const ImRect& rect, float radius, float amount = 1.0f,
                   ImDrawFlags corners = 0, RingShape shape = RingShape::Rect);

// The ring of an item: around item.rect with item.focusAmount, into the current window's draw
// list. Nothing when the item is not visible or the amount is 0.
void DrawItemFocusRing(const ItemState& item, float radius, ImDrawFlags corners = 0, RingShape shape = RingShape::Rect);

// The ring inside `rect` (screen pixels): its outer edge on the edge of `rect`, with corners of
// `radius` pixels, the width and color of the ring outside. For rows, cells, menu rows and tiles.
void DrawFocusRingInside(ImDrawList* drawList, const ImRect& rect, float radius, float amount = 1.0f,
                         ImDrawFlags corners = 0);
// The inside ring of an item: inside item.rect with item.focusAmount, into the current window's
// draw list. Nothing when the item is not visible or the amount is 0.
void DrawItemFocusRingInside(const ItemState& item, float radius, ImDrawFlags corners = 0);

} // namespace facet::internal
