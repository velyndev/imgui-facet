// Link: accent text that underlines on hover and focus, optionally opening a URL through Dear
// ImGui's platform hook, with a visited look and a trailing external-link icon for URLs.

#include "facet/widgets/buttons.h"

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
#include "internal/popup_host.h"

namespace facet
{

bool Link(const char* label, const LinkOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    ImGuiContext& g = *GImGui;
    const internal::NextItemData next = internal::TakeNextItemData();
    const internal::ControlMetrics m = internal::GetControlMetrics(options.size);
    internal::ControlTextScope text(m);
    const Theme& t = T();
    const Colors& c = t.colors;

    // One item the width of its text (and the icon of a URL), one text line tall.
    const char* labelEnd = ImGui::FindRenderedTextEnd(label);
    const float textWidth = ImGui::CalcTextSize(label, labelEnd).x;
    const bool external = options.url != nullptr && options.url[0] != '\0';
    const float iconSize = text.IconSize();
    const float iconSlot = external ? m.iconGap * 0.5f + iconSize : 0.0f;
    const float lineHeight = std::round(text.FontSize());
    const float width = internal::ResolveItemWidth(0.0f, next, std::ceil(textWidth + iconSlot));
    const ImGuiID id = window->GetID(label);
    internal::ItemOptions itemOptions;
    itemOptions.baseline = 0.0f;
    internal::ApplyNextItemData(next);
    const internal::ItemState item = internal::ItemBehavior(id, ImVec2(width, lineHeight), itemOptions);

    bool truncated = false;
    if (item.visible)
    {
        ImDrawList* drawList = window->DrawList;
        const ImVec4 color = item.disabled ? c.labelTertiary : (options.visited ? c.accentTextVisited : c.accentText);
        const internal::FittedText shown = internal::FitText(label, labelEnd, std::max(0.0f, width - iconSlot));
        truncated = shown.truncated;
        drawList->AddText(item.rect.Min, ToU32(color), shown.text);
        // The underline marks the link under the pointer and under the keyboard focus, a hairline
        // just under the baseline.
        if (!item.disabled && (item.hovered || item.held || item.focusVisible))
        {
            const float baseline = std::round(item.rect.Min.y + ImGui::GetFontBaked()->Ascent);
            HairlineH(drawList, item.rect.Min.x, item.rect.Min.x + shown.width, baseline + HairlineThickness(), ToU32(color));
        }
        if (external)
            internal::DrawControlIcon(drawList, Icon("external-link"), item.rect.Min.x + shown.width + m.iconGap * 0.5f,
                                      item.rect.GetCenter().y, iconSize, ToU32(color));
        internal::DrawItemFocusRing(item, Px(t.radii.element));
    }

    // The address shows as the tooltip, after a cut label when there is one.
    if (external && internal::BeginPopupTooltip(id, item.rect, item.hovered, item.focusVisible))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, c.label);
        PushTextStyle(TextStyle::Footnote);
        if (truncated)
            ImGui::TextUnformatted(label, labelEnd);
        ImGui::TextUnformatted(options.url);
        PopTextStyle();
        ImGui::PopStyleColor();
        internal::EndPopupTooltip();
    }
    else if (!external)
    {
        internal::CutTextTooltip(item, label, labelEnd, truncated);
    }

    if (item.pressed && external && g.PlatformIO.Platform_OpenInShellFn != nullptr)
        g.PlatformIO.Platform_OpenInShellFn(&g, options.url);
    return item.pressed;
}

} // namespace facet
