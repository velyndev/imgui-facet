// TruncatedText (facet/widgets/display.h): text cut with an ellipsis at the end, in the middle or
// at the start so it fits its width, on up to maxLines lines (End mode). While the text is cut,
// hovering it, or keyboard focus on a focusable one, shows the whole text as a tooltip.

#include "facet/widgets/display.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/text_util.h"
#include "widgets/display/display_look.h"

namespace facet
{
namespace
{

// Lines of a multi-line text measured at most; more lines than this are cut like any other.
constexpr int kTruncatedMaxLines = 32;

internal::TruncateMode TruncatedTextMode(TruncateMode mode)
{
    switch (mode)
    {
    case TruncateMode::Middle:
        return internal::TruncateMode::Middle;
    case TruncateMode::Start:
        return internal::TruncateMode::Start;
    case TruncateMode::End:
    default:
        return internal::TruncateMode::End;
    }
}

// [begin, end) fitted into `width` pixels. `more`: text follows that is not shown (further lines),
// so the fitted text must end in the ellipsis even when it fits.
internal::FittedText TruncatedFit(const char* begin, const char* end, float width, internal::TruncateMode mode, bool more)
{
    internal::FittedText shown = internal::FitText(begin, end, width, mode);
    if (more && !shown.truncated)
    {
        // Cutting at one pixel less than its own width leaves out at least one character.
        shown = internal::FitText(begin, end, std::max(0.0f, shown.width - 1.0f), internal::TruncateMode::End);
        shown.truncated = true;
    }
    return shown;
}

} // namespace

void TruncatedText(const char* text, const TruncatedTextOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return;
    const internal::NextItemData next = internal::TakeNextItemData();
    internal::ControlTextScope scope(options.style);
    const float lineHeight = ImGui::GetTextLineHeight();
    const float width = internal::ResolveItemWidth(options.width, next, kFill);
    const char* end = text != nullptr ? text + std::strlen(text) : nullptr;

    // Lines: one, or for End mode up to maxLines wrapped lines with the ellipsis on the last one.
    const int maxLines = options.mode == TruncateMode::End ? std::clamp(options.maxLines, 1, kTruncatedMaxLines) : 1;
    internal::WrapLine lines[kTruncatedMaxLines];
    int lineCount = 1;
    int wrappedTotal = 1;
    if (text != nullptr && maxLines > 1)
    {
        wrappedTotal = internal::WrapText(ImGui::GetFont(), ImGui::GetFontSize(), text, end, width,
                                          std::span<internal::WrapLine>(lines, static_cast<std::size_t>(maxLines)));
        lineCount = std::clamp(wrappedTotal, 1, maxLines);
    }

    // One item; with `focusable` a Tab stop whose id is the text (it is never activated).
    const ImVec2 origin = window->DC.CursorPos;
    const float top = origin.y + window->DC.CurrLineTextBaseOffset;
    const ImVec2 size(width, lineHeight * static_cast<float>(lineCount));
    const ImGuiID id = internal::DisplayAnonymousId(window, text != nullptr && text[0] != '\0' ? text : "##TruncatedText");
    internal::ItemState item;
    internal::ApplyNextItemData(next);
    if (options.focusable && text != nullptr)
    {
        internal::ItemOptions itemOptions;
        itemOptions.handCursor = false;
        itemOptions.hitTarget = false;
        itemOptions.baseline = 0.0f;
        item = internal::ItemBehavior(id, size, itemOptions);
    }
    else
    {
        const ImRect bounds(ImVec2(origin.x, top), ImVec2(origin.x + size.x, top + size.y));
        ImGui::ItemSize(size, 0.0f);
        item.visible = ImGui::ItemAdd(bounds, id, nullptr, ImGuiItemFlags_NoNav);
        item.id = id;
        item.rect = bounds;
        item.hovered = item.visible && ImGui::IsItemHovered();
        item.disabled = (GImGui->LastItemData.ItemFlags & ImGuiItemFlags_Disabled) != 0;
    }
    if (text == nullptr)
        return;

    const Colors& c = T().colors;
    const ImU32 color = ToU32(item.disabled ? c.labelTertiary : (options.secondary ? c.labelSecondary : c.label));
    ImDrawList* drawList = window->DrawList;
    bool truncated = false;
    if (maxLines > 1)
    {
        // Every line but the last is drawn as wrapped; the last one takes the rest of the
        // paragraph and ends in the ellipsis when anything is left out.
        for (int i = 0; i + 1 < lineCount; ++i)
            if (item.visible)
                drawList->AddText(ImVec2(item.rect.Min.x, item.rect.Min.y + lineHeight * static_cast<float>(i)), color,
                                  lines[i].begin, lines[i].end);
        const char* lastBegin = lines[lineCount - 1].begin;
        const char* lastEnd = wrappedTotal > lineCount ? end : lines[lineCount - 1].end;
        const char* newline = static_cast<const char*>(std::memchr(lastBegin, '\n', static_cast<std::size_t>(lastEnd - lastBegin)));
        if (newline != nullptr)
            lastEnd = newline;
        const internal::FittedText shown =
            TruncatedFit(lastBegin, lastEnd, width, internal::TruncateMode::End, wrappedTotal > lineCount);
        truncated = shown.truncated;
        if (item.visible)
            drawList->AddText(ImVec2(item.rect.Min.x, item.rect.Min.y + lineHeight * static_cast<float>(lineCount - 1)),
                              color, shown.text);
    }
    else
    {
        // One line: the first line break ends it, as if the rest did not fit.
        const char* lineEnd = static_cast<const char*>(std::memchr(text, '\n', static_cast<std::size_t>(end - text)));
        const internal::FittedText shown = TruncatedFit(text, lineEnd != nullptr ? lineEnd : end, width,
                                                        TruncatedTextMode(options.mode), lineEnd != nullptr);
        truncated = shown.truncated;
        if (item.visible)
            drawList->AddText(item.rect.Min, color, shown.text);
    }
    if (options.focusable)
        internal::DrawItemFocusRing(item, Px(T().radii.element));
    internal::CutTextTooltip(item.id, item.rect, item.hovered, item.focusVisible, text, end, truncated);
}

} // namespace facet
