// DatePicker: planned. Draws the placeholder at the field's default size until Phase 2
// implements the widget in this file on the popup host.

#include "facet/widgets/pickers.h"

#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "widgets/controls_stub.h"

namespace facet
{
namespace
{

// The inline calendar: a header row, the weekday row and six weeks, seven days wide.
constexpr float kCalendarRows = 8.0f;
constexpr float kCalendarColumns = 7.0f;

} // namespace

bool DatePicker(const char* label, Date*, const DatePickerOptions& options)
{
    float width = internal::StubWidth(options.width, 0.0f);
    float height = internal::StubHeight(options.size, options.error, options.helpText);
    if (options.inlineCalendar)
    {
        const float cell = ControlHeight(options.size);
        width = internal::StubFittedWidth(options.width, kCalendarColumns * cell);
        height += kCalendarRows * cell + Px(T().spacing.sm);
    }
    internal::ControlStub("DatePicker", label, width, height);
    return false;
}

} // namespace facet
