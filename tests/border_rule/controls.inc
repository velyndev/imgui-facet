// Border rule entries of the controls families: buttons, selection, sliders, inputs, pickers.
// Read by tests/test_border_rule.cpp.
//
// FACET_BORDER_PENDING("path") lists a file that still draws an outline at another width than
// the hairline; remove its line once the file draws every outline through the outline helpers of
// src/internal/control.h. FACET_BORDER_EXCEPTION("path", "width argument", "reason") allows a stroke
// that is not an outline (a glyph, a ring that marks focus) with its reason.

FACET_BORDER_EXCEPTION("src/widgets/sliders/arc_range_slider.cpp", "m.track",
                       "the track of a full circle arc range: sliderTrackHeight, a slider track, not an outline")

FACET_BORDER_PENDING("src/widgets/pickers/angle_picker.cpp")
FACET_BORDER_PENDING("src/widgets/pickers/color_model.cpp")
FACET_BORDER_PENDING("src/widgets/pickers/icon_picker.cpp")
FACET_BORDER_PENDING("src/widgets/pickers/swatch_palette.cpp")
FACET_BORDER_PENDING("src/widgets/pickers/thumbnail_picker.cpp")
