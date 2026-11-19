// ColorPicker: planned. Draws the placeholder at the picker's default size until Phase 2
// implements the widget in this file.

#include "facet/widgets/pickers.h"

#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "widgets/controls_stub.h"

namespace facet
{

bool ColorPicker(const char* label, ImVec4*, const ColorPickerOptions& options)
{
    namespace stub = internal::controls;
    const Theme& t = T();
    const float available = stub::StubWidth(kFill, 0.0f);
    const float preferred = Px(t.metrics.pickerWidth);
    const float width = stub::StubWidth(options.width, available < preferred ? available : preferred);
    const float control = ControlHeight(ControlSize::Inherit);
    // The square, then the hue and alpha bars, the inputs and the swatches, each on its own row.
    float height = width + control + Px(t.spacing.sm);
    if (options.alpha)
        height += control + Px(t.spacing.sm);
    if (options.inputs)
        height += control + Px(t.spacing.sm);
    if (!options.swatches.empty())
        height += control + Px(t.spacing.sm);
    stub::ControlStub("ColorPicker", label, width, height);
    return false;
}

} // namespace facet
