// SegmentedControl: joined segments with the sliding selection pill, one Tab stop with the arrow
// keys inside, a press or a drag across the segments to choose, built on the segment strip
// (segment_strip.h) that KeybindModeSelector and ToggleButtonGroup share.

#include "facet/widgets/selection.h"

#include "widgets/selection/segment_strip.h"

namespace facet
{
namespace
{

bool SegmentedControlImpl(const char* label, int* index, const internal::SelectionItems& items,
                          const SegmentedControlOptions& options)
{
    internal::SegmentStripDesc desc;
    desc.kind = internal::SegmentStripKind::Choice;
    desc.iconOnly = options.iconOnly;
    desc.collapseToIcons = true;
    desc.roving = true;
    desc.dragSelect = true;
    desc.size = options.size;
    desc.width = options.width;
    desc.readOnly = options.readOnly;
    desc.mixed = options.mixed;
    return internal::SegmentStrip(label, items, index, {}, desc);
}

} // namespace

bool SegmentedControl(const char* label, int* index, std::span<const ChoiceItem> items, const SegmentedControlOptions& options)
{
    return SegmentedControlImpl(label, index, {items, {}}, options);
}

bool SegmentedControl(const char* label, int* index, std::span<const char* const> items,
                      const SegmentedControlOptions& options)
{
    return SegmentedControlImpl(label, index, {{}, items}, options);
}

} // namespace facet
