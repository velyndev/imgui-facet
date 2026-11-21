// UnsavedChangesBar (overlays.h): a bar at the bottom of the current window while settings have
// unsaved changes.
//
// The bar is a child window (ImGuiChildFlags_NavFlattened, so Tab reaches its buttons from the page)
// pinned spacing.md above the bottom of the window's visible area, over the page content, as wide as
// that area less spacing.md on each side. It slides up while changeCount > 0 and down when it reaches
// 0 (motion.durationStandard, at once under reduced motion), with the message (Body, cut with an
// ellipsis and its tooltip) and the Revert (Secondary) and Apply (Primary) buttons at the end; while
// it slides down it keeps the last message and takes no input. The window's content grows by the
// bar's height, so the last rows of the page can still be scrolled above it. Escape reverts while one
// of its buttons has the keyboard focus (after every layer in front of it, rule 8), and applyShortcut
// applies from anywhere. Call it after the window's other content, so it draws over the page's own
// child windows.

#include "facet/widgets/overlays.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/anim.h"
#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/effects.h"
#include "facet/core/input.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "facet/widgets/buttons.h"
#include "internal/control.h"
#include "internal/popup_host.h"
#include "internal/state_storage.h"
#include "internal/text_util.h"
#include "widgets/overlays/overlay_parts.h"

namespace facet
{
namespace
{

constexpr int kBarSlideSub = 1;
constexpr int kBarMessageSize = 160;

struct BarState
{
    int lastCount = 0;  // the count while the bar showed, for its message while it slides down
};

float ButtonWidthFor(const char* label, ControlSize size)
{
    const internal::ControlMetrics m = internal::GetControlMetrics(size);
    internal::ControlTextScope text(m);
    return std::ceil(m.paddingX * 2.0f + ImGui::CalcTextSize(label, ImGui::FindRenderedTextEnd(label)).x);
}

} // namespace

UnsavedChangesAction UnsavedChangesBar(const char* id, int changeCount, const UnsavedChangesBarOptions& options)
{
    using namespace internal;
    ImGuiWindow* window = ControlWindow();
    if (window == nullptr)
        return UnsavedChangesAction::None;
    if (id == nullptr)
        id = "";
    const Theme& theme = T();
    const Strings& strings = GetStrings();
    const ImGuiID barId = window->GetID(id);
    BarState& state = GetState<BarState>(barId);
    UnsavedChangesAction action = UnsavedChangesAction::None;
    const bool live = changeCount > 0;
    if (live)
        state.lastCount = changeCount;
    if (live && options.applyShortcut.IsValid() && ShortcutPressed(options.applyShortcut))
        action = UnsavedChangesAction::Apply;

    const float amount = Tween(barId, kBarSlideSub, live ? 1.0f : 0.0f, theme.motion.durationStandard);
    const float margin = Px(theme.spacing.md);
    const float inset = Px(theme.spacing.sm);
    const ControlMetrics m = GetControlMetrics(ControlSize::Regular);
    const float height = m.height + inset * 2.0f;

    // Room under the page content for the bar, so the last rows scroll above it.
    const float reserve = (height + margin * 2.0f) * amount;
    const ImVec2 savedCursor = window->DC.CursorPos;
    const ImVec2 savedMax = window->DC.CursorMaxPos;  // the page content above the call
    if (amount <= 0.0f)
        return action;

    const ImRect inner = window->InnerRect;
    const float width = std::max(1.0f, inner.GetWidth() - margin * 2.0f);
    const float slide = (1.0f - EaseValue(Ease::OutCubic, amount)) * (height + margin);
    const ImVec2 pos(std::round(inner.Min.x + margin), std::round(inner.Max.y - margin - height + slide));

    // The message from the template unless the caller gives one.
    char message[kBarMessageSize];
    if (options.message != nullptr)
    {
        ImStrncpy(message, options.message, sizeof(message));
    }
    else
    {
        const TextToken tokens[] = {{"count", state.lastCount}};
        FormatTokens(message, sizeof(message), state.lastCount == 1 ? strings.unsavedChange : strings.unsavedChanges,
                     tokens);
    }
    const char* applyLabel = options.applyLabel != nullptr ? options.applyLabel : strings.apply;
    const char* revertLabel = options.revertLabel != nullptr ? options.revertLabel : strings.revert;

    ImGui::SetCursorScreenPos(pos);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    const bool visible = ImGui::BeginChild(barId, ImVec2(width, height), ImGuiChildFlags_NavFlattened,
                                           ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                                               ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleVar();
    if (visible)
    {
        ImGuiWindow* bar = ImGui::GetCurrentWindow();
        ImDrawList* drawList = bar->DrawList;
        const ImRect rect(bar->Pos, ImVec2(bar->Pos.x + width, bar->Pos.y + height));
        const float radius = Px(theme.radii.card);
        drawList->PushClipRect(window->InnerClipRect.Min, window->InnerClipRect.Max, false);
        DrawElevation(drawList, rect.Min, rect.Max, radius, 1);
        FillRect(drawList, rect.Min, rect.Max, ToU32(theme.colors.ElevatedSurface(1)), radius);
        StrokeRect(drawList, rect.Min, rect.Max, ToU32(theme.colors.separator), radius, HairlineThickness());
        drawList->PopClipRect();

        // The buttons at the end; the message takes what is left.
        const float gap = Px(theme.spacing.sm);
        const float applyWidth = ButtonWidthFor(applyLabel, ControlSize::Regular);
        const float revertWidth = ButtonWidthFor(revertLabel, ControlSize::Regular);
        const float buttonsX = std::max(rect.Min.x + inset, rect.Max.x - inset - applyWidth - gap - revertWidth);
        const float messageWidth = std::max(0.0f, buttonsX - gap - (rect.Min.x + m.paddingX));
        {
            ControlTextScope text(m);
            const FittedText shown = FitText(message, nullptr, messageWidth);
            const ImVec2 at(rect.Min.x + m.paddingX, CenteredTextY(rect.Min.y, height));
            drawList->AddText(at, ToU32(live ? theme.colors.label : theme.colors.labelTertiary), shown.text);
            const ImRect textRect(at, ImVec2(at.x + shown.width, at.y + ImGui::GetFontSize()));
            const bool hovered = ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(textRect.Min, textRect.Max);
            CutTextTooltip(ImHashStr("##facet.message", 0, barId), textRect, hovered, false, message, nullptr,
                           shown.truncated);
        }
        ImGui::BeginDisabled(!live);
        ImGui::SetCursorScreenPos(ImVec2(buttonsX, rect.Min.y + inset));
        if (Button(revertLabel, {.variant = ButtonVariant::Secondary}))
            action = UnsavedChangesAction::Revert;
        const ImGuiID revertId = ImGui::GetItemID();
        ImRect rects[2] = {ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()), ImRect()};
        ImGui::SameLine(0.0f, gap);
        if (Button(applyLabel, {.variant = ButtonVariant::Primary}))
            action = UnsavedChangesAction::Apply;
        const ImGuiID applyId = ImGui::GetItemID();
        rects[1] = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        OverlayRecordRects(OverlayPartKey("bar", barId), rects, 2);
        ImGui::EndDisabled();
        // Escape reverts while a button of the bar has the focus, after every layer in front of it.
        const ImGuiID focus = GImGui->NavId;
        if (live && (focus == revertId || focus == applyId) && ClaimEscape(barId, EscapeLayer::Shell))
            action = UnsavedChangesAction::Revert;
    }
    ImGui::EndChild();

    // The bar is not part of the page flow: the cursor and the content bounds are put back, and the
    // content grows by the bar's room instead.
    window->DC.CursorPos = savedCursor;
    window->DC.CursorMaxPos = savedMax;
    window->DC.CursorMaxPos.y += reserve;
    if (!live)
        action = UnsavedChangesAction::None;
    return action;
}

} // namespace facet
