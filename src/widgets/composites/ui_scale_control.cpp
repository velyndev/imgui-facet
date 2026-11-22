// UiScaleControl: a SliderStepped over the scale steps between min and max (a tick per step, the
// value shown as percent through Strings::percent), followed by a zoom out and a zoom in
// IconButton. The slider and the buttons are the finished widgets; this file adds the steps, the
// optional zoom shortcuts and Ctrl+wheel, and with `apply` the SetUiScale call.
//
// Applying the scale changes every length on screen, the slider's own track included, so a scale
// applied while the slider is dragged would move the track under the pointer and the value would
// run away. A drag therefore applies once, when the slider is released (the value is still written
// while dragging, as for every slider); the arrow keys, the buttons, the shortcuts and the wheel
// apply on the frame they write. Escape during a drag restores the value from the drag start, so
// the release then applies nothing new.

#include "facet/widgets/composites.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/input.h"
#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "facet/widgets/buttons.h"
#include "facet/widgets/sliders.h"
#include "internal/control.h"
#include "internal/item.h"
#include "internal/state_storage.h"
#include "internal/text_util.h"

namespace facet
{
namespace
{

// Steps the slider shows as stops at most; a finer step uses a continuous slider that snaps to it.
constexpr int kUiScaleMaxStops = 64;
// Two scale values closer than this are the same step (float steps such as 0.1 do not add up exactly).
constexpr float kUiScaleSame = 1e-4f;

struct UiScaleControlState
{
    bool pending = false;  // a drag wrote a value that is applied on release
    float wheel = 0.0f;    // Ctrl+wheel notches not yet turned into a step
};

int UiScalePercentText(char* buffer, int bufferSize, double value, void*)
{
    const internal::TextToken token{"percent", static_cast<long long>(std::lround(value * 100.0))};
    return internal::FormatTokens(buffer, static_cast<std::size_t>(std::max(0, bufferSize)), GetStrings().percent,
                                  std::span<const internal::TextToken>(&token, 1));
}

// The scale `steps` steps away from `value` on the grid min + n * step (the last stop is max).
float UiScaleStepFrom(float value, int steps, float min, float max, float step)
{
    if (step <= 0.0f)
        return std::clamp(value, min, max);
    const float position = (std::clamp(value, min, max) - min) / step;
    // A value between stops moves to the neighbouring stop first.
    float index = steps > 0 ? std::floor(position + kUiScaleSame) : std::ceil(position - kUiScaleSame);
    index += static_cast<float>(steps);
    return std::clamp(min + index * step, min, max);
}

} // namespace

bool UiScaleControl(const char* label, float* scale, float min, float max, const UiScaleControlOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    if (min > max)
        std::swap(min, max);
    const Theme& t = T();
    const Strings& s = GetStrings();
    const internal::NextItemData next = internal::TakeNextItemData();
    const internal::ForcedItemRequest forced = internal::TakeForcedItemState();
    const internal::ControlMetrics m = internal::GetControlMetrics(options.size);
    const ImGuiID id = window->GetID(label);
    const bool disabled = (next.itemFlags & ImGuiItemFlags_Disabled) != 0;
    const bool readOnly = options.readOnly || scale == nullptr || min == max || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    const float step = options.step > 0.0f ? options.step : 0.0f;
    UiScaleControlState& state = internal::GetState<UiScaleControlState>(id);
    const float before = scale != nullptr ? *scale : UiScale();
    float value = before;

    // ---- The stops ----
    float stops[kUiScaleMaxStops];
    int stopCount = 0;
    if (step > 0.0f && (max - min) / step < static_cast<float>(kUiScaleMaxStops - 1))
    {
        for (float v = min; v < max - kUiScaleSame && stopCount < kUiScaleMaxStops - 1; v = min + static_cast<float>(stopCount) * step)
            stops[stopCount++] = v;
        stops[stopCount++] = max;
    }

    // ---- Layout: the slider takes the width the two buttons leave ----
    const float resolved = std::round(internal::ResolveItemWidth(options.width, next, kFill));
    const float button = std::round(m.height);
    const float gap = std::round(Px(t.spacing.xs));
    const float sliderWidth = std::max(button, resolved - 2.0f * (button + gap));

    bool written = false;
    bool applyNow = false;
    const auto write = [&](float target) {
        if (readOnly || std::fabs(target - value) < kUiScaleSame)
            return;
        value = target;
        written = true;
        applyNow = true;
    };

    ImGui::BeginGroup();
    internal::NextItemData sliderNext = next;
    sliderNext.data.HasFlags &= ~ImGuiNextItemDataFlags_HasWidth;
    sliderNext.hasWidth = false;
    internal::ApplyNextItemData(sliderNext);
    // Part 0 is the slider, 1 the zoom out and 2 the zoom in button.
    if (forced.state != internal::ForcedItemState::None && forced.part <= 0)
        internal::ForceNextItemState(forced.state);
    ImGui::SetNextItemWidth(sliderWidth);
    bool sliderWrote = false;
    if (stopCount >= 2)
    {
        SliderSteppedOptions slider;
        slider.formatter = &UiScalePercentText;
        slider.size = options.size;
        slider.readOnly = readOnly;
        sliderWrote = SliderStepped(label, &value, std::span<const float>(stops, static_cast<std::size_t>(stopCount)), slider);
    }
    else
    {
        SliderOptions slider;
        slider.formatter = &UiScalePercentText;
        slider.step = step;
        slider.size = options.size;
        slider.readOnly = readOnly;
        sliderWrote = Slider(label, &value, min, max, slider);
    }
    const bool sliderActive = ImGui::IsItemActive();
    const bool sliderReleased = ImGui::IsItemDeactivated();
    if (sliderWrote && !readOnly)
    {
        written = true;
        if (sliderActive)
            state.pending = true;
        else
            applyNow = true;
    }
    if (sliderReleased && state.pending)
    {
        state.pending = false;
        applyNow = true;
    }

    // The buttons, labelled through Strings for their tooltips; "###" keeps their ids stable.
    char outLabel[96];
    char inLabel[96];
    std::snprintf(outLabel, sizeof(outLabel), "%s###zoomOut", s.zoomOut);
    std::snprintf(inLabel, sizeof(inLabel), "%s###zoomIn", s.zoomIn);
    IconButtonOptions buttons;
    buttons.size = options.size;
    // The buttons are named under the control's id, so two controls in one window never share them.
    ImGui::PushOverrideID(id);
    ImGui::SameLine(0.0f, gap);
    if (forced.state != internal::ForcedItemState::None && forced.part == 1)
        internal::ForceNextItemState(forced.state);
    ImGui::BeginDisabled(readOnly && !disabled);
    if (IconButton(outLabel, Icon("zoom-out"), buttons))
        write(UiScaleStepFrom(value, -1, min, max, step));
    ImGui::SameLine(0.0f, gap);
    if (forced.state != internal::ForcedItemState::None && forced.part == 2)
        internal::ForceNextItemState(forced.state);
    if (IconButton(inLabel, Icon("zoom-in"), buttons))
        write(UiScaleStepFrom(value, 1, min, max, step));
    ImGui::EndDisabled();
    ImGui::PopID();
    ImGui::EndGroup();
    internal::FinishWidgetGroup(id);

    // ---- Zoom from anywhere: the shortcuts and Ctrl+wheel ----
    if (!readOnly && !disabled)
    {
        if (options.zoomInShortcut.IsValid() && ShortcutPressed(options.zoomInShortcut))
            write(UiScaleStepFrom(value, 1, min, max, step));
        if (options.zoomOutShortcut.IsValid() && ShortcutPressed(options.zoomOutShortcut))
            write(UiScaleStepFrom(value, -1, min, max, step));
        if (options.zoomResetShortcut.IsValid() && ShortcutPressed(options.zoomResetShortcut))
            write(std::clamp(1.0f, min, max));
        const ImGuiIO& io = ImGui::GetIO();
        if (options.wheelZoom && io.KeyCtrl && io.MouseWheel != 0.0f)
        {
            // Whole notches step; a touchpad's small deltas add up to one first.
            state.wheel += io.MouseWheel;
            const int notches = static_cast<int>(state.wheel);
            if (notches != 0)
            {
                state.wheel -= static_cast<float>(notches);
                write(UiScaleStepFrom(value, notches, min, max, step));
            }
        }
        else if (!io.KeyCtrl)
        {
            state.wheel = 0.0f;
        }
    }

    if (!written && !applyNow)
        return false;
    if (written && scale != nullptr)
    {
        *scale = value;
        internal::MarkWidgetEdited(id);
    }
    if (applyNow && options.apply && scale != nullptr && std::fabs(UiScale() - *scale) >= kUiScaleSame)
        SetUiScale(*scale);
    return written && std::fabs(value - before) >= kUiScaleSame;
}

} // namespace facet
