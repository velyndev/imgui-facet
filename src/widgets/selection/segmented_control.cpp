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
    namespace stub = internal::controls;
    if (options.iconOnly)
        return ControlHeight(options.size);
    return stub::StubLabelWidth(text, options.size) + 2.0f * Px(T().spacing.sm);
}

void Stub(const char* label, float content, const SegmentedControlOptions& options)
{
    namespace stub = internal::controls;
    // Labels collapse to icons when the row is short, so the control never outgrows it.
    stub::ControlStub("SegmentedControl", label, stub::StubFittedWidth(options.width, content),
                          ControlHeight(options.size));
}

} // namespace

bool SegmentedControl(const char* label, int*, std::span<const ChoiceItem> items, const SegmentedControlOptions& options)
{
    float content = 0.0f;
    for (const ChoiceItem& item : items)
        content += SegmentWidth(item.label, options);
    Stub(label, content, options);
    return false;
}

bool SegmentedControl(const char* label, int*, std::span<const char* const> items,
                      const SegmentedControlOptions& options)
{
    float content = 0.0f;
    for (const char* item : items)
        content += SegmentWidth(item, options);
    Stub(label, content, options);
    return false;
}

} // namespace facet
