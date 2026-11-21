// SectionHeader (facet/widgets/display.h): a Title3 heading with an optional trailing link-style
// action on its line, a wrapped description under it and a hairline divider. Without an action it
// is one item that is not a Tab stop; with one, the action is a Tab stop whose id is the label.

#include "facet/widgets/display.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "widgets/display/display_look.h"

namespace facet
{
namespace
{

// The largest share of the width the trailing action takes; a longer action text is cut.
constexpr float kSectionActionMaxShare = 0.5f;

} // namespace

bool SectionHeader(const char* label, const SectionHeaderOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const internal::NextItemData next = internal::TakeNextItemData();
    const Theme& t = T();
    const Colors& c = t.colors;
    const float width = internal::ResolveItemWidth(options.width, next, kFill);
    const char* labelEnd = ImGui::FindRenderedTextEnd(label);
    const bool hasAction = options.actionLabel != nullptr && options.actionLabel[0] != '\0';

    // Measure: the heading line (Title3), the action in Body on its baseline, the description.
    PushTextStyle(TextStyle::Title3);
    const float titleHeight = ImGui::GetTextLineHeight();
    const float titleAscent = internal::DisplayFontAscent();
    PopTextStyle();
    float actionWidth = 0.0f;
    if (hasAction)
    {
        PushTextStyle(TextStyle::Body);
        actionWidth = std::ceil(ImGui::CalcTextSize(options.actionLabel).x);
        PopTextStyle();
    }
    const float gap = Px(t.spacing.md);
    const float actionRoom = hasAction ? std::min(actionWidth, width * kSectionActionMaxShare) : 0.0f;
    const float titleRoom = std::max(0.0f, width - (hasAction ? actionRoom + gap : 0.0f));
    float descriptionHeight = 0.0f;
    if (options.description != nullptr && options.description[0] != '\0')
        descriptionHeight = Px(t.spacing.xs) + MeasureText(TextStyle::Footnote, options.description, nullptr, width).y;
    const float dividerHeight = options.divider ? Px(t.spacing.sm) + HairlineThickness() : 0.0f;
    const ImVec2 origin = window->DC.CursorPos;
    const ImRect bounds(origin, ImVec2(origin.x + width, origin.y + std::ceil(titleHeight + descriptionHeight + dividerHeight)));
    const ImGuiID id = window->GetID(label);

    // Items: the header as one item, or a group of the header space and the action part.
    internal::ItemState action;
    const float baseline = std::round(origin.y + titleAscent);
    ImRect actionRect;
    if (hasAction)
    {
        PushTextStyle(TextStyle::Body);
        const float actionTop = internal::DisplayTextTop(baseline);
        actionRect = ImRect(ImVec2(bounds.Max.x - actionRoom, actionTop),
                            ImVec2(bounds.Max.x, actionTop + ImGui::GetTextLineHeight()));
        PopTextStyle();
        ImGui::BeginGroup();
        ImGui::ItemSize(bounds.GetSize());
        internal::ApplyNextItemData(next);
        internal::ItemOptions itemOptions;
        itemOptions.baseline = 0.0f;
        action = internal::PartBehavior(id, actionRect, itemOptions);
        ImGui::EndGroup();
        internal::FinishWidgetGroup(id);
    }
    else
    {
        ImGui::ItemSize(bounds.GetSize());
        internal::ApplyNextItemData(next);
        if (!ImGui::ItemAdd(bounds, id, nullptr, ImGuiItemFlags_NoNav))
            return false;
    }
    if (!ImGui::IsRectVisible(bounds.Min, bounds.Max))
        return false;

    const bool disabled = (GImGui->LastItemData.ItemFlags & ImGuiItemFlags_Disabled) != 0 || action.disabled;
    ImDrawList* drawList = window->DrawList;

    // The heading, cut with an ellipsis when the action leaves too little room.
    PushTextStyle(TextStyle::Title3);
    const internal::FittedText title = internal::FitText(label, labelEnd, titleRoom);
    drawList->AddText(ImVec2(origin.x, origin.y), ToU32(internal::ControlLabelColor(disabled)), title.text);
    const ImRect titleRect(origin, ImVec2(origin.x + title.width, origin.y + titleHeight));
    PopTextStyle();

    if (hasAction)
    {
        // A link: accentText, underlined while hovered or pressed so the state does not rest on
        // color alone; labelTertiary while disabled.
        PushTextStyle(TextStyle::Body);
        const internal::FittedText shown = internal::FitText(options.actionLabel, nullptr, actionRect.GetWidth());
        const ImVec4 color = action.disabled ? c.labelTertiary : c.accentText;
        const float x = actionRect.Max.x - shown.width;
        drawList->AddText(ImVec2(x, actionRect.Min.y), ToU32(color), shown.text);
        if (!action.disabled && (action.hovered || action.held))
            HairlineH(drawList, x, x + shown.width, std::round(baseline + Px(t.metrics.hairline)), ToU32(color));
        PopTextStyle();
        const ImRect ring(ImVec2(x, actionRect.Min.y), actionRect.Max);
        internal::DrawFocusRing(drawList, ring, Px(t.radii.element), action.focusAmount);
        internal::CutTextTooltip(action, options.actionLabel, nullptr, shown.truncated);
    }

    float y = origin.y + titleHeight;
    if (descriptionHeight > 0.0f)
    {
        PushTextStyle(TextStyle::Footnote);
        y += Px(t.spacing.xs);
        drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(origin.x, y),
                          ToU32(internal::ControlSecondaryColor(disabled)), options.description, nullptr, width);
        y += descriptionHeight - Px(t.spacing.xs);
        PopTextStyle();
    }
    if (options.divider)
        HairlineH(drawList, bounds.Min.x, bounds.Max.x, std::round(y + Px(t.spacing.sm)), ToU32(c.separator));

    // The cut heading shows in full on hover (and with the action's keyboard focus).
    const bool titleHovered = ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(titleRect.Min, titleRect.Max);
    internal::CutTextTooltip(ImHashStr("##title", 0, id), titleRect, titleHovered, action.focusVisible, label, labelEnd,
                             title.truncated);
    return action.pressed;
}

} // namespace facet
