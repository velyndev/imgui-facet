// EmptyState (facet/widgets/display.h): a centered placeholder for a view without content: an icon,
// the visible label part as the title, a wrapped message and one action button; or, compact, all
// of it on one line for lists and search results. Only the action is a Tab stop.

#include "facet/widgets/display.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/widgets/buttons.h"
#include "internal/control.h"
#include "internal/item.h"
#include "internal/text_util.h"
#include "widgets/display/display_look.h"

namespace facet
{
namespace
{

// Wrapped lines handled per batch, so a long message needs no allocation.
constexpr int kEmptyLineBatch = 16;

// Calls `line(begin, end, width, index)` for every line of [text, end) wrapped at `wrapWidth` in
// the current font; returns the number of lines.
template <class Line>
int EmptyStateLines(const char* text, const char* end, float wrapWidth, Line&& line)
{
    internal::WrapLine lines[kEmptyLineBatch];
    int index = 0;
    const char* from = text;
    for (;;)
    {
        const int total = internal::WrapText(ImGui::GetFont(), ImGui::GetFontSize(), from, end, wrapWidth, lines);
        const int batch = std::min(total, kEmptyLineBatch);
        for (int i = 0; i < batch; ++i)
            line(lines[i], index + i);
        index += batch;
        if (total <= kEmptyLineBatch)
            return index;
        const char* next = lines[batch - 1].end;
        while (next < end && (*next == ' ' || *next == '\r'))
            ++next;
        if (next < end && *next == '\n')
            ++next;
        if (next >= end)
            return index;
        from = next;
    }
}

// The height of [text, end) wrapped and centered at `width` in `style`, and, when `drawList` is
// set, draws it with its first line at `top`.
float EmptyStateText(ImDrawList* drawList, TextStyle style, const char* text, const char* end, float left, float width,
                     float top, ImU32 color)
{
    PushTextStyle(style);
    const float lineHeight = ImGui::GetTextLineHeight();
    const int count = EmptyStateLines(text, end, width, [&](const internal::WrapLine& line, int index) {
        if (drawList != nullptr)
            drawList->AddText(ImVec2(std::round(left + (width - line.width) * 0.5f), top + lineHeight * static_cast<float>(index)),
                              color, line.begin, line.end);
    });
    PopTextStyle();
    return lineHeight * static_cast<float>(count);
}

} // namespace

bool EmptyState(const char* label, const EmptyStateOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const internal::NextItemData next = internal::TakeNextItemData();
    const Theme& t = T();
    const ImGuiID id = window->GetID(label);
    const float width = internal::ResolveItemWidth(options.width, next, kFill);
    const char* titleEnd = ImGui::FindRenderedTextEnd(label);
    const bool hasTitle = titleEnd != label;
    const bool hasMessage = options.message != nullptr && options.message[0] != '\0';
    const bool hasAction = options.actionLabel != nullptr && options.actionLabel[0] != '\0';
    const bool hasIcon = options.icon.IsValid();
    const bool disabled = (next.itemFlags & ImGuiItemFlags_Disabled) != 0;
    const internal::ControlMetrics m = internal::GetControlMetrics(options.compact ? ControlSize::Small : ControlSize::Inherit);
    const float gap = Px(t.spacing.sm);
    const float actionWidth =
        hasAction ? std::ceil(m.paddingX * 2.0f + MeasureText(m.textStyle, options.actionLabel,
                                                               ImGui::FindRenderedTextEnd(options.actionLabel)).x)
                  : 0.0f;
    const internal::DisplayToneColors tone = internal::DisplayToneLook(options.tone, disabled);
    const ImU32 titleColor = ToU32(internal::ControlLabelColor(disabled));
    const ImU32 messageColor = ToU32(internal::ControlSecondaryColor(disabled));

    // Measure the content.
    float contentHeight = 0.0f;
    const float pad = Px(t.spacing.lg);
    const float iconSize = PxSnapped(t.metrics.emptyIconSize);
    float lineHeight = 0.0f;
    if (options.compact)
    {
        lineHeight = MeasureText(TextStyle::Body, "").y;
        contentHeight = std::max(lineHeight, hasAction ? m.height : 0.0f);
    }
    else
    {
        const float textWidth = std::max(1.0f, width - pad * 2.0f);
        if (hasIcon)
            contentHeight += iconSize + Px(t.spacing.md);
        if (hasTitle)
            contentHeight += EmptyStateText(nullptr, TextStyle::Headline, label, titleEnd, 0.0f, textWidth, 0.0f, 0);
        if (hasMessage)
            contentHeight += (hasTitle ? Px(t.spacing.xs) : 0.0f) +
                             EmptyStateText(nullptr, TextStyle::Body, options.message, nullptr, 0.0f, textWidth, 0.0f, 0);
        if (hasAction)
            contentHeight += Px(t.spacing.lg) + m.height;
        contentHeight += pad * 2.0f;
    }
    const float height = std::ceil(options.height > 0.0f ? Px(options.height) : contentHeight);

    const ImVec2 origin = window->DC.CursorPos;
    const ImRect bounds(origin, ImVec2(origin.x + width, origin.y + height));
    const ImVec2 savedMax = window->DC.CursorMaxPos;
    bool pressed = false;
    // Drawn and submitted also while out of sight: Tab reaches the action there and scrolls to it.
    {
        ImDrawList* drawList = window->DrawList;
        ImGui::PushOverrideID(id);
        if (options.compact)
        {
            // One line: [icon] title  message ... [action], the message cut first.
            PushTextStyle(TextStyle::Body);
            const float textTop = CenteredTextY(origin.y, height);
            const float small = std::round(ImGui::GetFontSize());
            float x = origin.x;
            const float right = origin.x + width - (hasAction ? actionWidth + gap : 0.0f);
            if (hasIcon)
            {
                internal::DrawControlIcon(drawList, options.icon, x, internal::DisplayCapsCenter(textTop), small,
                                          ToU32(tone.glyph));
                x += small + gap;
            }
            if (hasTitle)
            {
                const internal::FittedText title = internal::FitText(label, titleEnd, std::max(0.0f, right - x));
                drawList->AddText(ImVec2(x, textTop), titleColor, title.text);
                x += title.width + gap;
            }
            if (hasMessage && x < right)
            {
                const char* lineEnd = std::strchr(options.message, '\n');
                const internal::FittedText message = internal::FitText(options.message, lineEnd, right - x);
                drawList->AddText(ImVec2(x, textTop), messageColor, message.text);
            }
            PopTextStyle();
            if (hasAction)
            {
                ImGui::SetCursorScreenPos(ImVec2(origin.x + width - actionWidth, std::round(origin.y + (height - m.height) * 0.5f)));
                pressed = Button(options.actionLabel, {.size = ControlSize::Small});
            }
        }
        else
        {
            // Stacked and centered in the height.
            const float textWidth = std::max(1.0f, width - pad * 2.0f);
            float y = std::round(origin.y + std::max(0.0f, (height - contentHeight) * 0.5f) + pad);
            if (hasIcon)
            {
                internal::DrawControlIcon(drawList, options.icon, std::round(origin.x + (width - iconSize) * 0.5f),
                                          y + iconSize * 0.5f, iconSize, ToU32(tone.glyph));
                y += iconSize + Px(t.spacing.md);
            }
            if (hasTitle)
                y += EmptyStateText(drawList, TextStyle::Headline, label, titleEnd, origin.x + pad, textWidth, y, titleColor);
            if (hasMessage)
            {
                y += hasTitle ? Px(t.spacing.xs) : 0.0f;
                y += EmptyStateText(drawList, TextStyle::Body, options.message, nullptr, origin.x + pad, textWidth, y,
                                    messageColor);
            }
            if (hasAction)
            {
                y += Px(t.spacing.lg);
                ImGui::SetCursorScreenPos(ImVec2(std::round(origin.x + (width - actionWidth) * 0.5f), y));
                pressed = Button(options.actionLabel, {.size = options.compact ? ControlSize::Small : ControlSize::Inherit});
            }
        }
        ImGui::PopID();
    }

    // The placeholder is one item of its height; the action button stays inside it.
    window->DC.CursorMaxPos = savedMax;
    ImGui::SetCursorScreenPos(origin);
    ImGui::ItemSize(bounds.GetSize());
    internal::ApplyNextItemData(next);
    ImGui::ItemAdd(bounds, id, nullptr, ImGuiItemFlags_NoNav);
    return pressed;
}

} // namespace facet
