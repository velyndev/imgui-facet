// ToggleButtonGroup: a joined row of latched toolbar buttons, as independent latches or one
// exclusive choice, on the segment strip SegmentedControl uses (src/widgets/selection/
// segment_strip.h), so the exclusive form's selection pill moves exactly as in a segmented control.
// Every button is its own Tab stop; buttons that do not fit move into a "more" menu.

#include "facet/widgets/buttons.h"

#include "widgets/selection/segment_strip.h"

namespace facet
{
namespace
{

internal::SegmentStripDesc ToggleButtonDesc(internal::SegmentStripKind kind, bool iconOnly,
                                            const ToggleButtonGroupOptions& options)
{
    internal::SegmentStripDesc desc;
    desc.kind = kind;
    desc.iconOnly = iconOnly;
    desc.collapseToIcons = false;  // buttons that do not fit go into the menu instead
    desc.roving = false;           // each button is a Tab stop
    desc.allowEmpty = options.allowEmpty;
    desc.overflow = true;
    desc.size = options.size;
    desc.width = options.width;
    desc.readOnly = options.readOnly;
    desc.mixed = options.mixed;
    return desc;
}

} // namespace

bool ToggleButtonGroup(const char* label, std::span<bool> selected, std::span<const ChoiceItem> items,
                       const ToggleButtonGroupOptions& options)
{
    return internal::SegmentStrip(label, {items, {}}, nullptr, selected,
                                  ToggleButtonDesc(internal::SegmentStripKind::Latches, options.iconOnly, options));
}

bool ToggleButtonGroup(const char* label, int* index, std::span<const ChoiceItem> items,
                       const ToggleButtonGroupOptions& options)
{
    return internal::SegmentStrip(label, {items, {}}, index, {},
                                  ToggleButtonDesc(internal::SegmentStripKind::Choice, options.iconOnly, options));
}

bool ToggleButtonGroup(const char* label, std::span<bool> selected, std::span<const char* const> items,
                       const ToggleButtonGroupOptions& options)
{
    return internal::SegmentStrip(label, {{}, items}, nullptr, selected,
                                  ToggleButtonDesc(internal::SegmentStripKind::Latches, false, options));
}

bool ToggleButtonGroup(const char* label, int* index, std::span<const char* const> items,
                       const ToggleButtonGroupOptions& options)
{
    return internal::SegmentStrip(label, {{}, items}, index, {},
                                  ToggleButtonDesc(internal::SegmentStripKind::Choice, false, options));
}

} // namespace facet
