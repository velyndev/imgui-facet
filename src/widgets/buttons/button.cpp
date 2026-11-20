// Button: a push button in four variants and three sizes, with an optional leading icon, an
// icon-only form and a loading state.
//
// This file is the model for the widgets of every family (docs/writing-a-widget.md). It shows the
// order every widget follows:
//   1. entry: the window to submit to (or the neutral result), the next-item data, the metrics;
//   2. measure the content and resolve the width (docs/api-conventions.md rule 6);
//   3. one item through the item behaviour (src/internal/item.h), which handles hover, press,
//      keyboard activation, focus, disabled, the hit target and forced capture states;
//   4. draw from tokens only: the variant colors (button_look.h), the content, the focus ring;
//   5. the tooltip for a cut label, then the result.

#include "facet/widgets/buttons.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/text.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "widgets/buttons/button_look.h"

namespace facet
{
namespace
{

// What a button shows between its paddings: the icon slot (the icon, or the spinner while
// loading) and the label, measured in the control text style.
struct ButtonContent
{
    const char* label = nullptr;  // the visible part [label, labelEnd)
    const char* labelEnd = nullptr;
    bool hasLabel = false;
    Icon icon;
    bool hasIcon = false;
    float iconSize = 0.0f;        // pixels
    float iconSlot = 0.0f;        // the icon and the gap to the label, pixels; 0 without an icon
    float labelWidth = 0.0f;      // the whole visible label, pixels
};

ButtonContent MeasureButtonContent(const char* label, const ButtonOptions& options, const internal::ControlMetrics& m,
                                   const internal::ControlTextScope& text)
{
    ButtonContent content;
    content.label = label;
    content.labelEnd = ImGui::FindRenderedTextEnd(label);
    content.hasLabel = content.labelEnd != label;
    content.icon = options.icon;
    content.hasIcon = options.icon.IsValid();
    content.iconSize = text.IconSize();
    if (content.hasIcon)
        content.iconSlot = content.iconSize + (content.hasLabel ? m.iconGap : 0.0f);
    if (content.hasLabel)
        content.labelWidth = ImGui::CalcTextSize(label, content.labelEnd).x;
    return content;
}

// The width the button takes by default: the paddings around the content, or one control height
// for an icon-only button, so it is square.
float DefaultButtonWidth(const ButtonContent& content, const internal::ControlMetrics& m)
{
    if (!content.hasLabel)
        return m.height;
    return std::ceil(m.paddingX * 2.0f + content.iconSlot + content.labelWidth);
}

// Draws the icon (or spinner) and the label centered in the button. The label is cut with an
// ellipsis when the button is narrower than its content; returns whether it was cut.
bool DrawButtonContent(ImDrawList* drawList, const ImRect& rect, const ButtonContent& content, bool loading,
                       const internal::ControlMetrics& m, ImU32 color)
{
    const float centerY = rect.GetCenter().y;
    // Without a label or an icon slot, the spinner of a loading button stands in the middle.
    if (loading && !content.hasIcon)
    {
        internal::DrawControlSpinner(drawList, rect.GetCenter(), content.iconSize, color);
        return false;
    }
    const float inner = content.hasLabel ? std::max(0.0f, rect.GetWidth() - m.paddingX * 2.0f) : rect.GetWidth();
    internal::FittedText shown;
    if (content.hasLabel)
        shown = internal::FitText(content.label, content.labelEnd, std::max(0.0f, inner - content.iconSlot));
    float x = std::round(rect.GetCenter().x - (content.iconSlot + shown.width) * 0.5f);
    if (content.hasIcon)
    {
        if (loading)
            internal::DrawControlSpinner(drawList, ImVec2(x + content.iconSize * 0.5f, centerY), content.iconSize, color);
        else
            internal::DrawControlIcon(drawList, content.icon, x, centerY, content.iconSize, color);
        x += content.iconSlot;
    }
    if (content.hasLabel)
        drawList->AddText(ImVec2(x, CenteredTextY(rect.Min.y, rect.GetHeight())), color, shown.text);
    return shown.truncated;
}

} // namespace

bool Button(const char* label, const ButtonOptions& options)
{
    // 1. Entry. Outside a frame, or in a window whose items are skipped, nothing is submitted.
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    // SetNextItemWidth and the other next-item calls are read before the first item is added.
    const internal::NextItemData next = internal::TakeNextItemData();
    const internal::ControlMetrics m = internal::GetControlMetrics(options.size);
    internal::ControlTextScope text(m);

    // 2. Content and width: its content by default, or what rule 6 asks for.
    const ButtonContent content = MeasureButtonContent(label, options, m, text);
    const float width = internal::ResolveItemWidth(options.width, next, DefaultButtonWidth(content, m));

    // 3. One item. The text offset lets stock text after ImGui::SameLine sit on the label's line.
    const ImGuiID id = window->GetID(label);
    const float top = window->DC.CursorPos.y;
    internal::ItemOptions itemOptions;
    itemOptions.baseline = CenteredTextY(top, m.height) - top;
    itemOptions.handCursor = !options.loading;  // a loading button ignores input
    internal::ApplyNextItemData(next);
    const internal::ItemState item = internal::ItemBehavior(id, ImVec2(width, m.height), itemOptions);

    // 4. Draw. A loading button shows its resting look: hover and press do nothing.
    bool truncated = false;
    if (item.visible)
    {
        internal::ItemState look = item;
        if (options.loading)
        {
            look.hoverAmount = 0.0f;
            look.pressAmount = 0.0f;
        }
        const internal::ButtonColors colors = internal::ButtonLook(options.variant, look);
        ImDrawList* drawList = window->DrawList;
        FillRect(drawList, item.rect.Min, item.rect.Max, ToU32(colors.fill), m.radius);
        if (colors.outline.w > 0.0f)
            StrokeRect(drawList, item.rect.Min, item.rect.Max, ToU32(colors.outline), m.radius, HairlineThickness());
        truncated = DrawButtonContent(drawList, item.rect, content, options.loading, m, ToU32(colors.content));
        internal::DrawItemFocusRing(item, m.radius);
    }

    // 5. A cut label shows in full on hover and on keyboard focus (rule 11).
    internal::CutTextTooltip(item, content.label, content.labelEnd, truncated);
    return item.pressed && !options.loading;
}

} // namespace facet
