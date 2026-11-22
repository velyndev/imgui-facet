#pragma once

// The column layout of KeybindList (keybind_list.cpp), on its own so the list and the code that
// aims at its fields (the demo page's selftest) measure the same way:
//
//     const KeybindListLayout layout = KeybindListMeasure(width, alternate);
//     // Side by side: the label column, then each chord column, then the reset button.
//     // Stacked (too narrow for a label column): the label on a line of its own, the fields under it.
//     const ImRect first = KeybindListFirstField(ImRect(min, max), alternate, true, 0);
//
// Lengths are pixels at the current scale; x positions are from the list's left edge.

#include <imgui.h>
#include <imgui_internal.h>

namespace facet::internal
{

struct KeybindListLayout
{
    float gap = 0.0f;          // between the columns: spacing.sm
    float spacing = 0.0f;      // the item spacing between the lines of a stacked row
    float button = 0.0f;       // the reset button: one control height square
    float column = 0.0f;       // one chord column
    float labelWidth = 0.0f;   // the label column; the whole width when stacked
    float primaryX = 0.0f;
    float alternateX = 0.0f;
    float resetX = 0.0f;
    float rowHeight = 0.0f;    // the fields: one control height
    float labelLine = 0.0f;    // stacked: the label line, one Body line
    float headingHeight = 0.0f;  // the column headings line with its spacing
    float groupHeight = 0.0f;    // a group heading line with its spacing
    bool stacked = false;
};

// The layout of a list `width` pixels wide with one chord column, or two with `alternate`.
KeybindListLayout KeybindListMeasure(float width, bool alternate);

// The field of `column` (0 primary, 1 alternate) in the first row of a list whose item rect is
// `list`, under a group heading when `groupHeading`.
ImRect KeybindListFirstField(const ImRect& list, bool alternate, bool groupHeading, int column);

} // namespace facet::internal
