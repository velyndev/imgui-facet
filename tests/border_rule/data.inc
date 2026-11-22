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
FACET_BORDER_EXCEPTION("src/widgets/charts/donut_chart.cpp", "r1 - inner",
                       "a donut segment drawn as one thick arc: its width is the ring of data, not an outline")
FACET_BORDER_EXCEPTION("src/widgets/charts/gauge.cpp", "width",
                       "the track and the value arc of a gauge at charts.gaugeWidth, not an outline")
FACET_BORDER_EXCEPTION("src/widgets/charts/gauge.cpp", "zoneWidth",
                       "the zone bands along a gauge, a share of its track, not an outline")
FACET_BORDER_EXCEPTION("src/widgets/display/character_counter.cpp", "stroke",
                       "the track and the used arc of the counter ring, a share of the ring size, not an outline")
FACET_BORDER_EXCEPTION("src/widgets/display/progress_ring.cpp", "stroke",
                       "the track and the progress arc of a ring, a share of its diameter, not an outline")
