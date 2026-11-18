// AnglePicker: planned. Draws the placeholder at the picker's default size until Phase 2
// implements the widget in this file.

#include "facet/widgets/pickers.h"

#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "widgets/controls_stub.h"

namespace facet
{
namespace
{

// Width of the degrees field beside the dial, in spacing.xxxl.
constexpr float kFieldInXxxl = 2.5f;

} // namespace

bool AnglePicker(const char* label, float*, const AnglePickerOptions& options)
{
    namespace stub = internal::controls;
    const Theme& t = T();
    const float diameter = stub::StubDial(options.diameter, options.size);
    float content = stub::StubLabelWidth(label, options.size) + diameter;
    if (options.field)
        content += Px(t.spacing.sm) + Px(t.spacing.xxxl) * kFieldInXxxl;
    stub::ControlStub("AnglePicker", label, stub::StubFittedWidth(options.width, content),
                          diameter + stub::StubMessageHeight(options.error, options.helpText));
    return false;
}

} // namespace facet
