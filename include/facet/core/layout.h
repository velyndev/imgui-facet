#pragma once

// Responsive layout: width classes of the available content width, and the column layout of
// labelled rows.
//
// Widgets and apps adapt to the room they get, not to the window or the monitor: a settings
// row stacks its control under the label, a sidebar becomes an icon rail, a toolbar moves
// items into a "more" menu. The width classes give every such decision the same breakpoints,
// from the theme's layout tokens (T().layout) in design units, so a region of 600 units is
// Regular at every UI scale. Like every Facet function, these are for the thread that runs
// the Dear ImGui frame.

#include <imgui.h>

namespace facet
{

// Classes of an available width, from narrow to wide.
//   Compact   below T().layout.regularFrom: one column; labels go above their controls,
//             side panels collapse.
//   Regular   from regularFrom up: labels beside controls, one content column.
//   Expanded  from T().layout.expandedFrom up: room for a side column next to the content.
enum class WidthClass
{
    Compact,
    Regular,
    Expanded,
};

// The class of a width in design units. NaN and negative widths are Compact.
WidthClass WidthClassOf(float widthUnits);

// The available content width at the cursor of the current window
// (ImGui::GetContentRegionAvail().x), in design units (pixels / Scale()). 0 without a current
// window.
float AvailableWidthUnits();

// WidthClassOf(AvailableWidthUnits()).
WidthClass AvailableWidthClass();

// The columns of a labelled row (a label and its control) laid out in `availableWidth`
// pixels. Shared by every labelled row, so labels line up across SettingsRow, PropertyGrid,
// KeyValueList and the demo pages. Offsets are in pixels from the row's left edge.
struct LabeledRowLayout
{
    bool stacked = false;        // the control sits on its own line under the label
    float labelWidth = 0.0f;     // room for the label text (the column minus the gap)
    float controlOffset = 0.0f;  // where the control starts: the label column, or 0 when stacked
    float controlWidth = 0.0f;   // the rest of the row, or the whole row when stacked
};

// The row sits beside its label when the label column plus T().layout.minRowControl fit in
// `availableWidth`; otherwise the row is stacked. `labelColumnUnits` is the label column in
// design units, the gap to the control included; 0 or less uses T().layout.labelColumn. The
// gap is T().spacing.sm. Widths below 0 count as 0.
LabeledRowLayout LayoutLabeledRow(float availableWidth, float labelColumnUnits = 0.0f);

} // namespace facet
