#pragma once

// The column layout of KeybindList (keybind_list.cpp), on its own so the list and the code that
// aims at its fields (the demo page's selftest) measure the same way:
//
//     const KeybindListLayout layout = KeybindListMeasure(width, alternate);
//     // Side by side: the label column, then each chord column, then the reset button.
//     // Stacked (too narrow for a label column): the label on a line of its own, the fields under it.
//     const ImRect first = KeybindListFirstField(ImRect(min, max), alternate, true, 0);
//
// Lengths are pixels at the current scale; x positions are from the left edge of the rows, which
// is the list's left edge plus `inset`. A list with a height lays its rows out inside a scroll
// area, whose padding is the inset; the column headings above the area take the same inset, so
// each heading stands over its column. With a height the rows also keep `barReserve` free at their
// end, where the overlay scroll bar of the area lies, so the bar never covers a reset button.

#include <imgui.h>
#include <imgui_internal.h>

namespace facet::internal
{

struct KeybindListLayout
{
    float inset = 0.0f;        // the scroll area's padding with a height; 0 without
    float width = 0.0f;        // the rows: the list's width less the inset on both sides
    float gap = 0.0f;          // between the columns: spacing.sm
    float spacing = 0.0f;      // the item spacing between the lines of a stacked row
    float button = 0.0f;       // the reset button: one control height square
    float column = 0.0f;       // one chord column
    float labelWidth = 0.0f;   // the label column; the whole width when stacked
    float primaryX = 0.0f;
    float alternateX = 0.0f;
    float resetX = 0.0f;
    float barReserve = 0.0f;   // with a height: kept free at the row end for the overlay scroll bar
    float rowHeight = 0.0f;    // the fields: one control height
    float labelLine = 0.0f;    // stacked: the label line, one Body line
    float headingHeight = 0.0f;  // the column headings line with its spacing
    float groupHeight = 0.0f;    // a group heading line with its spacing
    bool stacked = false;
};

// The layout of a list `width` pixels wide with one chord column, or two with `alternate`;
// `scrolling` for a list with a height (its rows in a scroll area).
KeybindListLayout KeybindListMeasure(float width, bool alternate, bool scrolling = false);

// The field of `column` (0 primary, 1 alternate) in the first row of a list whose item rect is
// `list`, under a group heading when `groupHeading`, with the rows scrolled to the top.
ImRect KeybindListFirstField(const ImRect& list, bool alternate, bool groupHeading, int column, bool scrolling = false);

// Where the latest KeybindList drew its Primary column heading and its first primary capture
// field (empty while no row was shown), for probes in tests and selftests.
struct KeybindListProbe
{
    ImGuiID id = 0;
    float primaryHeadingX = 0.0f;  // screen x of the Primary heading text
    ImRect firstPrimary;           // the first row's primary capture field
};
const KeybindListProbe& KeybindListLastProbe();

} // namespace facet::internal
