// RangeInput: the two-handle track of RangeSlider (range_core.h) between a low and a high number
// field from the number core, or the two fields alone. A typed value follows the rules of a dragged
// handle: order, minimum gap and collision. When the track beside the fields would get too short,
// the fields drop under it (low at its start, high at its end). Tab order: low field, low handle,
// high handle, high field.

#include "facet/widgets/sliders.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <type_traits>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/item.h"
#include "internal/text_field_number.h"
#include "widgets/sliders/range_core.h"
#include "widgets/sliders/slider_core.h"

namespace facet
{
namespace
{

template <class Value>
bool RangeInputImpl(const char* label, Value* low, Value* high, double min, double max, bool integer,
                    const RangeInputOptions& o)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const internal::NextItemData next = internal::TakeNextItemData();
    const internal::SliderMetrics m = internal::GetSliderMetrics(o.size);
    internal::ControlTextScope text(m.control);
    if (min > max)
        std::swap(min, max);
    const ImGuiID id = window->GetID(label);
    const bool missing = low == nullptr || high == nullptr;
    const double raw[2] = {low != nullptr ? internal::SliderValueIn(*low) : min,
                           high != nullptr ? internal::SliderValueIn(*high) : max};

    const float width = internal::ResolveItemWidth(o.width, next, kFill);
    const float gap = PxSnapped(T().spacing.sm);
    const float fieldWidth = std::round(Px(o.fieldWidth > 0.0f ? o.fieldWidth : T().metrics.numberFieldWidth));
    const ImVec2 origin(std::round(window->DC.CursorPos.x), std::round(window->DC.CursorPos.y));
    internal::FrontLabel front = internal::MeasureFrontLabel(label, m.control.size, width);
    front.width = std::round(front.width);
    const float rest = width - front.width;
    const float inlineTrack = rest - (fieldWidth + gap) * 2.0f;
    const bool stacked = o.slider && inlineTrack < Px(T().layout.minRowControl);
    const float height = m.control.height;

    // Rows: the fields beside the track, or the track on the first row and both fields under it.
    const float fieldsY = stacked ? origin.y + height + PxSnapped(T().spacing.xs) : origin.y;
    const ImVec2 lowField(origin.x + front.width, fieldsY);
    const float highFieldX = origin.x + width - std::min(fieldWidth, std::max(1.0f, rest * 0.5f - gap * 0.5f));
    const float fieldW = std::min(fieldWidth, std::max(1.0f, rest * 0.5f - gap * 0.5f));
    const ImVec2 trackOrigin = stacked ? ImVec2(origin.x + front.width, origin.y)
                                       : ImVec2(lowField.x + fieldW + gap, origin.y);
    const float trackWidth = stacked ? rest : std::max(1.0f, highFieldX - gap - trackOrigin.x);
    const internal::SliderFrame frame = internal::LayoutSliderFrame(trackOrigin, "##track", trackWidth, 0.0f, m,
                                                                    Orientation::Horizontal, 0.0f);
    const ImRect widget(origin, ImVec2(origin.x + width, fieldsY + height));

    internal::RangeTrackParams params;
    params.format = o.format;
    params.formatter = o.formatter;
    params.formatterUser = o.formatterUser;
    params.scale = o.scale;
    params.step = o.step;
    params.minGap = o.minGap;
    params.collision = o.collision;
    params.dragSpan = o.dragSpan;
    params.allowOutOfRange = o.allowOutOfRange;
    params.doubleClickReset = o.doubleClickReset;
    params.size = o.size;
    params.integer = integer;
    params.readOnly = o.readOnly || missing;
    params.mixed = o.mixed;
    params.error = o.error != nullptr;

    internal::ValueMapping mapping;
    mapping.min = min;
    mapping.max = max;
    mapping.scale = o.scale;
    mapping.step = o.step;
    mapping.integer = integer;
    mapping = internal::NormalizeMapping(mapping);
    const internal::HandleConstraints constraints = internal::RangeConstraints(params, mapping);
    const bool readOnly = params.readOnly || !(mapping.max > mapping.min) || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    const bool disabled = (next.itemFlags & ImGuiItemFlags_Disabled) != 0;

    // The values as drawn: repaired, never written unless edited.
    double drawn[2] = {std::isnan(raw[0]) ? mapping.min : raw[0], std::isnan(raw[1]) ? mapping.max : raw[1]};
    internal::ConstrainHandles(std::span<double>(drawn, 2), constraints);
    double out[2] = {raw[0], raw[1]};
    bool written[2] = {false, false};

    ImGui::BeginGroup();
    ImGui::ItemSize(widget.GetSize());

    internal::NumberFieldParams field;
    field.format = o.format;
    field.formatter = o.formatter;
    field.formatterUser = o.formatterUser;
    if (o.allowOutOfRange)
    {
        field.min = -std::numeric_limits<double>::infinity();
        field.max = std::numeric_limits<double>::infinity();
    }
    else
    {
        field.min = mapping.min;
        field.max = mapping.max;
    }
    field.integer = integer;
    field.step = o.step > 0.0 ? o.step : (integer ? 1.0 : (mapping.max - mapping.min) / internal::kKeyboardGrid);
    field.stepFast = field.step * 10.0;
    field.size = o.size;
    field.readOnly = readOnly;
    field.mixed = o.mixed;
    // A typed value moves its handle by the rules of the track.
    const auto runField = [&](int index, ImVec2 at) {
        ImGui::SetCursorScreenPos(at);
        ImGui::SetNextItemWidth(fieldW);
        ImGui::PushOverrideID(id);
        double typed = out[index];
        const internal::NumberFieldResult edit =
            internal::NumberFieldBehavior(index == 0 ? "##low" : "##high", &typed, field);
        ImGui::PopID();
        if (!edit.changed || readOnly || disabled)
            return;
        double values[2] = {drawn[0], drawn[1]};
        if (internal::ApplyTypedHandleValue(values, index, typed, mapping, constraints, o.allowOutOfRange))
        {
            for (int i = 0; i < 2; ++i)
            {
                if (values[i] != drawn[i])
                {
                    out[i] = values[i];
                    written[i] = true;
                }
            }
        }
    };
    runField(0, lowField);

    if (o.slider)
    {
        const internal::RangeTrackResult r =
            internal::RangeTrackBehavior(id, frame, m, written[0] ? out[0] : raw[0], written[1] ? out[1] : raw[1], min, max, params);
        for (int i = 0; i < 2; ++i)
        {
            if (r.written[i])
            {
                out[i] = r.values[i];
                written[i] = true;
            }
        }
    }
    else
    {
        // Without the track: a short bar between the two fields.
        const float y = std::round(origin.y + height * 0.5f);
        const float barWidth = PxSnapped(T().spacing.sm);
        const float cx = std::round((lowField.x + fieldW + highFieldX) * 0.5f);
        window->DrawList->AddRectFilled(ImVec2(cx - barWidth * 0.5f, y), ImVec2(cx + barWidth * 0.5f, y + HairlineThickness()),
                                        ToU32(internal::ControlSecondaryColor(disabled)));
    }
    runField(1, ImVec2(highFieldX, fieldsY));

    if (front.text != nullptr)
        internal::DrawFrontLabel(window->DrawList, front, ImVec2(origin.x, CenteredTextY(origin.y, height)), m.control.size,
                                 disabled);
    const char* labelEnd = ImGui::FindRenderedTextEnd(label);
    internal::CutTextTooltip(id, widget, ImGui::IsMouseHoveringRect(widget.Min, widget.Max) && ImGui::IsWindowHovered(),
                             false, label, labelEnd, front.shown.truncated);

    const internal::ControlMessage message = internal::PickControlMessage(o.error, o.helpText);
    if (message.IsSet())
        internal::DrawControlMessage(message, ImVec2(lowField.x, widget.Max.y), widget.Max.x - lowField.x, disabled, id,
                                     widget);
    ImGui::EndGroup();
    internal::FinishWidgetGroup(id);

    if (missing || (!written[0] && !written[1]))
        return false;
    Value* targets[2] = {low, high};
    for (int i = 0; i < 2; ++i)
    {
        if (!written[i])
            continue;
        if constexpr (std::is_integral_v<Value>)
            *targets[i] = static_cast<Value>(std::llround(out[i]));
        else
            *targets[i] = static_cast<Value>(out[i]);
    }
    return true;
}

} // namespace

bool RangeInput(const char* label, float* low, float* high, float min, float max, const RangeInputOptions& options)
{
    return RangeInputImpl(label, low, high, internal::SliderFloatToDouble(min), internal::SliderFloatToDouble(max),
                          false, options);
}

bool RangeInput(const char* label, int* low, int* high, int min, int max, const RangeInputOptions& options)
{
    return RangeInputImpl(label, low, high, static_cast<double>(min), static_cast<double>(max), true, options);
}

} // namespace facet
