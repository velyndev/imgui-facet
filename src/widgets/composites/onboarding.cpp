// BeginOnboarding and EndOnboarding: a Modal (overlays.h) with the caller's step content and a
// footer of progress dots, the step text and the Skip, Back and Next or Finish Buttons. The modal
// gives the scrim, the open and close motion, the focus trap and Escape; the buttons are the
// finished widgets. This file adds the footer layout, the dots and the step logic.
//
// Footer: the dots (metrics.pageDotSize, the current one in the accent and the others in the
// separatorOpaque token) and Strings::stepOf beside them, so the step is never told by color
// alone; then at the end Skip (Plain, while skippable and not on the last step), Back (from the
// second step on) and Next, or Finish on the last step (Primary). Next and Finish take the
// keyboard focus when a step opens, so Enter steps through. A footer too narrow for the dots and
// the buttons puts the buttons on a line of their own.

#include "facet/widgets/composites.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "facet/widgets/buttons.h"
#include "facet/widgets/overlays.h"
#include "internal/control.h"
#include "internal/scope_stack.h"
#include "internal/state_storage.h"
#include "internal/text_util.h"

namespace facet
{
namespace
{

// Onboarding panels open inside each other at most.
constexpr int kOnboardingDepth = 4;

struct OnboardingFrame
{
    bool* open = nullptr;
    int* step = nullptr;
    int count = 1;
    bool skippable = true;
    ImGuiID id = 0;
};

struct OnboardingStack
{
    OnboardingFrame frames[kOnboardingDepth];
    int depth = 0;  // open panels, overflow included
};

OnboardingStack& Stack()
{
    static OnboardingStack stack;
    return stack;
}

// The step a panel showed last, so the primary button takes the focus once per step.
struct OnboardingState
{
    int focusedStep = -1;
};

float OnboardingButtonWidth(const char* label)
{
    const internal::ControlMetrics m = internal::GetControlMetrics(ControlSize::Inherit);
    internal::ControlTextScope text(m);
    return std::ceil(ImGui::CalcTextSize(label, ImGui::FindRenderedTextEnd(label)).x) + m.paddingX * 2.0f;
}

} // namespace

bool BeginOnboarding(const char* label, bool* open, int* step, int stepCount, const OnboardingOptions& options)
{
    if (open == nullptr || ImGui::GetCurrentContext() == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const int count = std::max(1, stepCount);
    if (step != nullptr && *open)
        *step = std::clamp(*step, 0, count - 1);
    ModalOptions modal;
    modal.closeButton = false;
    modal.width = options.width;
    if (!BeginModal(label, open, modal))
        return false;
    internal::PushScope("BeginOnboarding");
    OnboardingStack& stack = Stack();
    if (stack.depth < kOnboardingDepth)
    {
        OnboardingFrame& frame = stack.frames[stack.depth];
        frame.open = open;
        frame.step = step;
        frame.count = count;
        frame.skippable = options.skippable;
        frame.id = ImGui::GetID(label);
    }
    ++stack.depth;
    return true;
}

void EndOnboarding()
{
    if (!internal::PopScope("BeginOnboarding"))
        return;
    OnboardingStack& stack = Stack();
    if (stack.depth <= 0)
        return;
    const int slot = --stack.depth;
    if (slot >= kOnboardingDepth)
    {
        EndModal();
        return;
    }
    const OnboardingFrame frame = stack.frames[slot];
    const Theme& t = T();
    const Strings& s = GetStrings();
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    const int current = frame.step != nullptr ? std::clamp(*frame.step, 0, frame.count - 1) : 0;
    const bool last = current == frame.count - 1;
    OnboardingState& state = internal::GetState<OnboardingState>(frame.id);

    // ---- Footer ----
    ImGui::Dummy(ImVec2(0.0f, std::round(Px(t.spacing.lg))));
    const internal::ControlMetrics m = internal::GetControlMetrics(ControlSize::Inherit);
    const float width = ImGui::GetContentRegionAvail().x;
    const float gap = std::round(Px(t.spacing.sm));
    const float dot = std::round(Px(t.metrics.pageDotSize));
    char stepText[64];
    const internal::TextToken tokens[] = {{"step", current + 1}, {"count", frame.count}};
    internal::FormatTokens(stepText, sizeof(stepText), s.stepOf, tokens);
    const float dotsWidth = static_cast<float>(frame.count) * dot + static_cast<float>(frame.count - 1) * gap;
    const float textWidth = std::ceil(MeasureText(TextStyle::Footnote, stepText).x);
    const bool skip = frame.skippable && !last;
    const bool back = current > 0;
    const char* primary = last ? s.finish : s.next;
    const float buttonsWidth = (skip ? OnboardingButtonWidth(s.skip) + gap : 0.0f) +
                               (back ? OnboardingButtonWidth(s.back) + gap : 0.0f) + OnboardingButtonWidth(primary);
    const bool stacked = dotsWidth + gap + textWidth + gap * 2.0f + buttonsWidth > width;

    // The dots and the step text, centered on the button line.
    const ImVec2 origin(std::round(window->DC.CursorPos.x), std::round(window->DC.CursorPos.y));
    {
        const float centerY = origin.y + m.height * 0.5f;
        ImDrawList* drawList = window->DrawList;
        for (int i = 0; i < frame.count; ++i)
        {
            const ImVec2 center(origin.x + static_cast<float>(i) * (dot + gap) + dot * 0.5f, centerY);
            const ImVec4 color = i == current ? t.colors.accent : t.colors.separatorOpaque;
            drawList->AddCircleFilled(center, dot * 0.5f, ToU32(color));
        }
        internal::ControlTextScope text(TextStyle::Footnote);
        drawList->AddText(ImVec2(origin.x + dotsWidth + gap, CenteredTextY(origin.y, m.height)),
                          ToU32(internal::ControlSecondaryColor(false)), stepText);
        ImGui::Dummy(ImVec2(dotsWidth + gap + textWidth, m.height));
    }

    // The buttons at the end of the line, or of a line of their own.
    if (stacked)
        ImGui::SetCursorScreenPos(ImVec2(origin.x + std::max(0.0f, width - buttonsWidth), window->DC.CursorPos.y));
    else
        ImGui::SetCursorScreenPos(ImVec2(origin.x + width - buttonsWidth, origin.y));
    ImGui::PushOverrideID(frame.id);
    bool started = false;
    const auto sameLine = [&] {
        if (started)
            ImGui::SameLine(0.0f, gap);
        started = true;
    };
    if (skip)
    {
        sameLine();
        if (Button(s.skip, {.variant = ButtonVariant::Plain}) && frame.open != nullptr)
            *frame.open = false;
    }
    if (back)
    {
        sameLine();
        if (Button(s.back) && frame.step != nullptr)
            *frame.step = current - 1;
    }
    sameLine();
    char primaryLabel[96];
    std::snprintf(primaryLabel, sizeof(primaryLabel), "%s###primary", primary);
    if (state.focusedStep != current)
    {
        // A new step: the primary button takes the keyboard, so Enter steps through.
        ImGui::SetKeyboardFocusHere();
        state.focusedStep = current;
    }
    // Enter goes to the primary button while no button shows the keyboard focus (a panel opened
    // with the mouse); once one does, Enter activates that one as navigation does.
    const bool enter = !GImGui->NavCursorVisible && GImGui->ActiveId == 0 &&
                       ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
                       (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false));
    if (Button(primaryLabel, {.variant = ButtonVariant::Primary}) || enter)
    {
        if (last)
        {
            if (frame.open != nullptr)
                *frame.open = false;
        }
        else if (frame.step != nullptr)
        {
            *frame.step = current + 1;
        }
    }
    ImGui::PopID();
    if (frame.open != nullptr && !*frame.open)
        state.focusedStep = -1;
    EndModal();
}

} // namespace facet
