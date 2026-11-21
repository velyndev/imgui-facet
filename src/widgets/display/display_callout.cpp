// The alert surface of Banner and InlineAlert (see display_callout.h).

#include "widgets/display/display_callout.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/anim.h"
#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "facet/widgets/buttons.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "widgets/display/display_look.h"

namespace facet::internal
{
namespace
{

// Animation sub key of the open amount under the widget id.
constexpr int kCalloutAnimOpen = 1;

float CalloutButtonWidth(const char* text, const ControlMetrics& small)
{
    if (text == nullptr || text[0] == '\0')
        return 0.0f;
    return std::ceil(small.paddingX * 2.0f + MeasureText(small.textStyle, text, ImGui::FindRenderedTextEnd(text)).x);
}

// The close button: a square plain button with the close icon, named by Strings::close in its
// tooltip on hover and keyboard focus.
bool CalloutCloseButton(ImVec2 pos, bool* escape)
{
    ImGui::SetCursorScreenPos(pos);
    const bool pressed = Button("###close", {.variant = ButtonVariant::Plain, .icon = Icon("close"), .size = ControlSize::Small});
    const ImGuiID id = ImGui::GetItemID();
    const ImRect rect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    *escape = DisplayDismissKey(id) || *escape;
    CutTextTooltip(id, rect, ImGui::IsItemHovered(), ImGui::IsItemFocused() && KeyboardModality(), GetStrings().close,
                   nullptr, true);
    return pressed;
}

} // namespace

DisplayCalloutResult DisplayCallout(const DisplayCalloutSpec& spec)
{
    DisplayCalloutResult result;
    ImGuiWindow* window = ControlWindow();
    if (window == nullptr)
        return result;
    const char* label = spec.label != nullptr ? spec.label : "";
    const NextItemData next = TakeNextItemData();
    const Theme& t = T();
    const Colors& c = t.colors;
    const ImGuiID id = window->GetID(label);
    const bool open = spec.open == nullptr || *spec.open;
    // The open amount is kept every frame, also while collapsed, so a reopened banner grows back.
    const float openAmount = spec.open != nullptr ? Tween(id, kCalloutAnimOpen, open ? 1.0f : 0.0f, t.motion.durationStandard)
                                                  : 1.0f;
    if (openAmount <= 0.0f)
        return result;

    const float width = ResolveItemWidth(spec.width, next, kFill);
    const ControlMetrics small = GetControlMetrics(ControlSize::Small);
    const float pad = Px(t.spacing.md);
    const float gap = Px(t.spacing.sm);
    const float gapXs = Px(t.spacing.xs);
    const char* titleEnd = ImGui::FindRenderedTextEnd(label);
    const bool hasTitle = titleEnd != label;
    const bool hasMessage = spec.message != nullptr && spec.message[0] != '\0';
    PushTextStyle(TextStyle::Body);
    const float iconSize = std::round(ImGui::GetFontSize());
    PopTextStyle();

    // Columns: icon, text, actions (beside the text on a wide Banner, else under it), close button.
    const float primaryWidth = CalloutButtonWidth(spec.primary, small);
    const float secondaryWidth = CalloutButtonWidth(spec.secondary, small);
    const float actionsWidth = primaryWidth + secondaryWidth + (primaryWidth > 0.0f && secondaryWidth > 0.0f ? gap : 0.0f);
    const float closeSize = spec.open != nullptr ? small.height : 0.0f;
    const float textLeft = pad + iconSize + gap;
    const float textRight = width - pad - (closeSize > 0.0f ? closeSize + gap : 0.0f);
    float textWidth = std::max(1.0f, textRight - textLeft);
    const bool beside = spec.actionsBeside && actionsWidth > 0.0f &&
                        textWidth - actionsWidth - gap >= Px(t.layout.minRowControl);
    if (beside)
        textWidth -= actionsWidth + gap;
    const float titleHeight = hasTitle ? MeasureText(TextStyle::Headline, label, titleEnd, textWidth).y : 0.0f;
    const float messageHeight = hasMessage ? MeasureText(TextStyle::Body, spec.message, nullptr, textWidth).y : 0.0f;
    const float textHeight = titleHeight + (hasTitle && hasMessage ? gapXs : 0.0f) + messageHeight;
    const float firstLine = hasTitle ? MeasureText(TextStyle::Headline, "").y : MeasureText(TextStyle::Body, "").y;
    const float belowActions = actionsWidth > 0.0f && !beside ? gap + small.height : 0.0f;
    const float contentHeight = std::max({textHeight + belowActions, closeSize, beside ? small.height : 0.0f, firstLine});
    const float fullHeight = std::ceil(pad * 2.0f + contentHeight);
    const float height = std::round(fullHeight * openAmount);
    if (height < 1.0f)
        return result;

    const ImVec2 origin = window->DC.CursorPos;
    const ImRect bounds(origin, ImVec2(origin.x + width, origin.y + height));
    const ImVec2 savedMax = window->DC.CursorMaxPos;
    const bool disabled = (next.itemFlags & ImGuiItemFlags_Disabled) != 0;
    const DisplayToneColors tone = DisplayToneLook(spec.tone, disabled);
    ImDrawList* drawList = window->DrawList;
    const float radius = Px(t.radii.control);
    const float top = origin.y + pad;
    // Drawn and submitted also while out of sight: Tab reaches the buttons there and scrolls to them.
    {
        ImGui::PushClipRect(bounds.Min, bounds.Max, true);
        FillRect(drawList, bounds.Min, bounds.Max, ToU32(tone.wash), radius);
        if (spec.outline)
            StrokeRect(drawList, bounds.Min, bounds.Max, ToU32(disabled ? c.separator : c.separatorOpaque), radius,
                       HairlineThickness());
        // The icon on the capitals of the first line, in the tone: the cue besides the wash.
        PushTextStyle(hasTitle ? TextStyle::Headline : TextStyle::Body);
        const float capsCenter = DisplayCapsCenter(top);
        PopTextStyle();
        const Icon icon = spec.icon.IsValid() ? spec.icon : DisplayToneIcon(spec.tone);
        DrawControlIcon(drawList, icon, origin.x + pad, capsCenter, iconSize, ToU32(tone.glyph));
        const ImU32 text = ToU32(ControlLabelColor(disabled));
        float y = top;
        if (hasTitle)
        {
            PushTextStyle(TextStyle::Headline);
            drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(origin.x + textLeft, y), text, label, titleEnd,
                              textWidth);
            PopTextStyle();
            y += titleHeight + (hasMessage ? gapXs : 0.0f);
        }
        if (hasMessage)
        {
            PushTextStyle(TextStyle::Body);
            drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(origin.x + textLeft, y), text, spec.message,
                              nullptr, textWidth);
            PopTextStyle();
        }

        // Buttons: Tab stops under the widget id. They keep working while the surface is fully
        // open; while it collapses or grows they are drawn clipped and ignored.
        ImGui::PushOverrideID(id);
        bool escape = false;
        const bool settled = openAmount >= 1.0f && open;
        const float firstLineButtonTop = std::round(top + (firstLine - small.height) * 0.5f);
        float actionX = beside ? origin.x + textLeft + textWidth + gap : origin.x + textLeft;
        const float actionY = beside ? firstLineButtonTop : std::round(top + textHeight + gap);
        if (primaryWidth > 0.0f)
        {
            ImGui::SetCursorScreenPos(ImVec2(actionX, actionY));
            const bool pressed = Button(spec.primary, {.variant = ButtonVariant::Secondary, .size = ControlSize::Small});
            if (spec.open != nullptr)
                escape = DisplayDismissKey(ImGui::GetItemID()) || escape;
            if (pressed && settled)
                result.action = DisplayCalloutAction::Primary;
            actionX += primaryWidth + gap;
        }
        if (secondaryWidth > 0.0f)
        {
            ImGui::SetCursorScreenPos(ImVec2(actionX, actionY));
            const bool pressed = Button(spec.secondary, {.variant = ButtonVariant::Plain, .size = ControlSize::Small});
            if (spec.open != nullptr)
                escape = DisplayDismissKey(ImGui::GetItemID()) || escape;
            if (pressed && settled)
                result.action = DisplayCalloutAction::Secondary;
        }
        if (spec.open != nullptr)
        {
            const bool closed = CalloutCloseButton(ImVec2(bounds.Max.x - pad - closeSize, firstLineButtonTop), &escape);
            if ((closed || escape) && settled)
                result.action = DisplayCalloutAction::Dismissed;
        }
        ImGui::PopID();
        ImGui::PopClipRect();
    }

    // The surface is one item of its current height; the buttons above stay inside it.
    window->DC.CursorMaxPos = savedMax;
    ImGui::SetCursorScreenPos(origin);
    ImGui::ItemSize(bounds.GetSize());
    ApplyNextItemData(next);
    ImGui::ItemAdd(bounds, id, nullptr, ImGuiItemFlags_NoNav);
    if (result.action == DisplayCalloutAction::Dismissed && spec.open != nullptr)
        *spec.open = false;
    return result;
}

} // namespace facet::internal
