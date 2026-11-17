// SegmentedControl: planned. Draws the placeholder at the control's default size until Phase 2
// implements the widget in this file.

#include "facet/widgets/selection.h"

#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "widgets/controls_stub.h"

namespace facet
{
namespace
{

float SegmentWidth(const char* text, const SegmentedControlOptions& options)
{
    if (options.iconOnly)
        return ControlHeight(options.size);
    return internal::StubLabelWidth(text, options.size) + 2.0f * Px(T().spacing.sm);
}

} // namespace

bool SegmentedControl(const char* label, int*, std::span<const ChoiceItem> items, const SegmentedControlOptions& options)
{
    float content = 0.0f;
    for (const ChoiceItem& item : items)
        content += SegmentWidth(item.label, options);
    internal::ControlStub("SegmentedControl", label, internal::StubWidth(options.width, content),
                          ControlHeight(options.size));
    return false;
}

bool SegmentedControl(const char* label, int*, std::span<const char* const> items,
                      const SegmentedControlOptions& options)
{
    float content = 0.0f;
    for (const char* item : items)
        content += SegmentWidth(item, options);
    internal::ControlStub("SegmentedControl", label, internal::StubWidth(options.width, content),
                          ControlHeight(options.size));
    return false;
}

} // namespace facet
