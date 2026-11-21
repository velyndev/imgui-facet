// Separator and SeparatorText (layout.h): hairline dividers, not interactive.
//
// A horizontal separator is an item as wide as the available width and one hairline tall
// (HairlineThickness: whole pixels, at least one), so the window's item spacing gives the room
// above and below it. A vertical one sits on the current line, after ImGui::SameLine: as tall as
// the line (Dear ImGui's line height of the items before it), or one control height when the line
// is empty. SeparatorText puts the text in Footnote labelSecondary (labelTertiary inside
// ImGui::BeginDisabled) at the start or in the middle of the line, with the hairline on the free
// sides, vertically centered on the text; a text too long for the width ends in an ellipsis and
// shows in full as a tooltip on hover.

#include "facet/widgets/layout.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"

namespace facet
{
namespace
{

ImU32 SeparatorColor(bool opaque)
{
    const Colors& c = T().colors;
    return ToU32(opaque ? c.separatorOpaque : c.separator);
}

} // namespace

void Separator(const SeparatorOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return;
    const float hairline = HairlineThickness();
    const ImU32 color = SeparatorColor(options.opaque);
    if (options.orientation == Orientation::Vertical)
    {
        // After SameLine the current line holds the height of the items before it.
        float height = window->DC.CurrLineSize.y;
        if (height <= 0.0f)
            height = ControlHeight(ControlSize::Inherit);
        const ImVec2 pos = window->DC.CursorPos;
        const ImRect rect(pos, ImVec2(pos.x + hairline, pos.y + height));
        ImGui::ItemSize(rect.GetSize());
        if (!ImGui::ItemAdd(rect, 0, nullptr, ImGuiItemFlags_NoNav))
            return;
        HairlineV(window->DrawList, std::round(pos.x), rect.Min.y, rect.Max.y, color);
        return;
    }
    const float width = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    const ImVec2 pos = window->DC.CursorPos;
    const ImRect rect(pos, ImVec2(pos.x + width, pos.y + hairline));
    ImGui::ItemSize(rect.GetSize());
    if (!ImGui::ItemAdd(rect, 0, nullptr, ImGuiItemFlags_NoNav))
        return;
    HairlineH(window->DrawList, rect.Min.x, rect.Max.x, std::round(pos.y), color);
}

void SeparatorText(const char* text, const SeparatorTextOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return;
    if (text == nullptr)
        text = "";
    ImGuiContext& g = *GImGui;
    const Theme& theme = T();
    const bool disabled = (g.CurrentItemFlags & ImGuiItemFlags_Disabled) != 0;
    const float width = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    const float gap = Px(theme.spacing.sm);
    const ImVec2 pos = window->DC.CursorPos;
    const char* textEnd = text + std::strlen(text);

    const internal::ControlTextScope style(TextStyle::Footnote);
    const float lineHeight = ImGui::GetTextLineHeight();
    const ImRect rect(pos, ImVec2(pos.x + width, pos.y + lineHeight));
    ImGui::ItemSize(rect.GetSize());
    // An id of its own for the tooltip of a cut text; the item itself takes no focus.
    const ImGuiID id = window->GetID("##facet.separator");
    if (!ImGui::ItemAdd(rect, 0, nullptr, ImGuiItemFlags_NoNav))
        return;

    // Room for the text: the width less a hairline stretch and a gap on each free side.
    const float minLine = Px(theme.spacing.lg);
    const float sides = options.centered ? 2.0f : 1.0f;
    const float room = std::max(0.0f, width - (minLine + gap) * sides);
    const internal::FittedText shown = internal::FitText(text, textEnd, room);
    const float textX = options.centered ? std::round(pos.x + (width - shown.width) * 0.5f) : pos.x;
    const float lineY = std::round(pos.y + lineHeight * 0.5f - HairlineThickness() * 0.5f);
    const ImU32 lineColor = SeparatorColor(false);
    ImDrawList* drawList = window->DrawList;
    if (shown.width > 0.0f)
        drawList->AddText(ImVec2(textX, pos.y), ToU32(internal::ControlSecondaryColor(disabled)), shown.text);
    const float leftEnd = shown.width > 0.0f ? textX - gap : rect.Max.x;
    const float rightStart = shown.width > 0.0f ? textX + shown.width + gap : rect.Max.x;
    if (options.centered && leftEnd > rect.Min.x)
        HairlineH(drawList, rect.Min.x, leftEnd, lineY, lineColor);
    if (!options.centered && shown.width <= 0.0f)
        HairlineH(drawList, rect.Min.x, rect.Max.x, lineY, lineColor);
    else if (rightStart < rect.Max.x)
        HairlineH(drawList, rightStart, rect.Max.x, lineY, lineColor);

    const ImRect textRect(ImVec2(textX, pos.y), ImVec2(textX + shown.width, pos.y + lineHeight));
    if (shown.truncated)
    {
        const bool hovered = g.HoveredWindow == window && ImGui::IsMouseHoveringRect(textRect.Min, textRect.Max);
        internal::CutTextTooltip(ImHashStr(text, 0, id), textRect, hovered, false, text, textEnd, true);
    }
}

} // namespace facet
