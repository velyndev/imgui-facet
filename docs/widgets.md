# Widgets

Facet's widgets are grouped into fourteen families, one public header each under
`include/facet/widgets/`. `facet/facet.h` includes all of them, and `facet/widgets/common.h`
holds the types the families share (`ControlSize`, `Tone`, `ChoiceItem`, `DataState`, the
control size stack, the edit session queries and the built-in strings). Every declaration
follows [the API conventions](api-conventions.md).

**Status: planned.** No widget is implemented yet. The family headers are being filled with
documented declarations whose bodies only draw a placeholder; the widgets themselves are built
in Phase 2. The demo gallery has one page per family under Widgets.

| Family | Header | Documentation | What it holds |
|---|---|---|---|
| Buttons | `facet/widgets/buttons.h` | [buttons](widgets/buttons.md) | Push buttons with variants, icon buttons, copy buttons, links, toggle button groups |
| Selection | `facet/widgets/selection.h` | [selection](widgets/selection.md) | Toggles, checkboxes, radio groups, segmented controls, chips, cycle selectors, option cards, the key bind mode selector |
| Sliders | `facet/widgets/sliders.h` | [sliders](widgets/sliders.md) | Sliders, range sliders, multi-handle sliders, knobs, two-dimensional pads |
| Inputs | `facet/widgets/inputs.h` | [inputs](widgets/inputs.md) | Text, search and number fields, drag and vector fields, special-purpose fields, key bind capture, ratings |
| Pickers | `facet/widgets/pickers.h` | [pickers](widgets/pickers.md) | Color, date, time, angle, icon and thumbnail pickers, the file browser |
| Lists | `facet/widgets/lists.h` | [lists](widgets/lists.md) | Dropdowns, combo boxes, list boxes, selectable and reorderable lists, tree views, transfer lists |
| Navigation | `facet/widgets/navigation.h` | [navigation](widgets/navigation.md) | Sidebar, tab bars, menu bars, menus and menu items, breadcrumbs, pagination, wizards |
| Containers | `facet/widgets/containers.h` | [containers](widgets/containers.md) | The floating shell, the app frame, tool and status bars, accordions, cards, scroll areas, virtual lists |
| Layout | `facet/widgets/layout.h` | [layout](widgets/layout.md) | Stacks, grids, flow, split views, panels, settings rows, separators, item grids |
| Overlays | `facet/widgets/overlays.h` | [overlays](widgets/overlays.md) | Modals, popovers, context menus, tooltips, toasts, drawers, the command palette |
| Display | `facet/widgets/display.h` | [display](widgets/display.md) | Text styles, alerts, empty states, progress, badges, key caps, code and log views, data tables |
| Charts | `facet/widgets/charts.h` | [charts](widgets/charts.md) | Sparklines, line, bar and donut charts, gauges, meters, legends |
| Media | `facet/widgets/media.h` | [media](widgets/media.md) | Images in design units, avatars, thumbnail strips, carousels, image viewers, drop zones |
| Composites | `facet/widgets/composites.h` | [composites](widgets/composites.md) | Curve and gradient editors, property grids, console views, key bind and preset lists, inventory grids |

The low-level drawing, images and icons that exist today are in the core headers; see
[theming](theming.md), [scaling](scaling.md) and [images and icons](images-and-icons.md).
