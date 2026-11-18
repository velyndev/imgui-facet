// Chip: planned. Draws the placeholder at the chip's default size until Phase 2 implements the
// widget in this file.

#include "facet/widgets/selection.h"

#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "widgets/controls_stub.h"

namespace facet
{

bool Chip(const char* label, const ChipOptions& options)
{
    namespace stub = internal::controls;
    const float height = ControlHeight(options.size);
    float content = stub::StubLabelWidth(label, options.size) + 2.0f * Px(T().spacing.sm);
    if (options.icon.IsValid() || options.color.w > 0.0f)
        content += Px(T().spacing.lg);
    if (options.removable)
        content += height;
    stub::ControlStub("Chip", label, content, height);
    return false;
}

bool ChipRemovePressed()
{
    return false;
}

} // namespace facet
