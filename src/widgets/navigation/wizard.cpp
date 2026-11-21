// BeginWizard, WizardFooter and EndWizard (navigation.h): a multi-step flow. BeginWizard draws the
// interactive step indicator (done steps can be activated to go back), then lays out the caller's
// content of the current step, which slides in from the side it comes from when the step changes
// (a Tween of the step under the wizard id, motion.durationStandard; it stands still with reduced
// motion). WizardFooter puts Back, Next (Finish on the last step) and optionally Cancel at the end
// of a row; Next and Finish are the primary action.
//
// Steps whose ChoiceItem is disabled are skipped by Back and Next, so a flow can leave out a step
// that does not apply; the indicator shows them dimmed. Next and Finish are disabled while
// canAdvance is false, which is how a step that fails its validation holds the flow (the caller
// shows why, for example with the error text of the field). Back is disabled on the first enabled
// step. The footer's buttons are Tab stops; Enter and Space activate the focused one.

#include "facet/widgets/navigation.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <imgui.h>
#include <imgui_internal.h>

#include "core/core_internal.h"
#include "facet/core/anim.h"
#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "facet/widgets/buttons.h"
#include "internal/control.h"
#include "internal/item.h"
#include "internal/scope_stack.h"
#include "widgets/navigation/navigation_parts.h"
#include "widgets/selection/selection_parts.h"

namespace facet
{
namespace
{

using namespace internal;

constexpr int kWizardDepth = 4;
constexpr int kWizardMaxSteps = 32;
// Animation sub key of the sliding content (below kItemAnimSubBase).
constexpr int kWizardSlideSub = 1;

struct WizardFrame
{
    ImGuiID id = 0;
    ImGuiWindow* window = nullptr;
    int* step = nullptr;
    int count = 0;
    bool disabled[kWizardMaxSteps] = {};
    float left = 0.0f;
    float width = 0.0f;
    bool clipped = false;
};

struct WizardStack
{
    WizardFrame frames[kWizardDepth];
    int depth = 0;
    int overflow = 0;
};

WizardStack& Wizards()
{
    static WizardStack stack;
    return stack;
}

WizardFrame* CurrentWizard()
{
    WizardStack& stack = Wizards();
    if (stack.overflow > 0 || stack.depth == 0)
        return nullptr;
    WizardFrame& frame = stack.frames[stack.depth - 1];
    return frame.window != nullptr && frame.window == GImGui->CurrentWindow ? &frame : nullptr;
}

// The width facet::Button takes for `label` at the inherited control size, so the footer can put
// its buttons at the trailing end before submitting them.
float ButtonWidthFor(const char* label)
{
    const ControlMetrics m = GetControlMetrics(ControlSize::Inherit);
    const ControlTextScope text(m);
    return std::ceil(m.paddingX * 2.0f + ImGui::CalcTextSize(label, ImGui::FindRenderedTextEnd(label)).x);
}

// The next enabled step from `from` in direction `dir`, or -1.
int WizardStepFrom(const WizardFrame& frame, int from, int dir)
{
    for (int i = from + dir; i >= 0 && i < frame.count; i += dir)
        if (!frame.disabled[i])
            return i;
    return -1;
}

void BeginWizardImpl(const char* id, int* step, const SelectionItems& items, const WizardOptions& options)
{
    PushScope("BeginWizard");
    WizardStack& stack = Wizards();
    if (stack.depth >= kWizardDepth)
    {
        ReportUsageError("Wizards nested deeper than %d", kWizardDepth);
        ++stack.overflow;
        return;
    }
    WizardFrame& frame = stack.frames[stack.depth++];
    frame = WizardFrame{};
    ImGuiWindow* window = ControlWindow();
    if (window == nullptr)
        return;
    if (id == nullptr)
        id = "";
    const NextItemData next = TakeNextItemData();
    const Theme& t = T();
    frame.window = window;
    frame.id = window->GetID(id);
    frame.step = step;
    frame.count = std::min(items.Count(), kWizardMaxSteps);
    for (int i = 0; i < frame.count; ++i)
        frame.disabled[i] = items.At(i).disabled;
    frame.width = ResolveItemWidth(options.width, next, kFill);
    frame.left = window->DC.CursorPos.x;
    ImGui::PushID(id);
    ImGui::BeginGroup();
    if (options.stepIndicator && frame.count > 0)
    {
        StepIndicatorOptions indicator;
        indicator.width = frame.width / Px(1.0f);
        if (items.labels.empty())
            (void)StepIndicator("##steps", step, items.choices, indicator);
        else
            (void)StepIndicator("##steps", step, items.labels, indicator);
        ImGui::Dummy(ImVec2(0.0f, Px(t.spacing.sm)));
    }
    // The content slides from the side of the step it comes from: a Tween of the step index, so
    // the offset is (step - shown) of one slide distance and settles at 0.
    const int current = step != nullptr ? *step : 0;
    const float shown = Tween(frame.id, kWizardSlideSub, static_cast<float>(current), t.motion.durationStandard);
    const float offset = std::round((static_cast<float>(current) - shown) * Px(t.spacing.xxxl));
    const ImVec2 at = window->DC.CursorPos;
    ImGui::PushClipRect(ImVec2(frame.left, window->ClipRect.Min.y), ImVec2(frame.left + frame.width, window->ClipRect.Max.y), true);
    frame.clipped = true;
    ImGui::SetCursorScreenPos(ImVec2(at.x + offset, at.y));
    ImGui::BeginGroup();
}

} // namespace

void BeginWizard(const char* id, int* step, std::span<const ChoiceItem> steps, const WizardOptions& options)
{
    BeginWizardImpl(id, step, {steps, {}}, options);
}

void BeginWizard(const char* id, int* step, std::span<const char* const> steps, const WizardOptions& options)
{
    BeginWizardImpl(id, step, {{}, steps}, options);
}

void EndWizard()
{
    if (!PopScope("BeginWizard"))
        return;
    WizardStack& stack = Wizards();
    if (stack.overflow > 0)
    {
        --stack.overflow;
        return;
    }
    if (stack.depth == 0)
        return;
    WizardFrame& frame = stack.frames[--stack.depth];
    if (frame.window == nullptr || GImGui->CurrentWindow != frame.window)
        return;
    ImGui::EndGroup();
    if (frame.clipped)
        ImGui::PopClipRect();
    ImGui::EndGroup();
    ImGui::PopID();
    FinishWidgetGroup(frame.id);
    frame.window = nullptr;
}

WizardAction WizardFooter(const WizardFooterOptions& options)
{
    WizardFrame* frame = CurrentWizard();
    if (frame == nullptr || ControlWindow() == nullptr)
    {
        IM_ASSERT_USER_ERROR(Wizards().depth > 0, "WizardFooter outside BeginWizard and EndWizard");
        return WizardAction::None;
    }
    const Theme& t = T();
    const Strings& s = GetStrings();
    const int current = frame->step != nullptr ? *frame->step : 0;
    const int back = WizardStepFrom(*frame, current, -1);
    const int forward = WizardStepFrom(*frame, current, 1);
    const bool last = forward < 0;
    const bool readOnly = frame->step == nullptr;
    const char* finish = options.finishLabel != nullptr ? options.finishLabel : s.finish;
    char backLabel[96];
    char nextLabel[96];
    char cancelLabel[96];
    std::snprintf(backLabel, sizeof(backLabel), "%s###back", s.back);
    std::snprintf(nextLabel, sizeof(nextLabel), "%s###next", last ? finish : s.next);
    std::snprintf(cancelLabel, sizeof(cancelLabel), "%s###cancel", s.cancel);

    ImGui::Dummy(ImVec2(0.0f, Px(t.spacing.sm)));
    ImGui::PushID("##footer");
    const float gap = Px(t.spacing.sm);
    const float wBack = ButtonWidthFor(backLabel);
    const float wNext = ButtonWidthFor(nextLabel);
    const float right = frame->left + frame->width;
    WizardAction action = WizardAction::None;
    const float y = ImGui::GetCursorScreenPos().y;
    if (options.cancelButton)
    {
        ImGui::SetCursorScreenPos(ImVec2(frame->left, y));
        if (Button(cancelLabel))
            action = WizardAction::Cancel;
        NavigationRecordPart(frame->id, kNavigationPartCancel, GImGui->LastItemData.Rect);
        ImGui::SameLine();
    }
    // Back and Next at the trailing end, Next last, so it is the action the eye ends on.
    const float startX = std::max(ImGui::GetCursorScreenPos().x, right - wBack - gap - wNext);
    ImGui::SetCursorScreenPos(ImVec2(startX, y));
    ImGui::BeginDisabled(back < 0 || readOnly);
    if (Button(backLabel) && back >= 0 && !readOnly)
    {
        *frame->step = back;
        action = WizardAction::Back;
    }
    NavigationRecordPart(frame->id, kNavigationPartBack, GImGui->LastItemData.Rect);
    ImGui::EndDisabled();
    ImGui::SameLine(0.0f, gap);
    ImGui::BeginDisabled(!options.canAdvance || readOnly);
    if (Button(nextLabel, {.variant = ButtonVariant::Primary}) && options.canAdvance && !readOnly)
    {
        if (last)
        {
            action = WizardAction::Finish;
        }
        else
        {
            *frame->step = forward;
            action = WizardAction::Next;
        }
    }
    NavigationRecordPart(frame->id, kNavigationPartAdvance, GImGui->LastItemData.Rect);
    ImGui::EndDisabled();
    ImGui::PopID();
    return action;
}

} // namespace facet
