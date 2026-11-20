#pragma once

// Segment strip: a row of joined segments on one track, the shape SegmentedControl,
// KeybindModeSelector and ToggleButtonGroup (buttons) share, so the selection pill moves the same
// way in each of them and the segments measure, color and cut their labels alike.
//
//     SegmentStripDesc desc;
//     desc.kind = SegmentStripKind::Choice;   // one index, shown by the sliding selection pill
//     desc.roving = true;                      // one Tab stop, Left and Right move the selection
//     written = SegmentStrip(label, items, &index, {}, desc);
//
// Layout, in pixels: the visible label part in front (MeasureFrontLabel, rule 6), then the track,
// one control height tall. Each segment is its content (icon, label) plus paddingX on both sides,
// or one control height square for icons only. With width 0 the widget is that content, at most
// the available width; any other width (or a content wider than the room) divides the track into
// equal segments. A label that does not fit its segment collapses to the item's icon when every
// item has one (collapseToIcons), and is cut with an ellipsis otherwise; either way it shows in full
// as a tooltip on hover and keyboard focus. With `overflow`, segments that do not fit move into a
// "more" menu at the end of the track instead (FitOverflow, overflow.h), and the visible segments
// share the track width.
//
// Look: the track is fill[2] with the 3:1 boundary hairline (fill[3] and a separator hairline while
// disabled, no fill while read-only). The selected segment (Choice) sits on the selection pill in
// accentSubtle, metrics.segmentInset inside the track and concentric with it; latched segments
// (Latches) get the same shape as a still highlight that fades in. Labels of selected or latched
// segments are semibold (PushPillLabelStyle). Hairline dividers in separator stand between
// neighbouring segments that are not selected. Hover and press tint a segment with fill[2] and fill[1]
// inside the same inset.
//
// Edges sit on whole pixels, so the pill comes to rest on whole pixels; it animates relative to
// the window, so scrolling does not animate it, and it follows a change of width or scale with its
// spring (selection_pill.h). With reduced motion it moves at once.

#include <span>

#include <imgui.h>

#include "facet/widgets/common.h"
#include "widgets/selection/selection_parts.h"

namespace facet::internal
{

enum class SegmentStripKind
{
    Choice,   // *index; the selection pill shows it
    Latches,  // one flag per item; each latched segment shows the highlight
};

struct SegmentStripDesc
{
    SegmentStripKind kind = SegmentStripKind::Choice;
    bool iconOnly = false;        // segments show the item icons; labels become tooltips
    bool collapseToIcons = true;  // labels that do not fit collapse to icons when every item has one
    bool roving = true;           // Choice: one Tab stop with Left, Right, Home and End; false: every segment is a Tab stop
    bool allowEmpty = false;      // Choice without roving: activating the selected segment writes -1
    bool dragSelect = false;      // Choice: pressing selects at once and a drag across the segments moves the selection
    bool overflow = false;        // segments that do not fit move into a "more" menu
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;           // units, as the widget's options.width
    bool readOnly = false;
    bool mixed = false;           // nothing shows selected until an edit
    const char* error = nullptr;  // the message line under the strip
    const char* helpText = nullptr;
};

// Submits the strip. `index` is the Choice value (null: read-only), `flags` the Latches values
// (items without a flag are drawn disabled). Returns true when a value was written.
bool SegmentStrip(const char* label, const SelectionItems& items, int* index, std::span<bool> flags,
                  const SegmentStripDesc& desc);

} // namespace facet::internal
