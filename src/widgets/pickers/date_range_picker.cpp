// DateRangePicker: planned. Draws the placeholder at the field's default size until Phase 2
// implements the widget in this file on the popup host.

#include "facet/widgets/pickers.h"

#include "widgets/controls_stub.h"

namespace facet
{

bool DateRangePicker(const char* label, Date*, Date*, const DateRangePickerOptions& options)
{
    namespace stub = internal::controls;
    stub::ControlStub("DateRangePicker", label, stub::StubWidth(options.width, 0.0f),
                          stub::StubHeight(options.size, options.error, options.helpText));
    return false;
}

} // namespace facet
