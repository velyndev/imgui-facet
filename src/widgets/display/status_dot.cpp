// StatusDot (facet/widgets/display.h): a small dot in a tone, centered on the capitals of the line
// it sits in, with the visible label part after it. "##text" draws the dot alone and shows the text
// after "##" as its tooltip. Not interactive: one item that is not a Tab stop.

#include "facet/widgets/display.h"

#include <cmath>
#include <cstring>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "internal/control.h"
#include "internal/popup_host.h"
#include "widgets/display/display_look.h"

namespace facet
{

void StatusDot(const char* label, const StatusDotOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return;
    if (label == nullptr)
        label = "";
    const Theme& t = T();
    const char* labelEnd = ImGui::FindRenderedTextEnd(label);
    const bool hasLabel = labelEnd != label;

    internal::ControlTextScope scope(TextStyle::Body);
    const float lineHeight = std::round(ImGui::GetTextLineHeight());
    const float baselineInBox = std::round(internal::DisplayFontAscent());
    const float dot = PxSnapped(t.metrics.statusDotSize);
    const float gap = Px(t.spacing.sm);
    const float textWidth = hasLabel ? std::ceil(ImGui::CalcTextSize(label, labelEnd).x) : 0.0f;
    const float width = dot + (hasLabel ? gap + textWidth : 0.0f);
    const ImGuiID id = window->GetID(label[0] != '\0' ? label : "##StatusDot");
    const internal::DisplayInlineBox box =
        internal::DisplayPlaceInline(window, id, ImVec2(width, lineHeight), baselineInBox, TextStyle::Body);
    if (!box.visible)
        return;

    const bool disabled = (GImGui->LastItemData.ItemFlags & ImGuiItemFlags_Disabled) != 0;
    const internal::DisplayToneColors tone = internal::DisplayToneLook(options.tone, disabled);
    ImDrawList* drawList = window->DrawList;
    const float textTop = internal::DisplayTextTop(box.baseline);
    const ImVec2 center(box.rect.Min.x + dot * 0.5f, std::round(internal::DisplayCapsCenter(textTop)));
    internal::DrawDisplayDot(drawList, center, dot, ToU32(tone.glyph), options.pulse && !disabled);
    if (hasLabel)
    {
        drawList->AddText(ImVec2(box.rect.Min.x + dot + gap, textTop), ToU32(internal::ControlLabelColor(disabled)), label,
                          labelEnd);
        return;
    }
    // A dot alone: the text after "##" names it, on hover.
    const char* name = std::strstr(label, "##");
    while (name != nullptr && *name == '#')
        ++name;
    if (name != nullptr && name[0] != '\0')
    {
        if (internal::BeginPopupTooltip(id, box.rect, ImGui::IsItemHovered(), false))
        {
            ImGui::PushStyleColor(ImGuiCol_Text, t.colors.label);
            PushTextStyle(TextStyle::Footnote);
            ImGui::TextUnformatted(name);
            PopTextStyle();
            ImGui::PopStyleColor();
            internal::EndPopupTooltip();
        }
    }
}

} // namespace facet
