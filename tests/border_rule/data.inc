// Border rule entries of the data families: display with the data table, charts, media,
// composites.
// Read by tests/test_border_rule.cpp.
//
// FACET_BORDER_PENDING("path") lists a file that still draws an outline at another width than
// the hairline; remove its line once the file draws every outline through the outline helpers of
// src/internal/control.h. FACET_BORDER_EXCEPTION("path", "width argument", "reason") allows a stroke
// that is not an outline (a glyph, a ring that marks focus) with its reason.

FACET_BORDER_EXCEPTION("src/widgets/charts/chart_parts.cpp", "stroke",
                       "the hollow marker and legend swatch of a hidden series: a glyph at a share of charts.lineWidth, not an outline")
FACET_BORDER_EXCEPTION("src/widgets/media/image_viewer.cpp", "ring",
                       "the mark of the visible region on the minimap, drawn over a picture where a hairline would vanish, not an outline")
