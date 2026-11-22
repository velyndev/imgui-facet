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
// ends in an ellipsis, and resting the pointer on it shows the full text as a tooltip, as does
// keyboard focus on the row's control (the tooltip then sits under the label). An optional
// icon sits before the label, one text line square, in labelSecondary; the label column counts
// it, and in a group where any row has an icon every row keeps the icon's room free (measured
// in the previous frame), so the label texts start in one line. The optional description is a
// second line under the label (footnote, labelSecondary). It wraps across the label column and
// the control slot when nothing else sits under the control (no help text, no error, a control
// no taller than one control height), and in the label column otherwise, so a short label never
// squeezes it into a narrow column next to an empty row.
// Under the control come the help text (footnote, labelSecondary) or, replacing it, the error
// message with the error icon in the destructive color, wrapped in the slot.
// A row begun inside ImGui::BeginDisabled draws its label, marker, description and help text in
// labelTertiary like its disabled control (the error keeps its color: it still applies).
// `modified` puts a marker dot after the label; `resettable` reserves a square reset button at
// the end of the row (so the control does not change width), shown and focusable while
// `modified` is on. Tab order follows the visual order: the control, then the reset button.
//
// Inside BeginRowGroup / EndRowGroup the label column is the widest label of the group measured
// in the previous frame plus the gap, at least the labelColumn token and at most
// kRowGroupMaxShare of the available width; outside a group it is the token (or
// options.labelColumn). When any row of a group is resettable, every row of the group keeps
// the reset slot free (also measured in the previous frame), so the controls end in one line.
// The row pushes its label on the id stack until EndRow, so the control
// in every row can use the same "##value". The row is one group item, so rows stack with the
// window's item spacing and work inside table cells.

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/icons.h"
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
    Icon icon;                          // before the label; none by default
    const char* description = nullptr;  // second line under the label; null for none
    const char* error = nullptr;        // message under the control with the error icon; replaces helpText
    const char* helpText = nullptr;     // message under the control in labelSecondary
    bool modified = false;              // the value differs from its default: marker after the label
    bool resettable = false;            // a reset button at the end of the row while modified
    ControlSize size = ControlSize::Inherit;  // the control the label lines up with
    float labelColumn = 0.0f;           // units, gap included; 0: the group's column or the token
    // Pixels kept free at the end of the row, gap included, at least: the room a reset slot takes
    // in another row of the same column, so rows outside a group end their controls in one line and
    // stack at the same width (SettingsRow). 0 for none.
    float endReserve = 0.0f;
};

// The geometry of the row in progress, in screen pixels.
struct RowLayout
{
    bool stacked = false;     // the label sits above the control
    ImRect label;             // the label line (the visible label text box)
    ImRect control;           // the control slot: its width, and one control height tall
    float baseline = 0.0f;    // the baseline of the label and of the control's text
    bool truncated = false;   // the label did not fit and ends in an ellipsis
    bool disabled = false;    // inside ImGui::BeginDisabled: the texts are in the disabled color
};

// Starts a row (see the top of this file). Returns its layout, valid until EndRow.
const RowLayout& BeginRow(const char* label, const RowOptions& options = {});
// Ends the row started last. Returns true on the frame the reset button was activated.
bool EndRow();

// Rows between these share one label column (see the top of this file).
void BeginRowGroup(const char* id);
void EndRowGroup();

// The column of settings rows (facet::BeginSettingsRow) the cursor is in. The rows of one window
// under one id stack top and one column key keep one reset reserve; a container that narrows the
// work rect (the container inset of a card, a group box or an accordion) sets a key of its own while
// its content runs, so its rows form a column apart from the rows around it. 0 outside any; a key
// set in an earlier frame reads as 0.
ImGuiID RowColumnKey();
// Sets the column key and returns the one it replaces, to be put back with this call again.
ImGuiID SetRowColumnKey(ImGuiID key);

// ---- One control column -------------------------------------------------------------------------
//
// Controls stacked in a column line up whatever their labels say: a control with its own label in
// front (MeasureFrontLabel in control.h) starts its control at the label column of a labelled row,
// the same x as the control of a settings row, and controls that end in a column of their own (a
// slider's value, the number field beside a slider) share that column, so their tracks end at one
// x as well and keep one length.

// The label column a visible label in front of a control takes, in pixels, the gap to the control
// included: T().layout.labelColumn, the column of a settings row outside a group.
float RowFrontLabelColumn();

// Most trailing columns shared in one frame; a column beyond them is not shared.
inline constexpr int kRowTrailingColumns = 32;

// A column at the end of a control, shared by the controls of one window and one column key
// (RowColumnKey) whose right edge is `right` (screen pixels) and that ask with the same `kind` (a
// module's own numbering, so a wide kind of column never widens another). Returns the widest
// `width` (pixels) any of them asked for in this frame or the previous one, at least `width`.
// The edge counts relative to the window's content, so a window that moves keeps its columns;
// each Dear ImGui context has columns of its own. Nothing is allocated.
float RowTrailingColumn(float right, float width, int kind = 0);
// A column at the start of a control (a visible label in front that takes only its text width,
// below the label column plus layout.minRowControl), shared like RowTrailingColumn by the controls
// of one window and column key that start at `left` (screen pixels) and ask with the same `kind`,
// so their controls start at one x in a narrow column too. Leading and trailing columns never
// share a record.
float RowLeadingColumn(float left, float width, int kind = 0);
// Forgets every shared leading and trailing column (facet::Shutdown through the widgets_common
// subsystem).
void ClearRowTrailingColumns();

// The y at which text in the current font sits on `baseline` (screen pixels).
float TextTopForBaseline(float baseline);

// facet::EndFrame (through the widgets_common subsystem): reports rows and groups still open
// and resets both stacks, so the next frame starts clean.
void EndFrameRows();

} // namespace facet::internal
