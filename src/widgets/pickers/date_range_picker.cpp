// DateRangePicker: planned. Draws the placeholder at the field's default size until Phase 2
// implements the widget in this file on the popup host.

#include "facet/widgets/pickers.h"

#include "widgets/controls_stub.h"

namespace facet
{

bool DateRangePicker(const char* label, Date*, Date*, const DateRangePickerOptions& options)
{
    internal::ControlStub("DateRangePicker", label, internal::StubWidth(options.width, 0.0f),
                          internal::StubHeight(options.size, options.error, options.helpText));
    return false;
}

} // namespace facet
