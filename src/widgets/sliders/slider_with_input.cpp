// SliderWithInput: a Slider (without its value column) and a number field from the number core
// bound to the same value, side by side; when the row is narrow the field drops below the track.
// The slider writes while dragging, the field on Enter, Tab or focus loss; with allowOutOfRange
// the field accepts values outside the range, which the slider shows clamped.

#include "facet/widgets/sliders.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <type_traits>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/item.h"
#include "internal/row_layout.h"
#include "internal/text_field_number.h"
#include "widgets/sliders/slider_core.h"

namespace facet
{
namespace
{

template <class Value>
bool SliderWithInputImpl(const char* label, Value* value, double min, double max, bool integer,
                         const SliderWithInputOptions& o)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const internal::NextItemData next = internal::TakeNextItemData();
    const internal::SliderMetrics m = internal::GetSliderMetrics(o.size);
    if (min > max)
        std::swap(min, max);
    const ImGuiID id = window->GetID(label);
    const float width = internal::ResolveItemWidth(o.width, next, kFill);
    const float gap = PxSnapped(T().spacing.sm);
    const float fieldWidth = std::round(Px(o.fieldWidth > 0.0f ? o.fieldWidth : T().metrics.numberFieldWidth));
    const internal::FrontLabel front = internal::MeasureFrontLabel(label, m.control.size, width);
    // Beside the track while the track keeps the narrowest control length of a labelled row.
    const bool stacked = width - front.width - fieldWidth - gap < Px(T().layout.minRowControl);
    // Beside the track the field sits at the end of a trailing column shared with the sliders above
    // and below (row_layout.h), so the tracks of one column end at one x.
    const float column = stacked ? 0.0f
                                 : std::min(width - front.width - Px(T().layout.minRowControl),
                                            internal::RowTrailingColumn(std::round(window->DC.CursorPos.x) + width,
                                                                        fieldWidth + gap, internal::kSliderColumnField));
    const float trailing = std::max(fieldWidth + gap, column);
    const float sliderWidth = stacked ? width : std::max(1.0f, width - trailing);

    double edited = value != nullptr ? internal::SliderValueIn(*value) : min;
    const bool readOnly = o.readOnly || value == nullptr;
    bool written = false;

    ImGui::BeginGroup();
    SliderOptions slider;
    slider.format = o.format;
    slider.formatter = o.formatter;
    slider.formatterUser = o.formatterUser;
    slider.scale = o.scale;
    slider.step = o.step;
    slider.size = o.size;
    slider.readOnly = readOnly;
    slider.mixed = o.mixed;
    internal::SliderBodyParams body;
    body.valueColumn = false;
    body.width = sliderWidth;
    body.errorLook = o.error != nullptr;
    body.next = &next;
    ImRect control;
    body.controlRect = &control;
    // The slider shows an out-of-range value clamped and writes only values inside the range.
    double shown = edited;
    if (internal::SliderBody(label, value != nullptr ? &shown : nullptr, min, max, integer, slider, body))
    {
        edited = shown;
        written = true;
    }

    // The field, under the widget id.
    if (stacked)
        ImGui::SetCursorScreenPos(ImVec2(control.Min.x, ImGui::GetCursorScreenPos().y));
    else
        ImGui::SameLine(0.0f, trailing - fieldWidth);
    internal::NumberFieldParams field;
    field.format = o.format;
    field.formatter = o.formatter;
    field.formatterUser = o.formatterUser;
    if (!o.allowOutOfRange)
    {
        field.min = min;
        field.max = max;
    }
    else
    {
        field.min = -std::numeric_limits<double>::infinity();
        field.max = std::numeric_limits<double>::infinity();
    }
    field.integer = integer;
    // Up and Down in the field step like the slider's arrow keys.
    field.step = o.step > 0.0 ? o.step : (integer ? 1.0 : (max - min) / internal::kKeyboardGrid);
    field.stepFast = field.step * 10.0;
    field.size = o.size;
    field.readOnly = readOnly;
    field.mixed = o.mixed && !written;
    ImGui::PushOverrideID(id);
    ImGui::SetNextItemWidth(fieldWidth);
    double typed = edited;
    const internal::NumberFieldResult edit = internal::NumberFieldBehavior("##field", &typed, field);
    ImGui::PopID();
    if (edit.changed && !readOnly && typed != edited)
    {
        edited = typed;
        written = true;
    }
    const bool disabled = (next.itemFlags & ImGuiItemFlags_Disabled) != 0;
    const internal::ControlMessage message = internal::PickControlMessage(o.error, o.helpText);
    if (message.IsSet())
    {
        const float bottom = ImGui::GetItemRectMax().y;
        const float right = control.Min.x + width - front.width;
        internal::DrawControlMessage(message, ImVec2(control.Min.x, bottom), right - control.Min.x, disabled, id,
                                     ImRect(control.Min, ImVec2(right, bottom)));
    }
    ImGui::EndGroup();
    internal::FinishWidgetGroup(id);

    if (!written || value == nullptr)
        return false;
    if constexpr (std::is_integral_v<Value>)
        *value = static_cast<Value>(std::llround(edited));
    else
        *value = static_cast<Value>(edited);
    return true;
}

} // namespace

bool SliderWithInput(const char* label, float* value, float min, float max, const SliderWithInputOptions& options)
{
    return SliderWithInputImpl(label, value, internal::SliderFloatToDouble(min), internal::SliderFloatToDouble(max),
                               false, options);
}

bool SliderWithInput(const char* label, int* value, int min, int max, const SliderWithInputOptions& options)
{
    return SliderWithInputImpl(label, value, static_cast<double>(min), static_cast<double>(max), true, options);
}

} // namespace facet
