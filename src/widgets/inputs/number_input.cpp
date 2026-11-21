// NumberInput: a typed number field with - and + steppers, limits and a unit suffix, on the
// number core (src/internal/text_field_number.h), which types, parses, steps and draws. This
// file maps the options onto the core and the edited double back onto the caller's type: an
// int stays inside the int range and a float inside the float range, so a typed "99999999999"
// or a step past the end never overflows.

#include "facet/widgets/inputs.h"

#include <type_traits>

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

template <class T>
bool RunNumberInput(const char* label, T* value, const NumberInputOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const ImGuiID id = window->GetID(label);

    // The limits inside the range of T, so the steppers also stop at the end of the type.
    double low = options.min;
    double high = options.max;
    internal::NumberValueClampRange<T>(&low, &high);

    internal::NumberFieldParams params;
    params.format = options.format;
    params.formatter = options.formatter;
    params.formatterUser = options.formatterUser;
    params.min = low;
    params.max = high;
    params.integer = std::is_integral_v<T>;
    params.step = internal::NumberValueStep<T>(options.step);
    params.stepFast = internal::NumberValueStep<T>(options.stepFast);
    params.steppers = params.step > 0.0;
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

bool NumberInput(const char* label, int* value, const NumberInputOptions& options)
{
    return RunNumberInput(label, value, options);
}

bool NumberInput(const char* label, float* value, const NumberInputOptions& options)
{
    return RunNumberInput(label, value, options);
}

bool NumberInput(const char* label, double* value, const NumberInputOptions& options)
{
    return RunNumberInput(label, value, options);
}

} // namespace facet
