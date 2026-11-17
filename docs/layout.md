# Layout

`include/facet/core/layout.h` gives every layout decision the same breakpoints: width classes
of the room a region really has, and the columns of a labelled row. Everything here works in
design units (pixels at scale 1, see `docs/scaling.md`) unless a function says pixels, so a
region keeps its class at every UI scale. Like every Facet function, these run on the thread that
runs the Dear ImGui frame.

## Width classes

Widgets and apps adapt to the room they get, not to the window or the monitor. A settings row in
a narrow side panel stacks its control under the label even when the window is wide.

| Class | Available width | Layout |
|---|---|---|
| `WidthClass::Compact` | below `T().layout.regularFrom` (400) | One column; labels above their controls |
| `WidthClass::Regular` | from `regularFrom` up | Labels beside controls, one content column |
| `WidthClass::Expanded` | from `T().layout.expandedFrom` (720) up | Room for a side column next to the content |

```cpp
// The class of the room left at the cursor of the current window.
switch (facet::AvailableWidthClass())
{
case facet::WidthClass::Compact:  DrawSingleColumn(); break;
case facet::WidthClass::Regular:  DrawWithLabels();   break;
case facet::WidthClass::Expanded: DrawWithSidebar();  break;
}
```

- `WidthClassOf(units)` classifies any width in design units. NaN and negative widths are
  `Compact`.
- `AvailableWidthUnits()` is `ImGui::GetContentRegionAvail().x` divided by `Scale()`, or 0
  without a current window.
- `AvailableWidthClass()` is `WidthClassOf(AvailableWidthUnits())`.

The breakpoints are theme tokens (`LayoutTokens` in `theme.h`), so an app can move them with
`SetTheme`; they blend linearly during an animated theme switch.

## Labelled rows

`LayoutLabeledRow(availableWidth, labelColumnUnits)` returns the columns of a row that puts a
label in front of a control, in pixels from the row's left edge:

```cpp
const facet::LabeledRowLayout row = facet::LayoutLabeledRow(ImGui::GetContentRegionAvail().x);
// row.stacked:        the control goes on its own line under the label
// row.labelWidth:     room for the label text (the column without the gap)
// row.controlOffset:  where the control starts (0 when stacked)
// row.controlWidth:   the rest of the row (the whole row when stacked)
```

- The label column is `labelColumnUnits` design units, the gap to the control included; 0 or less
  takes `T().layout.labelColumn` (112). The gap is `T().spacing.sm`.
- The row stays side by side while the column plus `T().layout.minRowControl` (128) fit;
  otherwise it stacks, so a control never gets too narrow to operate.
- Widths below 0 count as 0.

Every labelled row in Facet takes its columns from this function, so labels line up across rows
of different kinds. Inside the library, the row layout block (`src/internal/row_layout.h`) builds
the full row on it: a label column shared by a group of rows (the widest label of the group, at
most 40 % of the width), the label on the baseline of the control's text, truncation with an
ellipsis and the full label as a tooltip, a description line under the label, a help or error
line under the control, and a marker and reset button for modified values. The demo page
"Rows and width classes" shows it at every width. The public `SettingsRow` widget
(`widgets/layout.h`) is planned for Phase 2 and will use the same block.
