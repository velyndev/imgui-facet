// DragNumber: a number field that changes when dragged sideways, on the drag mode of the number
// core (src/internal/text_field_number.h): the pointer travel since the press sets the value, so
// a drag back to the start lands exactly on the start value; a click without travel, Enter or
// Space types a value. Left and Right step while the field is focused; Up and Down while typing.

#include "facet/widgets/inputs.h"

#include <cmath>
#include <type_traits>
#include <utility>

#include <imgui.h>
#include <imgui_internal.h>

#include "internal/control.h"
#include "internal/text_field_number.h"
#include "widgets/inputs/number_value.h"
#include "widgets/inputs/value_inputs_common.h"

namespace facet
{
namespace
{

// Keyboard steps of a finite range: a hundredth of it, as the value drag core steps a slider.
constexpr double kDragKeySteps = 100.0;
// Keyboard steps of an open range move the value as far as this many units of pointer travel.
constexpr double kDragKeyTravel = 10.0;
// Ctrl, Page Up and Page Down step this many keyboard steps.
constexpr double kDragFastSteps = 10.0;

template <class T>
bool RunDragNumber(const char* label, T* value, const DragNumberOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    constexpr bool integer = std::is_integral_v<T>;
    const ImGuiID id = window->GetID(label);

    // The core gets the caller's range as it is: an open range keeps the speed derived from the
    // step instead of the whole range of T. The type's own limits are applied on the way back.
    double low = std::isnan(options.min) ? -HUGE_VAL : options.min;
    double high = std::isnan(options.max) ? HUGE_VAL : options.max;
    if (low > high)
        std::swap(low, high);
    const bool finite = std::isfinite(low) && std::isfinite(high) && high > low;

    double speed = options.speed > 0.0 && std::isfinite(options.speed) ? options.speed : 0.0;
    if (integer && speed <= 0.0 && !finite)
        speed = 1.0;
    double step = finite ? (high - low) / kDragKeySteps : (speed > 0.0 ? speed * kDragKeyTravel : 1.0);
    step = internal::NumberValueStep<T>(step);

    internal::NumberFieldParams params;
    params.format = options.format;
    params.formatter = options.formatter;
    params.formatterUser = options.formatterUser;
    params.min = low;
    params.max = high;
    params.integer = integer;
    params.step = step;
    params.stepFast = step * kDragFastSteps;
    params.drag = true;
    params.speed = speed;
    params.scale = options.scale;
    params.wrap = options.wrap && finite;
    params.unit = options.unit;
    params.size = options.size;
    params.width = options.width;
    params.readOnly = options.readOnly;
    params.mixed = options.mixed;
    params.error = internal::ValueInputError(id, options.error);
    params.helpText = options.helpText;

    double edited = value != nullptr ? static_cast<double>(*value) : 0.0;
    const internal::NumberFieldResult result =
        internal::NumberFieldBehavior(label, value != nullptr ? &edited : nullptr, params);
    const bool written = result.changed && internal::NumberValueStore(edited, value);
    internal::ValueInputTrackRejected(id, result.parseFailed, written);
    return written;
}

} // namespace

bool DragNumber(const char* label, int* value, const DragNumberOptions& options)
{
    return RunDragNumber(label, value, options);
}

bool DragNumber(const char* label, float* value, const DragNumberOptions& options)
{
    return RunDragNumber(label, value, options);
}

bool DragNumber(const char* label, double* value, const DragNumberOptions& options)
{
    return RunDragNumber(label, value, options);
}

} // namespace facet
