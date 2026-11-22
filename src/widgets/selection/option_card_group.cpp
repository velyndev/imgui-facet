// OptionCardGroup: large selectable cards in a grid with a title, a description, an icon or a
// picture and a badge. One choice (a radio mark, one Tab stop with the arrow keys) or several (a
// checkbox mark, a Tab stop per card).

#include "facet/widgets/selection.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/image.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "widgets/selection/selection_parts.h"

namespace facet
{
namespace
{

// The measures every card of a group shares, in pixels.
struct CardLayout
{
    int columns = 1;
    float cardWidth = 0.0f;
    float gap = 0.0f;           // between cards, both ways
    float padding = 0.0f;       // inside a card
    float imageHeight = 0.0f;   // a picture across the top
    float titleHeight = 0.0f;   // the title line (Headline)
    float lineGap = 0.0f;       // between the title and the description
    float mark = 0.0f;          // the radio or checkbox mark in the top-right corner
};

// The height of `card` laid out `layout.cardWidth` wide.
float CardHeight(const OptionCard& card, const CardLayout& layout)
{
    float height = layout.padding * 2.0f + std::max(layout.titleHeight, layout.mark);
    if (card.image.IsValid())
        height += layout.imageHeight + layout.padding;
    if (card.description != nullptr && card.description[0] != '\0')
    {
        internal::ControlTextScope text(TextStyle::Footnote);
        const float wrap = std::max(1.0f, layout.cardWidth - layout.padding * 2.0f);
        height += layout.lineGap + ImGui::CalcTextSize(card.description, nullptr, false, wrap).y;
    }
    return std::ceil(height);
}

// The height of row `row`: its tallest card, so the cards of a row line up.
float RowHeight(std::span<const OptionCard> cards, const CardLayout& layout, int row)
{
    float height = 0.0f;
    const int count = static_cast<int>(cards.size());
    for (int i = row * layout.columns; i < std::min(count, (row + 1) * layout.columns); ++i)
        height = std::max(height, CardHeight(cards[static_cast<std::size_t>(i)], layout));
    return height;
}

bool OptionCardGroupImpl(const char* label, int* index, std::span<bool> selected, bool multi,
                         std::span<const OptionCard> cards, const OptionCardGroupOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    ImGuiContext& g = *GImGui;
    const internal::NextItemData next = internal::TakeNextItemData();
    const internal::ForcedItemRequest forced = internal::TakeForcedItemState();
    const internal::ControlMetrics m = internal::GetControlMetrics(ControlSize::Inherit);
    const Theme& t = T();
    const Colors& c = t.colors;
    const ImGuiID widgetId = window->GetID(label);
    const int count = static_cast<int>(cards.size());
    const bool readOnly = options.readOnly || (!multi && index == nullptr) || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    const bool groupDisabled = (next.itemFlags & ImGuiItemFlags_Disabled) != 0;
    const auto hasFlag = [&](int i) { return i < static_cast<int>(selected.size()); };
    const auto usable = [&](int i) { return !cards[static_cast<std::size_t>(i)].disabled && (!multi || hasFlag(i)); };
    const auto isOn = [&](int i) {
        return multi ? hasFlag(i) && selected[static_cast<std::size_t>(i)] : (index != nullptr && *index == i);
    };

    // ---- Layout ----
    const float width = internal::ResolveItemWidth(options.width, next, kFill);
    const internal::AboveLabel above = internal::MeasureAboveLabel(label, m.size);
    CardLayout layout;
    layout.gap = std::round(Px(t.spacing.md));
    layout.padding = std::round(Px(t.spacing.md));
    layout.imageHeight = std::round(Px(t.metrics.thumbnailSize));
    layout.lineGap = std::round(Px(t.spacing.xs));
    layout.mark = std::max(1.0f, PxSnapped(t.metrics.checkboxSize));
    {
        internal::ControlTextScope title(TextStyle::Headline);
        layout.titleHeight = std::round(title.FontSize());
    }
    const float wanted = Px(options.cardWidth > 0.0f ? options.cardWidth : t.metrics.cardWidth);
    int columns = options.columns > 0 ? options.columns : static_cast<int>((width + layout.gap) / (wanted + layout.gap));
    columns = std::clamp(columns, 1, std::max(1, count));
    layout.columns = columns;
    layout.cardWidth = std::max(1.0f, std::floor((width - layout.gap * static_cast<float>(columns - 1)) / static_cast<float>(columns)));
    const int rows = (count + columns - 1) / columns;
    float gridHeight = 0.0f;
    for (int r = 0; r < rows; ++r)
        gridHeight += RowHeight(cards, layout, r) + (r > 0 ? layout.gap : 0.0f);

    const ImVec2 origin(std::round(window->DC.CursorPos.x), std::round(window->DC.CursorPos.y));
    const float gridTop = origin.y + above.height;
    ImGui::BeginGroup();
    ImGui::ItemSize(ImVec2(width, above.height + gridHeight));
    ImDrawList* drawList = window->DrawList;
    if (above.text != nullptr)
        internal::DrawAboveLabel(drawList, above, origin, width, m.size, groupDisabled);

    const auto idOf = [&](int i) { return internal::SelectionEntryId(widgetId, cards[static_cast<std::size_t>(i)].id, i); };
    const auto enabled = [&](int i) { return !groupDisabled && usable(i); };
    const int current = !multi && index != nullptr ? *index : -1;
    const int stop = multi ? -1 : internal::SelectionRovingStop(count, current, enabled, idOf);
    int chosen = current;
    int flipped = -1;
    int focused = -1;
    ImGuiID editor = widgetId;
    float rowTop = gridTop;
    // An error message turns the outline of every card and mark destructive.
    const internal::ControlMessage message = internal::PickControlMessage(options.error, options.helpText);
    for (int r = 0; r < rows; ++r)
    {
        const float rowHeight = RowHeight(cards, layout, r);
        for (int i = r * columns; i < std::min(count, (r + 1) * columns); ++i)
        {
            const OptionCard& card = cards[static_cast<std::size_t>(i)];
            const int column = i - r * columns;
            const float x = origin.x + static_cast<float>(column) * (layout.cardWidth + layout.gap);
            const ImRect rect(x, rowTop, x + layout.cardWidth, rowTop + rowHeight);
            const ImGuiID cardId = idOf(i);
            internal::ItemOptions itemOptions;
            itemOptions.disabled = !usable(i);
            itemOptions.readOnly = readOnly;
            itemOptions.tabStop = multi || i == stop;
            itemOptions.handCursor = !readOnly;
            itemOptions.part = i;
            itemOptions.forced = &forced;
            const internal::ItemState item = internal::PartBehavior(cardId, rect, itemOptions);
            const bool live = item.forced == internal::ForcedItemState::None;
            if (item.focused && g.NavId == cardId && live)
                focused = i;
            if (item.pressed && !item.readOnly && live)
            {
                editor = cardId;
                if (multi)
                    flipped = i;
                else
                    chosen = i;
            }

            const bool on = isOn(i);
            const float onAmount = internal::SelectionMarkAmount(cardId, on);
            if (!item.visible)
                continue;
            // The card: bgSecondary with the one controlBorder hairline (controlBorderHover under the
            // pointer); selected, the selected tile look of the rows rule: the accentSubtle tint and
            // the accent hairline at the same width. Read-only selected keeps the quiet hairline and
            // takes fill[0]; hover and press tint the card further.
            const float radius = Px(t.radii.card);
            const bool off = item.disabled;
            FillRect(drawList, rect.Min, rect.Max, ToU32(off ? c.fill[3] : c.bgSecondary), radius);
            if (!off && onAmount > 0.0f)
            {
                const ImVec4 tint = item.readOnly ? c.fill[0] : c.accentSubtle;
                FillRect(drawList, rect.Min, rect.Max, ToU32(WithAlpha(tint, tint.w * onAmount)), radius);
            }
            const float feedback = off || item.readOnly ? 0.0f : std::max(item.hoverAmount, item.pressAmount);
            if (feedback > 0.0f)
            {
                const ImVec4 tint = Mix(c.fill[2], c.fill[1], item.pressAmount);
                FillRect(drawList, rect.Min, rect.Max, ToU32(WithAlpha(tint, tint.w * feedback)), radius);
            }
            internal::OutlineLook edge;
            edge.hoverAmount = off || item.readOnly ? 0.0f : item.hoverAmount;
            edge.disabled = off;
            edge.error = message.error;
            const ImVec4 boundary = internal::ControlOutlineColor(edge);
            const bool accentEdge = !off && !item.readOnly && !message.error;
            internal::DrawOutline(drawList, rect, radius, accentEdge ? Mix(boundary, c.focusRing, onAmount) : boundary);

            float y = rect.Min.y + layout.padding;
            const float innerLeft = rect.Min.x + layout.padding;
            const float innerRight = rect.Max.x - layout.padding;
            if (card.image.IsValid())
            {
                DrawImage(drawList, card.image, ImVec2(innerLeft, y), ImVec2(innerRight, y + layout.imageHeight),
                          {.fit = ImageFit::Cover, .radius = Px(t.radii.control), .tint = off ? ScaleAlpha(IM_COL32_WHITE, internal::kDisabledPictureOpacity) : IM_COL32_WHITE});
                y += layout.imageHeight + layout.padding;
            }
            // The mark in the top-right corner of the text area: a radio mark for one choice, a
            // checkbox for several.
            const float lineHeight = std::max(layout.titleHeight, layout.mark);
            const ImRect markRect = internal::SelectionMarkRect(innerRight - layout.mark, y + lineHeight * 0.5f);
            if (multi)
                internal::DrawSelectionCheckbox(drawList, markRect, internal::SelectionMark::On, onAmount, item, message.error);
            else
                internal::DrawSelectionRadio(drawList, markRect, onAmount, item, message.error);
            float right = markRect.Min.x - m.iconGap;
            // The badge: short status text in a pill left of the mark.
            if (card.badge != nullptr && card.badge[0] != '\0')
            {
                internal::ControlTextScope caption(TextStyle::Caption);
                const ImVec2 size = ImGui::CalcTextSize(card.badge);
                const float padX = std::round(Px(t.spacing.sm));
                const float padY = std::round(Px(t.spacing.xs) * 0.5f);
                const ImRect badge(std::round(right - size.x - padX * 2.0f), std::round(y + lineHeight * 0.5f - size.y * 0.5f - padY),
                                   right, std::round(y + lineHeight * 0.5f + size.y * 0.5f + padY));
                if (badge.Min.x > innerLeft)
                {
                    FillRect(drawList, badge.Min, badge.Max, ToU32(off ? c.fill[3] : c.fill[1]), badge.GetHeight() * 0.5f);
                    drawList->AddText(ImVec2(badge.Min.x + padX, badge.Min.y + padY), ToU32(internal::ControlSecondaryColor(off)),
                                      card.badge);
                    right = badge.Min.x - m.iconGap;
                }
            }
            float titleLeft = innerLeft;
            if (!card.image.IsValid() && card.icon.IsValid())
            {
                internal::ControlTextScope body(TextStyle::Body);
                internal::DrawControlIcon(drawList, card.icon, innerLeft, y + lineHeight * 0.5f, body.IconSize(),
                                          ToU32(internal::ControlLabelColor(off)));
                titleLeft += body.IconSize() + m.iconGap;
            }
            const char* title = card.label != nullptr ? card.label : "";
            bool truncated = false;
            {
                internal::ControlTextScope titleText(TextStyle::Headline);
                const char* titleEnd = ImGui::FindRenderedTextEnd(title);
                const internal::FittedText shown = internal::FitText(title, titleEnd, std::max(0.0f, right - titleLeft));
                truncated = shown.truncated;
                drawList->AddText(ImVec2(titleLeft, std::round(y + (lineHeight - titleText.FontSize()) * 0.5f)),
                                  ToU32(internal::ControlLabelColor(off)), shown.text);
            }
            y += lineHeight + layout.lineGap;
            if (card.description != nullptr && card.description[0] != '\0')
            {
                internal::ControlTextScope footnote(TextStyle::Footnote);
                drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(innerLeft, y),
                                  ToU32(internal::ControlSecondaryColor(off)), card.description, nullptr,
                                  std::max(1.0f, innerRight - innerLeft));
            }
            // Inside the card, as for every tile of a grid: never cut by a neighbour, never one ring
            // around another with the accent hairline of a selected card.
            internal::DrawFocusRingInside(drawList, rect, radius, item.focusAmount);
            internal::CutTextTooltip(item, title, ImGui::FindRenderedTextEnd(title), truncated);
        }
        rowTop += rowHeight + layout.gap;
    }

    // One choice: the arrow keys move in the grid (Left and Right along the reading order, Up and
    // Down by a row), skipping disabled cards and stopping at the edges.
    if (!multi && focused >= 0)
    {
        const ImGuiID owner = idOf(focused);
        const auto take = [owner](ImGuiKey key) {
            ImGui::SetKeyOwner(key, owner);
            return ImGui::IsKeyPressed(key, ImGuiInputFlags_Repeat, owner);
        };
        const bool left = take(ImGuiKey_LeftArrow);
        const bool right = take(ImGuiKey_RightArrow);
        const bool up = take(ImGuiKey_UpArrow);
        const bool down = take(ImGuiKey_DownArrow);
        const int step = right ? 1 : (left ? -1 : (down ? columns : (up ? -columns : 0)));
        int target = -1;
        for (int i = focused + step; step != 0 && i >= 0 && i < count; i += step)
            if (enabled(i))
            {
                target = i;
                break;
            }
        if (target >= 0)
        {
            internal::SelectionMoveFocus(idOf(target));
            if (!readOnly)
            {
                chosen = target;
                editor = idOf(target);
            }
        }
    }

    bool written = false;
    if (!readOnly)
    {
        if (!multi && index != nullptr && chosen != current && chosen >= 0 && chosen < count)
        {
            *index = chosen;
            written = true;
        }
        if (multi && flipped >= 0 && hasFlag(flipped))
        {
            selected[static_cast<std::size_t>(flipped)] = !selected[static_cast<std::size_t>(flipped)];
            written = true;
        }
    }

    const ImRect gridRect(origin.x, gridTop, origin.x + width, gridTop + gridHeight);
    const float bottom = internal::DrawControlMessage(message, ImVec2(origin.x, gridRect.Max.y), width, groupDisabled, widgetId, gridRect);
    window->DC.CursorMaxPos = ImMax(window->DC.CursorMaxPos, ImVec2(gridRect.Max.x, bottom));
    ImGui::EndGroup();
    internal::FinishWidgetGroup(widgetId);
    if (written)
        internal::MarkWidgetEdited(editor);
    return written;
}

} // namespace

bool OptionCardGroup(const char* label, int* index, std::span<const OptionCard> cards, const OptionCardGroupOptions& options)
{
    return OptionCardGroupImpl(label, index, {}, false, cards, options);
}

bool OptionCardGroup(const char* label, std::span<bool> selected, std::span<const OptionCard> cards,
                     const OptionCardGroupOptions& options)
{
    return OptionCardGroupImpl(label, nullptr, selected, true, cards, options);
}

} // namespace facet
