// ConfirmDialog (overlays.h): a compact modal with a message and two buttons.
//
// Built on BeginModal (modal.cpp) without its close button: the scrim, the focus trap and return,
// Escape and the stacking come from there. The buttons are facet::Button: Cancel (Secondary) and the
// confirm button (Primary, or Destructive), at the trailing end of the last row, side by side while
// they fit and stacked at full width otherwise.
//
// Keys: the keyboard focus starts on the confirm button, or on Cancel when the dialog is
// destructive, so Enter and Space act on that button; Enter anywhere in the dialog confirms unless it
// is destructive or must be held, or the focus is on Cancel. Escape and a click on the scrim cancel.
// With holdToConfirm the confirm button must stay held (mouse, Space or Enter) for
// motion.durationSlow; its fill grows from the start while it is held and drops back on release.

#include "facet/widgets/overlays.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/context.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "facet/widgets/buttons.h"
#include "internal/control.h"
#include "internal/popup_host.h"
#include "internal/state_storage.h"
#include "widgets/overlays/overlay_parts.h"

namespace facet
{
namespace
{

struct ConfirmHoldState
{
    float held = 0.0f;  // seconds the confirm button has been held in the current press
};

float ButtonWidthFor(const char* label)
{
    const internal::ControlMetrics m = internal::GetControlMetrics(ControlSize::Regular);
    internal::ControlTextScope text(m);
    const char* end = ImGui::FindRenderedTextEnd(label);
    return std::ceil(m.paddingX * 2.0f + ImGui::CalcTextSize(label, end).x);
}

} // namespace

DialogResult ConfirmDialog(const char* label, bool* open, const char* message, const ConfirmDialogOptions& options)
{
    using namespace internal;
    if (open == nullptr || ControlWindow() == nullptr)
        return DialogResult::None;
    if (label == nullptr)
        label = "";
    const ImGuiID level = GImGui->CurrentWindow->GetID(label);
    const bool wasOpen = *open;
    if (!BeginModal(label, open, {.dismissible = true, .closeButton = false}))
        return wasOpen && !*open ? DialogResult::Canceled : DialogResult::None;

    const Theme& theme = T();
    const Strings& strings = GetStrings();
    const char* confirmLabel = options.confirmLabel != nullptr ? options.confirmLabel : strings.confirm;
    const char* cancelLabel = options.cancelLabel != nullptr ? options.cancelLabel : strings.cancel;
    DialogResult result = DialogResult::None;

    if (message != nullptr && message[0] != '\0')
    {
        PushTextStyle(TextStyle::Body);
        ImGui::PushStyleColor(ImGuiCol_Text, theme.colors.labelSecondary);
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(message);
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
        PopTextStyle();
        ImGui::Dummy(ImVec2(0.0f, Px(theme.spacing.md)));
    }

    // The button row, trailing; stacked at full width when the two do not fit side by side.
    const float gap = Px(theme.spacing.sm);
    const float available = ImGui::GetContentRegionAvail().x;
    const float cancelWidth = ButtonWidthFor(cancelLabel);
    const float confirmWidth = ButtonWidthFor(confirmLabel);
    const bool stacked = cancelWidth + gap + confirmWidth > available;
    if (options.holdToConfirm)
    {
        PushTextStyle(TextStyle::Footnote);
        ImGui::PushStyleColor(ImGuiCol_Text, theme.colors.labelSecondary);
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(strings.holdToConfirm);
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
        PopTextStyle();
    }
    const float rowX = ImGui::GetCursorPosX();
    if (!stacked)
        ImGui::SetCursorPosX(rowX + available - cancelWidth - gap - confirmWidth);
    const float buttonWidth = stacked ? kFill : 0.0f;
    if (Button(cancelLabel, {.variant = ButtonVariant::Secondary, .width = buttonWidth}))
        result = DialogResult::Canceled;
    const ImGuiID cancelId = ImGui::GetItemID();
    ImRect rects[2] = {ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()), ImRect()};
    if (options.destructive)
        ImGui::SetItemDefaultFocus();
    if (!stacked)
        ImGui::SameLine(0.0f, gap);
    const ButtonVariant variant = options.destructive ? ButtonVariant::Destructive : ButtonVariant::Primary;
    const bool confirmPressed = Button(confirmLabel, {.variant = variant, .width = buttonWidth});
    rects[1] = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    OverlayRecordRects(OverlayPartKey("confirm", level), rects, 2);
    if (!options.destructive)
        ImGui::SetItemDefaultFocus();
    if (options.holdToConfirm)
    {
        // Held long enough confirms; a release before that does nothing.
        ConfirmHoldState& hold = GetState<ConfirmHoldState>(level);
        const float needed = std::max(theme.motion.durationSlow, 0.0f);
        if (ImGui::IsItemActive())
        {
            hold.held += DeltaTime();
            const ImRect rect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
            const float share = needed > 0.0f ? std::clamp(hold.held / needed, 0.0f, 1.0f) : 1.0f;
            const float radius = Px(theme.radii.control);
            FillRect(ImGui::GetWindowDrawList(), rect.Min, ImVec2(rect.Min.x + rect.GetWidth() * share, rect.Max.y),
                     ToU32(theme.colors.fill[0]), radius);
            if (hold.held >= needed)
            {
                result = DialogResult::Confirmed;
                hold.held = 0.0f;
            }
        }
        else
        {
            hold.held = 0.0f;
        }
    }
    else if (confirmPressed)
    {
        result = DialogResult::Confirmed;
    }

    // Enter confirms from anywhere in the dialog, unless the dialog is destructive, must be held, or
    // the focus is on Cancel (Enter then presses Cancel). While the keyboard focus is not shown yet
    // (the dialog opened from a click), Dear ImGui's Enter acts on no button, so Enter takes the
    // button the focus starts on: Cancel in a destructive dialog.
    const ImGuiContext& g = *GImGui;
    const bool enter = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
                       (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false));
    if (result == DialogResult::None && enter)
    {
        if (!options.destructive && !options.holdToConfirm && g.NavId != cancelId)
            result = DialogResult::Confirmed;
        else if (options.destructive && !g.NavCursorVisible)
            result = DialogResult::Canceled;
    }

    EndModal();
    if (result != DialogResult::None)
    {
        ClosePopupLevel(level);
        *open = false;
    }
    return result;
}

} // namespace facet
