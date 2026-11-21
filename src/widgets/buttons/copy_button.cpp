// CopyButton: copies a text to the clipboard and confirms it with the check icon and
// Strings::copied for a moment, in a width that holds both looks so nothing moves.

#include "facet/widgets/buttons.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/copy_feedback.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/popup_host.h"
#include "internal/state_storage.h"
#include "widgets/buttons/button_look.h"

namespace facet
{
namespace
{

// The time of the last copy, kept under the button id.
struct CopyButtonState
{
    double copiedAt = -1.0;  // ImGui::GetTime() of the last copy; < 0 for none
};

} // namespace

bool CopyButton(const char* label, const char* text, const CopyButtonOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const internal::NextItemData next = internal::TakeNextItemData();
    const internal::ControlMetrics m = internal::GetControlMetrics(options.size);
    internal::ControlTextScope scope(m);
    const Theme& t = T();
    const Colors& c = t.colors;
    const Strings& strings = GetStrings();

    // The name: the visible label part, or Strings::copy when it is empty.
    const char* name = label;
    const char* nameEnd = ImGui::FindRenderedTextEnd(label);
    if (nameEnd == name)
    {
        name = strings.copy;
        nameEnd = name + std::strlen(name);
    }
    const float iconSize = scope.IconSize();
    float defaultWidth = m.height;
    if (!options.iconOnly)
    {
        // Wide enough for the name and for Strings::copied, so the confirmation does not move anything.
        const float textWidth = std::max(ImGui::CalcTextSize(name, nameEnd).x, ImGui::CalcTextSize(strings.copied).x);
        defaultWidth = std::ceil(m.paddingX * 2.0f + iconSize + m.iconGap + textWidth);
    }
    const float width = internal::ResolveItemWidth(0.0f, next, defaultWidth);

    const ImGuiID id = window->GetID(label);
    const float top = window->DC.CursorPos.y;
    internal::ItemOptions itemOptions;
    itemOptions.baseline = CenteredTextY(top, m.height) - top;
    itemOptions.disabled = text == nullptr;  // nothing to copy
    internal::ApplyNextItemData(next);
    const internal::ItemState item = internal::ItemBehavior(id, ImVec2(width, m.height), itemOptions);

    // Copy, and remember when, so the confirmation shows for CopyFeedbackHold() seconds.
    const bool copied = item.pressed && text != nullptr && item.forced == internal::ForcedItemState::None;
    const double now = ImGui::GetTime();
    bool confirming = false;
    if (copied || internal::GlobalStateStore().Find(id, internal::StateTypeOf<CopyButtonState>()) != nullptr)
    {
        CopyButtonState& state = internal::GetState<CopyButtonState>(id);
        if (copied)
        {
            ImGui::SetClipboardText(text);
            state.copiedAt = now;
        }
        confirming = internal::CopyFeedbackActive(state.copiedAt);
    }
    if (item.forced != internal::ForcedItemState::None && internal::Has(item.forced, internal::ForcedItemState::Pressed))
        confirming = true;  // captures show the confirmation with the pressed look

    bool truncated = false;
    if (item.visible)
    {
        const internal::ButtonColors colors = internal::ButtonLook(options.variant, item);
        ImDrawList* drawList = window->DrawList;
        FillRect(drawList, item.rect.Min, item.rect.Max, ToU32(colors.fill), m.radius);
        if (colors.outline.w > 0.0f)
            StrokeRect(drawList, item.rect.Min, item.rect.Max, ToU32(colors.outline), m.radius, HairlineThickness());
        // The confirmation: the check icon, in the success color on the neutral variants (it reads
        // as text on every surface), with Strings::copied in place of the name.
        const bool neutral = options.variant == ButtonVariant::Secondary || options.variant == ButtonVariant::Plain;
        const ImU32 iconColor = ToU32(confirming && neutral && !item.disabled ? c.success : colors.content);
        const Icon icon = confirming ? Icon("check") : Icon("copy");
        if (options.iconOnly)
        {
            internal::DrawControlIcon(drawList, icon, std::round(item.rect.GetCenter().x - iconSize * 0.5f),
                                      item.rect.GetCenter().y, iconSize, iconColor);
        }
        else
        {
            const char* shown = confirming ? strings.copied : name;
            const char* shownEnd = confirming ? nullptr : nameEnd;
            const float room = std::max(0.0f, width - m.paddingX * 2.0f - iconSize - m.iconGap);
            const internal::FittedText fitted = internal::FitText(shown, shownEnd, room);
            truncated = fitted.truncated;
            const float x = std::round(item.rect.GetCenter().x - (iconSize + m.iconGap + fitted.width) * 0.5f);
            internal::DrawControlIcon(drawList, icon, x, item.rect.GetCenter().y, iconSize, iconColor);
            drawList->AddText(ImVec2(x + iconSize + m.iconGap, CenteredTextY(item.rect.Min.y, m.height)),
                              ToU32(colors.content), fitted.text);
        }
        internal::DrawItemFocusRing(item, m.radius);
    }

    // The name (or Strings::copied while confirming) as the tooltip of an icon-only button, and the
    // whole name of a cut label (rule 11).
    if (options.iconOnly || truncated)
    {
        if (internal::BeginPopupTooltip(id, item.rect, item.hovered, item.focusVisible))
        {
            ImGui::PushStyleColor(ImGuiCol_Text, c.label);
            PushTextStyle(TextStyle::Footnote);
            if (confirming)
                ImGui::TextUnformatted(strings.copied);
            else
                ImGui::TextUnformatted(name, nameEnd);
            PopTextStyle();
            ImGui::PopStyleColor();
            internal::EndPopupTooltip();
        }
    }
    return copied;
}

} // namespace facet
