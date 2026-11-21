// SizeInput: width and height as two typed number fields (the number core,
// src/internal/text_field_number.h) side by side after the label. The aspect lock is a slot
// button at the end of the width field, so it stands between the two values; the swap and preset
// buttons are slot buttons at the end of the height field. Every button is a Tab stop after the
// text of its field, so Tab goes width, lock, height, swap, presets. The presets menu is a popup
// level with menu rows (popup_menu.h).

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
#include "internal/popup_host.h"
#include "internal/popup_menu.h"
#include "internal/state_storage.h"
#include "internal/text_field.h"
#include "internal/text_field_number.h"
#include "widgets/inputs/number_value.h"
#include "widgets/inputs/value_inputs_common.h"

namespace facet
{
namespace
{

using internal::ForcedItemState;

struct SizeInputState
{
    bool locked = false;
    double ratio = 0.0;  // width / height while locked; 0 when the values had no ratio
};

// What the slot buttons report back to the widget.
struct SizeInputSlots
{
    const SizeInputOptions* options = nullptr;
    bool locked = false;
    bool lockPressed = false;
    bool swapPressed = false;
    bool presetsPressed = false;
    ImGuiID presetsId = 0;
    ImRect presetsRect;
    ForcedItemState presetsForced = ForcedItemState::None;
};

void SizeLockSlot(const internal::TextFieldSlot& slot, void* user)
{
    SizeInputSlots& slots = *static_cast<SizeInputSlots*>(user);
    const internal::ItemState lock = internal::TextFieldSlotButton(
        slot, 0, ImGui::GetIDWithSeed("lock", nullptr, slot.fieldId), Icon(slots.locked ? "lock" : "unlock"),
        {.tabStop = true, .selected = slots.locked, .disabled = slot.readOnly, .name = GetStrings().aspectLock});
    slots.lockPressed = lock.pressed && !slot.readOnly;
}

void SizeHeightSlot(const internal::TextFieldSlot& slot, void* user)
{
    SizeInputSlots& slots = *static_cast<SizeInputSlots*>(user);
    // Submitted from the left, so Tab meets them in their visual order.
    const bool presets = !slots.options->presets.empty();
    if (slots.options->swapButton)
    {
        const internal::ItemState swap = internal::TextFieldSlotButton(
            slot, presets ? 1 : 0, ImGui::GetIDWithSeed("swap", nullptr, slot.fieldId), Icon("refresh"),
            {.tabStop = true, .disabled = slot.readOnly, .name = GetStrings().swap});
        slots.swapPressed = swap.pressed && !slot.readOnly;
    }
    if (presets)
    {
        const internal::ItemState menu = internal::TextFieldSlotButton(
            slot, 0, slots.presetsId, Icon("chevron-down"),
            {.tabStop = true, .disabled = slot.readOnly, .name = GetStrings().presets});
        slots.presetsPressed = menu.pressed && !slot.readOnly;
        slots.presetsRect = menu.rect;
        slots.presetsForced = menu.forced;
    }
}

// "1920 × 1080" in the field format, for a preset row.
void SizePresetText(char* out, int size, ImVec2 preset, const SizeInputOptions& options, bool integer)
{
    char width[32];
    char height[32];
    internal::FormatNumber(width, sizeof(width), preset.x, options.format, options.formatter, options.formatterUser,
                           integer, false);
    internal::FormatNumber(height, sizeof(height), preset.y, options.format, options.formatter, options.formatterUser,
                           integer, false);
    std::snprintf(out, static_cast<std::size_t>(size), "%s \xC3\x97 %s", width, height);
}

template <class Value>
bool RunSizeInput(const char* label, Value* x, Value* y, const SizeInputOptions& options)
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
    SizeInputState& state = internal::GetState<SizeInputState>(id);
    const bool readOnly = options.readOnly || x == nullptr || y == nullptr;
    const bool disabled = ((next.itemFlags | GImGui->CurrentItemFlags) & ImGuiItemFlags_Disabled) != 0;

    // Layout: the label in front, the two fields sharing the rest; stacked when too narrow.
    const float width = internal::ResolveItemWidth(options.width, next, kFill);
    const internal::FrontLabel front = internal::MeasureFrontLabel(label, m.size, width);
    const float area = std::max(1.0f, width - front.width);
    const float gap = std::round(Px(t.spacing.sm));
    float fieldWidth = std::floor((area - gap) * 0.5f);
    const bool stacked = fieldWidth < std::round(Px(t.metrics.numberFieldWidth));
    if (stacked)
        fieldWidth = area;
    fieldWidth = std::max(1.0f, fieldWidth);

    SizeInputSlots slots;
    slots.options = &options;
    slots.locked = options.aspectLock && state.locked;
    slots.presetsId = ImGui::GetIDWithSeed("presets", nullptr, id);
    const char* error = options.error != nullptr ? "" : internal::ValueInputError(id, nullptr) != nullptr ? "" : nullptr;

    ImGui::BeginGroup();
    const ImVec2 origin = window->DC.CursorPos;
    double values[2] = {x != nullptr ? static_cast<double>(*x) : 0.0, y != nullptr ? static_cast<double>(*y) : 0.0};
    const double before[2] = {values[0], values[1]};
    const double mins[2] = {options.min.x, options.min.y};
    const double maxs[2] = {options.max.x, options.max.y};
    Value* targets[2] = {x, y};
    int edited = -1;
    bool written = false;
    bool parseFailed = false;
    bool activated = false;
    ImGuiID firstId = 0;
    ImRect fieldsRect(origin.x + front.width, origin.y, origin.x + front.width, origin.y + m.height);
    bool anyFocusVisible = false;

    ImGui::PushOverrideID(id);
    for (int i = 0; i < 2; ++i)
    {
        const float fx = stacked || i == 0 ? origin.x + front.width : origin.x + front.width + fieldWidth + gap;
        const float fy = stacked && i == 1 ? origin.y + m.height + std::round(Px(t.spacing.xs)) : origin.y;
        ImGui::SetCursorScreenPos(ImVec2(fx, fy));
        internal::NumberFieldParams params;
        params.format = options.format;
        params.formatter = options.formatter;
        params.formatterUser = options.formatterUser;
        double low = mins[i];
        double high = maxs[i];
        internal::NumberValueClampRange<Value>(&low, &high);
        params.min = low;
        params.max = high;
        params.integer = integer;
        params.step = internal::NumberValueStep<Value>(options.step);
        params.stepFast = params.step * 10.0;
        params.size = options.size;
        params.width = internal::ValueInputUnits(fieldWidth);
        params.readOnly = readOnly;
        params.mixed = options.mixed;
        params.error = error;
        if (i == 0 && options.aspectLock)
        {
            params.trailingWidth = internal::TextFieldSlotButtonsWidth(1, options.size);
            params.trailingSlot = SizeLockSlot;
            params.trailingUser = &slots;
        }
        const int heightButtons = (options.swapButton ? 1 : 0) + (options.presets.empty() ? 0 : 1);
        if (i == 1 && heightButtons > 0)
        {
            params.trailingWidth = internal::TextFieldSlotButtonsWidth(heightButtons, options.size);
            params.trailingSlot = SizeHeightSlot;
            params.trailingUser = &slots;
        }
        if (i == 0)
            internal::ApplyNextItemData(next);
        double value = values[i];
        const internal::NumberFieldResult result = internal::NumberFieldBehavior(
            i == 0 ? "##width" : "##height", readOnly ? nullptr : &value, params);
        if (i == 0)
            firstId = result.field.id;
        fieldsRect.Add(result.field.frame);
        parseFailed = parseFailed || result.parseFailed;
        activated = activated || ImGui::IsItemActivated();
        anyFocusVisible = anyFocusVisible || (ImGui::IsItemFocused() && internal::KeyboardModality());
        if (result.changed && !readOnly && internal::NumberValueStore(value, targets[i]))
        {
            written = true;
            edited = i;
            values[i] = static_cast<double>(*targets[i]);
        }
    }
    ImGui::PopID();

    // The lock writes the other value in the same frame.
    if (edited >= 0 && slots.locked)
    {
        const int other = 1 - edited;
        double ratio = state.ratio;
        if (!(ratio > 0.0) || !std::isfinite(ratio))
            ratio = before[1] != 0.0 ? before[0] / before[1] : 0.0;
        if (ratio > 0.0 && std::isfinite(ratio))
        {
            double low = mins[other];
            double high = maxs[other];
            internal::NumberValueClampRange<Value>(&low, &high);
            const double linked = edited == 0 ? values[0] / ratio : values[1] * ratio;
            internal::NumberValueStore(std::clamp(linked, low, high), targets[other]);
        }
    }
    if (slots.lockPressed)
    {
        state.locked = !state.locked;
        state.ratio = state.locked && values[1] != 0.0 ? values[0] / values[1] : 0.0;
    }
    if (slots.swapPressed && !readOnly)
    {
        double low = 0.0;
        double high = 0.0;
        for (int i = 0; i < 2; ++i)
        {
            low = mins[i];
            high = maxs[i];
            internal::NumberValueClampRange<Value>(&low, &high);
            written |= internal::NumberValueStore(std::clamp(values[1 - i], low, high), targets[i]);
        }
        if (state.ratio > 0.0)
            state.ratio = 1.0 / state.ratio;
    }

    // The presets menu.
    if (!options.presets.empty())
    {
        const internal::PopupAnchor anchor{.rect = slots.presetsRect, .side = internal::PopupSide::Below,
                                           .align = internal::PopupAlign::End};
        if (slots.presetsPressed)
            internal::OpenPopupLevel(slots.presetsId, anchor, {.minWidth = Px(t.overlay.menuMinWidth)});
        if (Has(slots.presetsForced, ForcedItemState::Open))
            internal::ForcePopupLevelOpen(slots.presetsId, anchor, {.takeFocus = false, .minWidth = Px(t.overlay.menuMinWidth)});
        if (internal::BeginPopupLevel(slots.presetsId))
        {
            internal::MenuLevelBegin(slots.presetsId);
            for (int i = 0; i < static_cast<int>(options.presets.size()); ++i)
            {
                const ImVec2 preset = options.presets[i];
                char text[80];
                SizePresetText(text, static_cast<int>(sizeof(text)), preset, options, integer);
                ImGui::PushID(i);
                const bool current = static_cast<double>(preset.x) == values[0] && static_cast<double>(preset.y) == values[1];
                if (internal::MenuRow(slots.presetsId, text, {.checked = current}).pressed && !readOnly)
                {
                    bool changed = internal::NumberValueStore(preset.x, x);
                    changed = internal::NumberValueStore(preset.y, y) || changed;
                    written = written || changed;
                    if (state.locked && preset.y != 0.0f)
                        state.ratio = static_cast<double>(preset.x) / static_cast<double>(preset.y);
                }
                ImGui::PopID();
            }
            internal::EndPopupLevel();
        }
    }
    internal::ValueInputTrackRejected(id, parseFailed, written || activated);

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
    const internal::ControlMessage message =
        internal::PickControlMessage(internal::ValueInputError(id, options.error), options.helpText);
    internal::DrawControlMessage(message, ImVec2(fieldsRect.Min.x, fieldsRect.Max.y), fieldsRect.GetWidth(), disabled,
                                 firstId, fieldsRect);
    window->DC.CursorMaxPos.x = std::max(window->DC.CursorMaxPos.x, origin.x + width);
    ImGui::EndGroup();
    internal::FinishWidgetGroup(id);
    return written;
}

} // namespace

bool SizeInput(const char* label, ImVec2* value, const SizeInputOptions& options)
{
    float x = value != nullptr ? value->x : 0.0f;
    float y = value != nullptr ? value->y : 0.0f;
    const bool written = RunSizeInput(label, value != nullptr ? &x : nullptr, value != nullptr ? &y : nullptr, options);
    if (written && value != nullptr)
        *value = ImVec2(x, y);
    return written;
}

bool SizeInput(const char* label, int* x, int* y, const SizeInputOptions& options)
{
    return RunSizeInput(label, x, y, options);
}

} // namespace facet
