// VectorInput: one to four drag fields side by side, one per component, each with its axis
// label in front as a drag handle, on the drag mode of the number core
// (src/internal/text_field_number.h). The optional link button is a slot button
// (TextFieldSlotButton) at the end of the last field. When the width leaves a field narrower than
// metrics.numberFieldWidth, the fields stack under each other instead.

#include "facet/widgets/inputs.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <type_traits>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/state_storage.h"
#include "internal/text_field.h"
#include "internal/text_field_number.h"
#include "widgets/inputs/number_value.h"
#include "widgets/inputs/value_inputs_common.h"

namespace facet
{
namespace
{

constexpr int kVectorMaxComponents = 4;
constexpr const char* kVectorAxes[kVectorMaxComponents] = {"X", "Y", "Z", "W"};

struct VectorInputState
{
    bool linked = false;
    ImGuiID sessionId = 0;                       // the active item of the edit the start belongs to
    double start[kVectorMaxComponents] = {};     // the components when that edit began
};

// The link button, in the trailing slot of the last field.
struct VectorLinkSlot
{
    bool* linked = nullptr;
    bool pressed = false;
};

void VectorLinkSlotFn(const internal::TextFieldSlot& slot, void* user)
{
    VectorLinkSlot& link = *static_cast<VectorLinkSlot*>(user);
    const internal::ItemState button = internal::TextFieldSlotButton(
        slot, 0, ImGui::GetIDWithSeed("link", nullptr, slot.fieldId), Icon("link"),
        {.tabStop = true, .selected = *link.linked, .disabled = slot.readOnly, .name = GetStrings().linkValues});
    link.pressed = button.pressed && !slot.readOnly;
}

template <class Value>
bool RunVectorInput(const char* label, Value* components, int count, const VectorInputOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    constexpr bool integer = std::is_integral_v<Value>;
    const internal::NextItemData next = internal::TakeNextItemData();
    const internal::ControlMetrics m = internal::GetControlMetrics(options.size);
    const Theme& t = T();
    const ImGuiID id = window->GetID(label);
    VectorInputState& state = internal::GetState<VectorInputState>(id);
    if (GImGui->ActiveId != state.sessionId)
        state.sessionId = 0;  // the edit the start belonged to has ended
    const int shown = std::clamp(count, 0, kVectorMaxComponents);
    const bool readOnly = options.readOnly || components == nullptr || count > kVectorMaxComponents;
    const bool disabled = ((next.itemFlags | GImGui->CurrentItemFlags) & ImGuiItemFlags_Disabled) != 0;

    // Layout: the label in front, then the fields; stacked when they would get too narrow.
    const float width = internal::ResolveItemWidth(options.width, next, kFill);
    const internal::FrontLabel front = internal::MeasureFrontLabel(label, m.size, width);
    const float area = std::max(1.0f, width - front.width);
    const float gap = std::round(Px(t.spacing.sm));
    const int fields = std::max(1, shown);
    float fieldWidth = std::floor((area - gap * static_cast<float>(fields - 1)) / static_cast<float>(fields));
    const bool stacked = fields > 1 && fieldWidth < std::round(Px(t.metrics.numberFieldWidth));
    if (stacked)
        fieldWidth = area;
    fieldWidth = std::max(1.0f, fieldWidth);

    ImGui::BeginGroup();
    const ImVec2 origin = window->DC.CursorPos;
    const char* error = options.error != nullptr ? "" : nullptr;  // the outline on each field, the message once
    bool written = false;
    ImGuiID firstId = 0;
    ImRect fieldsRect(origin.x + front.width, origin.y, origin.x + front.width, origin.y + m.height);
    VectorLinkSlot link{&state.linked};
    bool anyFocusVisible = false;

    double before[kVectorMaxComponents] = {};
    for (int i = 0; i < shown; ++i)
        before[i] = components != nullptr ? static_cast<double>(components[i]) : 0.0;
    int editedIndex = -1;
    double editedValue = 0.0;

    ImGui::PushOverrideID(id);
    for (int i = 0; i < shown; ++i)
    {
        const float x = stacked ? origin.x + front.width : origin.x + front.width + static_cast<float>(i) * (fieldWidth + gap);
        const float y = stacked ? origin.y + static_cast<float>(i) * (m.height + std::round(Px(t.spacing.xs))) : origin.y;
        ImGui::SetCursorScreenPos(ImVec2(x, y));
        const char* axis = i < static_cast<int>(options.axisLabels.size()) && options.axisLabels[i] != nullptr
                               ? options.axisLabels[i]
                               : kVectorAxes[i];
        char fieldLabel[64];
        std::snprintf(fieldLabel, sizeof(fieldLabel), "%s###%d", axis, i);

        internal::NumberFieldParams params;
        params.format = options.format;
        params.formatter = options.formatter;
        params.formatterUser = options.formatterUser;
        params.min = options.min;
        params.max = options.max;
        params.integer = integer;
        params.drag = true;
        params.speed = options.speed > 0.0 ? options.speed : (integer ? 1.0 : 0.0);
        params.step = integer ? 1.0 : (std::isfinite(options.min) && std::isfinite(options.max) && options.max > options.min
                                           ? (options.max - options.min) / 100.0
                                           : 0.1);
        params.stepFast = params.step * 10.0;
        params.size = options.size;
        params.width = internal::ValueInputUnits(fieldWidth);
        params.readOnly = readOnly;
        params.mixed = options.mixed;
        params.error = error;
        if (options.linkToggle && i == shown - 1)
        {
            params.trailingWidth = internal::TextFieldSlotButtonsWidth(1, options.size);
            params.trailingSlot = VectorLinkSlotFn;
            params.trailingUser = &link;
        }
        if (i == 0)
            internal::ApplyNextItemData(next);
        double value = before[i];
        const internal::NumberFieldResult result =
            internal::NumberFieldBehavior(fieldLabel, components != nullptr ? &value : nullptr, params);
        if (i == 0)
            firstId = result.field.id;
        fieldsRect.Add(result.field.frame);
        anyFocusVisible = anyFocusVisible || (ImGui::IsItemFocused() && internal::KeyboardModality());
        if (result.changed && components != nullptr && !readOnly && internal::NumberValueStore(value, &components[i]))
        {
            written = true;
            editedIndex = i;
            editedValue = static_cast<double>(components[i]);
        }
    }
    ImGui::PopID();

    // A locked vector scales every component by the factor of the edited one, measured from the
    // components at the start of the edit, so a long drag of whole numbers does not drift.
    if (editedIndex >= 0 && state.linked)
    {
        const ImGuiID activeId = GImGui->ActiveId;
        if (activeId == 0 || activeId != state.sessionId)
        {
            state.sessionId = activeId;
            for (int j = 0; j < shown; ++j)
                state.start[j] = before[j];
        }
        if (state.start[editedIndex] != 0.0)
        {
            const double factor = editedValue / state.start[editedIndex];
            for (int j = 0; j < shown; ++j)
                if (j != editedIndex)
                    internal::NumberValueStore(std::clamp(state.start[j] * factor, std::min(options.min, options.max),
                                                          std::max(options.min, options.max)),
                                               &components[j]);
        }
    }
    // normalize: the edited vector is brought back to length 1 (float components).
    if (editedIndex >= 0 && options.normalize && !integer)
    {
        double length = 0.0;
        for (int j = 0; j < shown; ++j)
            length += static_cast<double>(components[j]) * static_cast<double>(components[j]);
        length = std::sqrt(length);
        if (length > 0.0 && std::isfinite(length))
            for (int j = 0; j < shown; ++j)
                internal::NumberValueStore(static_cast<double>(components[j]) / length, &components[j]);
    }
    if (link.pressed)
        state.linked = !state.linked;

    // The visible label in front of the first line, then one message line under the fields.
    ImDrawList* drawList = window->DrawList;
    if (front.text != nullptr)
    {
        internal::ControlTextScope text(m);
        internal::DrawFrontLabel(drawList, front, ImVec2(origin.x, CenteredTextY(origin.y, m.height)), m.size, disabled);
        const ImRect labelRect(origin, ImVec2(origin.x + front.width, origin.y + m.height));
        internal::CutTextTooltip(id, labelRect, ImGui::IsMouseHoveringRect(labelRect.Min, labelRect.Max), anyFocusVisible,
                                 front.text, front.end, front.shown.truncated);
    }
    const internal::ControlMessage message = internal::PickControlMessage(options.error, options.helpText);
    internal::DrawControlMessage(message, ImVec2(fieldsRect.Min.x, fieldsRect.Max.y), fieldsRect.GetWidth(), disabled,
                                 firstId, fieldsRect);
    window->DC.CursorMaxPos.x = std::max(window->DC.CursorMaxPos.x, origin.x + width);
    ImGui::EndGroup();
    internal::FinishWidgetGroup(id);
    return written;
}

} // namespace

bool VectorInput(const char* label, std::span<float> components, const VectorInputOptions& options)
{
    return RunVectorInput(label, components.data(), static_cast<int>(components.size()), options);
}

bool VectorInput(const char* label, std::span<int> components, const VectorInputOptions& options)
{
    return RunVectorInput(label, components.data(), static_cast<int>(components.size()), options);
}

bool VectorInput(const char* label, ImVec2* value, const VectorInputOptions& options)
{
    float components[2] = {value != nullptr ? value->x : 0.0f, value != nullptr ? value->y : 0.0f};
    const bool written = RunVectorInput(label, value != nullptr ? components : nullptr, 2, options);
    if (written && value != nullptr)
        *value = ImVec2(components[0], components[1]);
    return written;
}

bool VectorInput(const char* label, ImVec4* value, const VectorInputOptions& options)
{
    float components[4] = {};
    if (value != nullptr)
    {
        components[0] = value->x;
        components[1] = value->y;
        components[2] = value->z;
        components[3] = value->w;
    }
    const bool written = RunVectorInput(label, value != nullptr ? components : nullptr, 4, options);
    if (written && value != nullptr)
        *value = ImVec4(components[0], components[1], components[2], components[3]);
    return written;
}

} // namespace facet
