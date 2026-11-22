// Border rule entries of the surfaces families (lists, navigation, containers, layout,
// overlays), the shared blocks and the test app.
// Read by tests/test_border_rule.cpp.
//
// FACET_BORDER_PENDING("path") lists a file that still draws an outline at another width than
// the hairline; remove its line once the file draws every outline through the outline helpers of
// src/internal/control.h. FACET_BORDER_EXCEPTION("path", "width argument", "reason") allows a stroke
// that is not an outline (a glyph, a ring that marks focus) with its reason.

FACET_BORDER_EXCEPTION("demo/overlay_backdrop.cpp", "facet::Px(t.metrics.hairline) * 2.0f",
                       "a ring inside the moving discs of the picture that stands in for a host's frame behind the overlay menu, not an outline of the interface")
FACET_BORDER_EXCEPTION("src/internal/loading.cpp", "thickness",
                       "the track and the turning arc of the spinner: the stroke its caller asks for, not an outline")
FACET_BORDER_EXCEPTION("demo/pages/page_draw.cpp", "thickness",
                       "the progress arc of the drawing demo at sliderTrackHeight, a track, not an outline")
