// StyledText (facet/widgets/display.h): text in a theme text style and a tone color, on one line
// or wrapped at the item width, aligned inside that width. One item that is not a Tab stop.

#include "facet/widgets/display.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/text.h"
#include "internal/control.h"
#include "internal/item.h"
#include "internal/text_util.h"
#include "widgets/display/display_look.h"

namespace facet
{
namespace
{

// Wrapped lines measured and drawn per batch, so a long text needs no allocation.
constexpr int kStyledTextLineBatch = 32;

// The text color of a tone: readable as text on the content surfaces (display_look.h).
ImVec4 StyledTextColor(const StyledTextOptions& options, bool disabled)
{
    if (disabled)
        return T().colors.labelTertiary;
    if (options.tone == Tone::Neutral)
        return options.secondary ? T().colors.labelSecondary : T().colors.label;
    return internal::DisplayToneLook(options.tone, false).text;
}

float StyledTextAlignedX(float left, float width, float lineWidth, Align align)
{
    switch (align)
    {
    case Align::Center:
        return std::round(left + (width - lineWidth) * 0.5f);
    case Align::End:
        return std::round(left + width - lineWidth);
    case Align::Start:
    default:
        return left;
    }
}

// Calls `draw(line, index)` for every wrapped line of [text, end) at `wrapWidth`, batch by batch,
// and returns the number of lines.
template <class Draw>
int ForEachWrappedLine(const char* text, const char* end, float wrapWidth, Draw&& draw)
{
    ImFont* font = ImGui::GetFont();
    const float size = ImGui::GetFontSize();
    internal::WrapLine lines[kStyledTextLineBatch];
    int index = 0;
    const char* from = text;
    for (;;)
    {
        const int total = internal::WrapText(font, size, from, end, wrapWidth, lines);
        const int batch = std::min(total, kStyledTextLineBatch);
        for (int i = 0; i < batch; ++i)
            draw(lines[i], index + i);
        index += batch;
        if (total <= kStyledTextLineBatch)
            return index;
        // Continue after the last line of the batch: past its trailing spaces and one line break.
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

} // namespace

void StyledText(const char* text, const StyledTextOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return;
    const internal::NextItemData next = internal::TakeNextItemData();
    internal::ControlTextScope scope(options.style);
    const char* end = text != nullptr ? text + std::strlen(text) : nullptr;
    const float lineHeight = ImGui::GetTextLineHeight();

    // Width: the text's own width, or the available width when it wraps (rule 6 otherwise).
    const float ownWidth = text != nullptr ? std::ceil(ImGui::CalcTextSize(text, end).x) : 0.0f;
    const float width = internal::ResolveItemWidth(options.width, next, options.wrap ? kFill : std::max(1.0f, ownWidth));
    int lineCount = 1;
    if (text != nullptr && options.wrap)
        lineCount = std::max(1, ForEachWrappedLine(text, end, width, [](const internal::WrapLine&, int) {}));
    else if (text != nullptr)
        lineCount = 1 + static_cast<int>(std::count(text, end, '\n'));

    // Text sits on the line's text offset, like ImGui::Text, so a styled text after a control on
    // the same line meets the control's label.
    const ImVec2 origin = window->DC.CursorPos;
    const float top = origin.y + window->DC.CurrLineTextBaseOffset;
    const ImVec2 size(width, lineHeight * static_cast<float>(lineCount));
    const ImRect bounds(ImVec2(origin.x, top), ImVec2(origin.x + size.x, top + size.y));
    const ImGuiID id = internal::DisplayAnonymousId(window, text != nullptr && text[0] != '\0' ? text : "##StyledText");
    ImGui::ItemSize(size, 0.0f);
    internal::ApplyNextItemData(next);
    if (!ImGui::ItemAdd(bounds, id, nullptr, ImGuiItemFlags_NoNav) || text == nullptr)
        return;

    const bool disabled = (GImGui->LastItemData.ItemFlags & ImGuiItemFlags_Disabled) != 0;
    const ImU32 color = ToU32(StyledTextColor(options, disabled));
    ImDrawList* drawList = window->DrawList;
    if (options.wrap)
    {
        ForEachWrappedLine(text, end, width, [&](const internal::WrapLine& line, int index) {
            const ImVec2 at(StyledTextAlignedX(bounds.Min.x, width, line.width, options.align),
                            top + lineHeight * static_cast<float>(index));
            drawList->AddText(at, color, line.begin, line.end);
        });
        return;
    }
    // One line per explicit line break; a line longer than the width runs past it (no wrap).
    const char* line = text;
    for (int index = 0; index < lineCount; ++index)
    {
        const char* lineEnd = static_cast<const char*>(std::memchr(line, '\n', static_cast<std::size_t>(end - line)));
        if (lineEnd == nullptr)
            lineEnd = end;
        const float lineWidth = ImGui::CalcTextSize(line, lineEnd).x;
        const float x = width > lineWidth ? StyledTextAlignedX(bounds.Min.x, width, lineWidth, options.align) : bounds.Min.x;
        drawList->AddText(ImVec2(x, top + lineHeight * static_cast<float>(index)), color, line, lineEnd);
        line = lineEnd + 1;
    }
}

} // namespace facet
