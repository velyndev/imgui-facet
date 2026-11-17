#pragma once

// Labelled rows: the layout behind SettingsRow, PropertyGrid, KeyValueList, DependentGroup and
// every page that lines up labels with controls.
//
//     BeginRowGroup("display");                      // optional: rows share one label column
//     BeginRow("Vertical sync", {.description = "Waits for the display refresh."});
//     ... one control, sized by the pushed item width (ImGui::CalcItemWidth or
//         ResolveItemWidth in item.h) ...
//     EndRow();
//     BeginRow("Scale", {.error = "Must be between 50 and 200", .modified = true, .resettable = true});
//     ...
//     if (EndRow())                                  // the reset button was activated
//         scale = kDefaultScale;
//     EndRowGroup();
//
// Layout: a label column and a control slot that takes the rest of the available width
// (facet::LayoutLabeledRow in core/layout.h decides the columns and when to stack). In a narrow
// region (the row would leave the control less than T().layout.minRowControl) the label goes on
// its own line above the control, which then takes the whole width. The label is drawn in the
// control's text style and the label color, with its baseline on the baseline of the text of a
// control of options.size, so label and control text line up. A label too long for its column
// ends in an ellipsis, and resting the pointer on it shows the full text as a tooltip. The
// optional description is a second line under the label (footnote, labelSecondary, wrapped in
// the column). Under the control come the help text (footnote, labelSecondary) or, replacing
// it, the error message with the error icon in the destructive color, wrapped in the slot.
// `modified` puts a marker dot after the label; `resettable` reserves a square reset button at
// the end of the row (so the control does not change width), shown and focusable while
// `modified` is on. Tab order follows the visual order: the control, then the reset button.
//
// Inside BeginRowGroup / EndRowGroup the label column is the widest label of the group measured
// in the previous frame plus the gap, at least the labelColumn token and at most
// kRowGroupMaxShare of the available width; outside a group it is the token (or
// options.labelColumn). The row pushes its label on the id stack until EndRow, so the control
// in every row can use the same "##value". The row is one group item, so rows stack with the
// window's item spacing and work inside table cells.

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/widgets/common.h"

namespace facet::internal
{

// The largest share of the available width a group's shared label column takes.
inline constexpr float kRowGroupMaxShare = 0.4f;
// Rows open at once (rows inside rows, for dependent groups).
inline constexpr int kMaxRowDepth = 8;
// Groups open at once.
inline constexpr int kMaxRowGroupDepth = 4;

struct RowOptions
{
    const char* description = nullptr;  // second line under the label; null for none
    const char* error = nullptr;        // message under the control with the error icon; replaces helpText
    const char* helpText = nullptr;     // message under the control in labelSecondary
    bool modified = false;              // the value differs from its default: marker after the label
    bool resettable = false;            // a reset button at the end of the row while modified
    ControlSize size = ControlSize::Inherit;  // the control the label lines up with
    float labelColumn = 0.0f;           // units, gap included; 0: the group's column or the token
};

// The geometry of the row in progress, in screen pixels.
struct RowLayout
{
    bool stacked = false;     // the label sits above the control
    ImRect label;             // the label line (the visible label text box)
    ImRect control;           // the control slot: its width, and one control height tall
    float baseline = 0.0f;    // the baseline of the label and of the control's text
    bool truncated = false;   // the label did not fit and ends in an ellipsis
};

// Starts a row (see the top of this file). Returns its layout, valid until EndRow.
const RowLayout& BeginRow(const char* label, const RowOptions& options = {});
// Ends the row started last. Returns true on the frame the reset button was activated.
bool EndRow();

// Rows between these share one label column (see the top of this file).
void BeginRowGroup(const char* id);
void EndRowGroup();

// The y at which text in the current font sits on `baseline` (screen pixels).
float TextTopForBaseline(float baseline);

// facet::EndFrame (through the widgets_common subsystem): reports rows and groups still open
// and resets both stacks, so the next frame starts clean.
void EndFrameRows();

} // namespace facet::internal
