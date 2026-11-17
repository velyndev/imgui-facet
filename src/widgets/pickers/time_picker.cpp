// TimePicker: planned. Draws the placeholder at the field's default size until Phase 2
// implements the widget in this file on the popup host and the scroll core.

#include "facet/widgets/pickers.h"

#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "widgets/controls_stub.h"

namespace facet
{
namespace
{

// Visible rows of the inline columns.
constexpr float kColumnRows = 5.0f;

} // namespace

bool TimePicker(const char* label, TimeOfDay*, const TimePickerOptions& options)
{
    float height = internal::StubHeight(options.size, options.error, options.helpText);
    if (options.inlineColumns)
        height += kColumnRows * ControlHeight(options.size) + Px(T().spacing.sm);
    internal::ControlStub("TimePicker", label, internal::StubWidth(options.width, 0.0f), height);
    return false;
}

} // namespace facet
