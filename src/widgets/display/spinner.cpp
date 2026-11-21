// Spinner (facet/widgets/display.h): a busy indicator as tall as the text of its size, centered
// on the capitals of the line, with the visible label part after it. Every spinner reads the
// shared loading clock; with reduced motion it stands still as three quarters of a ring and fades
// in place. Not interactive: one item that is not a Tab stop.

#include "facet/widgets/display.h"

#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "internal/control.h"
#include "widgets/display/display_look.h"

namespace facet
{

void Spinner(const char* label, const SpinnerOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return;
    if (label == nullptr)
        label = "";
    const Theme& t = T();
    const TextStyle style = ControlTextStyle(options.size);
    internal::ControlTextScope scope(style);
    const char* labelEnd = ImGui::FindRenderedTextEnd(label);
    const bool hasLabel = labelEnd != label;
    const float diameter = scope.IconSize();
    const float gap = Px(t.spacing.sm);
    const float lineHeight = std::round(ImGui::GetTextLineHeight());
    const float textWidth = hasLabel ? std::ceil(ImGui::CalcTextSize(label, labelEnd).x) : 0.0f;
    const float width = diameter + (hasLabel ? gap + textWidth : 0.0f);
    const ImGuiID id = window->GetID(label[0] != '\0' ? label : "##Spinner");
    const internal::DisplayInlineBox box = internal::DisplayPlaceInline(
        window, id, ImVec2(width, lineHeight), std::round(internal::DisplayFontAscent()), style);
    if (!box.visible)
        return;

    const Colors& c = t.colors;
    const bool disabled = (GImGui->LastItemData.ItemFlags & ImGuiItemFlags_Disabled) != 0;
    ImVec4 color = options.tone == Tone::Neutral ? c.labelSecondary : internal::DisplayProgressColor(options.tone);
    if (disabled)
        color = c.labelTertiary;
    const float textTop = internal::DisplayTextTop(box.baseline);
    ImDrawList* drawList = window->DrawList;
    internal::DrawControlSpinner(drawList,
                                 ImVec2(box.rect.Min.x + diameter * 0.5f, std::round(internal::DisplayCapsCenter(textTop))),
                                 diameter, ToU32(color));
    if (hasLabel)
        drawList->AddText(ImVec2(box.rect.Min.x + diameter + gap, textTop), ToU32(internal::ControlSecondaryColor(disabled)),
                          label, labelEnd);
}

} // namespace facet
