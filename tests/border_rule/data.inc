// Border rule entries of the data families: display with the data table, charts, media,
// composites.
// Read by tests/test_border_rule.cpp.
//
// FACET_BORDER_PENDING("path") lists a file that still draws an outline at another width than
// the hairline; remove its line once the file draws every outline through the outline helpers of
// src/internal/control.h. FACET_BORDER_EXCEPTION("path", "width argument", "reason") allows a stroke
// that is not an outline (a glyph, a ring that marks focus) with its reason.

FACET_BORDER_EXCEPTION("src/widgets/composites/accent_picker.cpp", "hairline * kAccentPickerRingWidth",
                       "the chosen-color ring around the custom well: the selection mark of SwatchPalette's chosen swatch, outside the color it marks, not an outline")
FACET_BORDER_PENDING("src/widgets/charts/chart_parts.cpp")
FACET_BORDER_PENDING("src/widgets/media/image_viewer.cpp")
FACET_BORDER_PENDING("src/widgets/media/media_look.cpp")
FACET_BORDER_PENDING("src/widgets/media/thumbnail_strip.cpp")
FACET_BORDER_PENDING("src/widgets/media/timeline_scrubber.cpp")
