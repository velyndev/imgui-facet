// CodeView (facet/widgets/display.h): a read-only code block on the grouped surface, with a line
// number gutter that stays in place, the light syntax tint of code_tint.h, highlighted lines, and
// either sideways scrolling or wrapped lines, in a scroll area (scroll.h). The view is one Tab stop:
// while it has keyboard focus the arrows, Page Up and Down, Home and End scroll it and Ctrl+C copies
// the whole text. A copy button sits in the top-right corner. Without wrapping only the visible
// lines are drawn. The line table is built again only when the text changes.

#include "facet/widgets/display.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/widgets/buttons.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/scroll.h"
#include "internal/state_storage.h"
#include "internal/text_util.h"
#include "widgets/display/code_tint.h"
#include "widgets/display/display_look.h"

namespace facet
{
namespace
{

// Lines a view with height 0 shows before it scrolls (display.h).
constexpr int kCodeFitLines = 20;
// A tab advances this many space widths.
constexpr int kCodeTabSpaces = 4;
// Wrapped segments of one line handled per batch, so long lines need no allocation.
constexpr int kCodeWrapBatch = 16;
// Characters a Left or Right press scrolls sideways, in space widths.
constexpr float kCodeSideStep = 4.0f;

struct CodeViewState
{
    std::vector<int> lineStarts;           // byte offset of each line start
    std::vector<unsigned char> inBlock;    // the line starts inside a C++ block comment
    std::string text;                      // the code with a terminator, for the clipboard
    ImGuiID hash = 0;
    std::size_t size = static_cast<std::size_t>(-1);
    CodeLanguage language = CodeLanguage::Plain;
    float widestLine = -1.0f;              // pixels at `measuredSize`; -1 not measured
    float measuredSize = 0.0f;
};

// Builds the line table again when the text or the language changed. Allocates only then.
void UpdateCodeLines(CodeViewState& state, std::string_view code, CodeLanguage language)
{
    const ImGuiID hash = code.empty() ? 0 : ImHashData(code.data(), code.size());
    if (hash == state.hash && code.size() == state.size && language == state.language)
        return;
    state.hash = hash;
    state.size = code.size();
    state.language = language;
    state.widestLine = -1.0f;
    state.text.assign(code.data(), code.size());
    state.lineStarts.clear();
    state.inBlock.clear();
    state.lineStarts.push_back(0);
    for (std::size_t i = 0; i < code.size(); ++i)
        if (code[i] == '\n' && i + 1 <= code.size())
            state.lineStarts.push_back(static_cast<int>(i + 1));
    // A final line break does not open an empty last line.
    if (state.lineStarts.size() > 1 && static_cast<std::size_t>(state.lineStarts.back()) == code.size())
        state.lineStarts.pop_back();
    bool block = false;
    for (std::size_t line = 0; line < state.lineStarts.size(); ++line)
    {
        state.inBlock.push_back(block ? 1 : 0);
        const char* begin = code.data() + state.lineStarts[line];
        const char* end = line + 1 < state.lineStarts.size() ? code.data() + state.lineStarts[line + 1] - 1
                                                              : code.data() + code.size();
        block = internal::CodeTintEndsInBlock(language, begin, end, block);
    }
}

// [begin, end) of line `index`, without its line break and a carriage return before it.
void CodeLine(const CodeViewState& state, std::string_view code, int index, const char** begin, const char** end)
{
    *begin = code.data() + state.lineStarts[static_cast<std::size_t>(index)];
    *end = static_cast<std::size_t>(index) + 1 < state.lineStarts.size()
               ? code.data() + state.lineStarts[static_cast<std::size_t>(index) + 1] - 1
               : code.data() + code.size();
    // The last line ends at the end of the text, which may hold its line break.
    if (*end > *begin && (*end)[-1] == '\n')
        --*end;
    if (*end > *begin && (*end)[-1] == '\r')
        --*end;
}

ImVec4 CodeTintColor(internal::CodeTintKind kind, bool disabled)
{
    const Colors& c = T().colors;
    if (disabled)
        return c.labelTertiary;
    switch (kind)
    {
    case internal::CodeTintKind::Keyword:
        return c.codeKeyword;
    case internal::CodeTintKind::String:
        return c.codeString;
    case internal::CodeTintKind::Number:
        return c.codeNumber;
    case internal::CodeTintKind::Comment:
        return c.labelSecondary;
    case internal::CodeTintKind::Plain:
    default:
        return c.label;
    }
}

// The width of [begin, end) with tabs expanded.
float CodeTextWidth(const char* begin, const char* end, float tabWidth)
{
    float width = 0.0f;
    const char* run = begin;
    for (const char* p = begin; p <= end; ++p)
    {
        if (p == end || *p == '\t')
        {
            if (p > run)
                width += ImGui::CalcTextSize(run, p).x;
            if (p < end)
                width += tabWidth;
            run = p + 1;
        }
    }
    return width;
}

// Draws the part [from, to) of the line [begin, end) at x, tinted; returns the x after it.
float DrawCodeSegment(ImDrawList* drawList, CodeLanguage language, const char* begin, const char* end, const char* from,
                      const char* to, bool startsInBlock, float x, float y, float tabWidth, bool disabled)
{
    bool block = startsInBlock;
    const char* p = begin;
    while (p < end && p < to)
    {
        const internal::CodeTintRun run = internal::NextCodeTintRun(language, begin, &p, end, &block);
        const char* a = std::max(run.begin, from);
        const char* b = std::min(run.end, to);
        if (a >= b)
            continue;
        const ImU32 color = ToU32(CodeTintColor(run.kind, disabled));
        const char* piece = a;
        for (const char* q = a; q <= b; ++q)
        {
            if (q == b || *q == '\t')
            {
                if (q > piece)
                {
                    drawList->AddText(ImVec2(x, y), color, piece, q);
                    x += ImGui::CalcTextSize(piece, q).x;
                }
                if (q < b)
                    x += tabWidth;
                piece = q + 1;
            }
        }
    }
    return x;
}

} // namespace

bool CodeView(const char* id, std::string_view code, const CodeViewOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (id == nullptr)
        id = "";
    const internal::NextItemData next = internal::TakeNextItemData();
    const Theme& t = T();
    const Colors& c = t.colors;
    const ImGuiID viewId = window->GetID(id);
    CodeViewState& state = internal::GetState<CodeViewState>(viewId);
    UpdateCodeLines(state, code, options.language);
    const int lineCount = static_cast<int>(state.lineStarts.size());

    internal::ControlTextScope scope(TextStyle::Body);
    const float lineHeight = std::round(ImGui::GetTextLineHeight());
    const float tabWidth = ImGui::CalcTextSize(" ").x * static_cast<float>(kCodeTabSpaces);
    const float pad = Px(t.spacing.sm);
    const float width = internal::ResolveItemWidth(options.width, next, kFill);
    char digits[16];
    ImFormatString(digits, sizeof(digits), "%d", std::max(lineCount, 1));
    for (char* d = digits; *d != '\0'; ++d)
        *d = '0';
    const float numberWidth = options.lineNumbers ? std::ceil(ImGui::CalcTextSize(digits).x) : 0.0f;
    const float gutter = options.lineNumbers ? numberWidth + Px(t.spacing.md) : 0.0f;
    const float textWidth = std::max(1.0f, width - pad * 2.0f - gutter);

    // Visual lines: one per line, or the wrapped segments of every line.
    int visualLines = lineCount;
    if (options.wrap)
    {
        visualLines = 0;
        internal::WrapLine segments[1];
        for (int i = 0; i < lineCount; ++i)
        {
            const char* begin = nullptr;
            const char* end = nullptr;
            CodeLine(state, code, i, &begin, &end);
            visualLines += std::max(1, internal::WrapText(ImGui::GetFont(), ImGui::GetFontSize(), begin, end, textWidth, segments));
        }
    }
    else if (state.widestLine < 0.0f || state.measuredSize != ImGui::GetFontSize())
    {
        state.widestLine = 0.0f;
        state.measuredSize = ImGui::GetFontSize();
        for (int i = 0; i < lineCount; ++i)
        {
            const char* begin = nullptr;
            const char* end = nullptr;
            CodeLine(state, code, i, &begin, &end);
            state.widestLine = std::max(state.widestLine, CodeTextWidth(begin, end, tabWidth));
        }
    }
    const float fitHeight = static_cast<float>(std::min(std::max(visualLines, 1), kCodeFitLines)) * lineHeight + pad * 2.0f;
    const float height = std::round(options.height > 0.0f ? Px(options.height) : fitHeight);
    const ImVec2 origin = window->DC.CursorPos;
    const ImRect bounds(origin, ImVec2(origin.x + width, origin.y + height));

    // The view as one Tab stop over the area; keys are read while it has keyboard focus. The group
    // makes the whole view the last item.
    ImGui::BeginGroup();
    internal::ApplyNextItemData(next);
    internal::ItemOptions itemOptions;
    itemOptions.handCursor = false;
    itemOptions.hitTarget = false;
    itemOptions.allowOverlap = true;
    const internal::ItemState item = internal::PartBehavior(viewId, bounds, itemOptions);
    const bool focused = item.focused;
    if (focused)
        for (const ImGuiKey key : {ImGuiKey_UpArrow, ImGuiKey_DownArrow, ImGuiKey_LeftArrow, ImGuiKey_RightArrow,
                                   ImGuiKey_PageUp, ImGuiKey_PageDown, ImGuiKey_Home, ImGuiKey_End})
            ImGui::SetKeyOwner(key, viewId);

    // The surface: the grouped background with a hairline, so the block reads on any surface.
    const float radius = Px(t.radii.control);
    const bool disabled = item.disabled;
    ImDrawList* parentList = window->DrawList;
    FillRect(parentList, bounds.Min, bounds.Max, ToU32(c.bgSecondary), radius);
    StrokeRect(parentList, bounds.Min, bounds.Max, ToU32(c.separator), radius, HairlineThickness());

    bool copied = false;
    if (focused && ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_C, ImGuiInputFlags_None, viewId))
    {
        ImGui::SetClipboardText(state.text.c_str());
        internal::StartDisplayCopiedFeedback(viewId, bounds);
        copied = true;
    }

    internal::ScrollAreaDesc area;
    area.size = bounds.GetSize();
    area.horizontal = !options.wrap;
    area.fadeSurface = c.bgSecondary;
    area.padding = pad;
    ImGui::PushOverrideID(viewId);
    if (internal::BeginScrollAreaCore("##code", area))
    {
        ImGuiWindow* view = ImGui::GetCurrentWindow();
        // Keys scroll the area while the view has focus.
        if (focused)
        {
            const ImVec2 target = internal::ScrollAreaTarget(view);
            ImVec2 to = target;
            const float page = std::max(lineHeight, view->InnerRect.GetHeight() - lineHeight);
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, ImGuiInputFlags_Repeat, viewId))
                to.y -= lineHeight;
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, ImGuiInputFlags_Repeat, viewId))
                to.y += lineHeight;
            if (ImGui::IsKeyPressed(ImGuiKey_PageUp, ImGuiInputFlags_Repeat, viewId))
                to.y -= page;
            if (ImGui::IsKeyPressed(ImGuiKey_PageDown, ImGuiInputFlags_Repeat, viewId))
                to.y += page;
            if (ImGui::IsKeyPressed(ImGuiKey_Home, ImGuiInputFlags_None, viewId))
                to.y = 0.0f;
            if (ImGui::IsKeyPressed(ImGuiKey_End, ImGuiInputFlags_None, viewId))
                to.y = view->ScrollMax.y;
            if (!options.wrap && ImGui::IsKeyPressed(ImGuiKey_LeftArrow, ImGuiInputFlags_Repeat, viewId))
                to.x -= tabWidth / kCodeTabSpaces * kCodeSideStep;
            if (!options.wrap && ImGui::IsKeyPressed(ImGuiKey_RightArrow, ImGuiInputFlags_Repeat, viewId))
                to.x += tabWidth / kCodeTabSpaces * kCodeSideStep;
            to.x = std::clamp(to.x, 0.0f, view->ScrollMax.x);
            to.y = std::clamp(to.y, 0.0f, view->ScrollMax.y);
            if (to.x != target.x || to.y != target.y)
                internal::ScrollAreaTo(view, to);
        }
        // A click in the code gives the view keyboard focus, as a click on any item does. It is taken on
        // the release: on the press Dear ImGui focuses the area window itself at the end of the frame.
        const ImGuiIO& io = ImGui::GetIO();
        if (ImGui::IsWindowHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !disabled &&
            view->InnerRect.Contains(io.MouseClickedPos[ImGuiMouseButton_Left]) && !ImGui::IsMouseDragPastThreshold(ImGuiMouseButton_Left))
            ImGui::SetFocusID(viewId, window);

        const ImVec2 contentOrigin = view->DC.CursorPos;
        const float contentWidth = options.wrap ? textWidth + gutter : gutter + state.widestLine;
        const float contentHeight = static_cast<float>(std::max(visualLines, 1)) * lineHeight;
        ImGui::ItemSize(ImVec2(contentWidth, contentHeight));
        ImDrawList* drawList = view->DrawList;
        const ImRect visible = view->InnerRect;
        // The gutter stays at the left edge while the code scrolls sideways.
        const float gutterLeft = visible.Min.x + view->WindowPadding.x;
        const float codeLeft = contentOrigin.x + gutter;
        const float codeClip = gutterLeft + gutter;
        const ImU32 numberColor = ToU32(internal::ControlSecondaryColor(disabled));

        auto highlighted = [&](int line) {
            return std::find(options.highlightLines.begin(), options.highlightLines.end(), line + 1) !=
                   options.highlightLines.end();
        };
        auto drawNumber = [&](int line, float y) {
            if (!options.lineNumbers)
                return;
            char number[16];
            ImFormatString(number, sizeof(number), "%d", line + 1);
            const float w = ImGui::CalcTextSize(number).x;
            drawList->AddText(ImVec2(std::round(gutterLeft + numberWidth - w), y), numberColor, number);
        };
        auto drawHighlight = [&](float y, float h) {
            if (!disabled)
                drawList->AddRectFilled(ImVec2(visible.Min.x, y), ImVec2(visible.Max.x, y + h), ToU32(c.accentSubtle));
        };

        if (!options.wrap)
        {
            const float scroll = view->Scroll.y;
            const int first = std::clamp(static_cast<int>(std::floor((scroll - view->WindowPadding.y) / lineHeight)), 0, lineCount);
            const int last = std::clamp(static_cast<int>(std::ceil((scroll + visible.GetHeight()) / lineHeight)) + 1, 0, lineCount);
            for (int i = first; i < last; ++i)
            {
                const float y = contentOrigin.y + lineHeight * static_cast<float>(i);
                if (highlighted(i))
                    drawHighlight(y, lineHeight);
                const char* begin = nullptr;
                const char* end = nullptr;
                CodeLine(state, code, i, &begin, &end);
                drawList->PushClipRect(ImVec2(codeClip, visible.Min.y), visible.Max, true);
                DrawCodeSegment(drawList, options.language, begin, end, begin, end, state.inBlock[static_cast<std::size_t>(i)] != 0,
                                codeLeft, y, tabWidth, disabled);
                drawList->PopClipRect();
                drawNumber(i, y);
            }
        }
        else
        {
            // Wrapped: every line, with its number on its first segment.
            float y = contentOrigin.y;
            for (int i = 0; i < lineCount; ++i)
            {
                const char* begin = nullptr;
                const char* end = nullptr;
                CodeLine(state, code, i, &begin, &end);
                internal::WrapLine segments[kCodeWrapBatch];
                const int total = std::max(1, internal::WrapText(ImGui::GetFont(), ImGui::GetFontSize(), begin, end, textWidth, segments));
                const float lineTop = y;
                if (highlighted(i))
                    drawHighlight(lineTop, lineHeight * static_cast<float>(total));
                const bool inView = lineTop + lineHeight * static_cast<float>(total) >= visible.Min.y && lineTop <= visible.Max.y;
                if (inView)
                {
                    const bool startsInBlock = state.inBlock[static_cast<std::size_t>(i)] != 0;
                    if (begin == end)
                        DrawCodeSegment(drawList, options.language, begin, end, begin, end, startsInBlock, codeLeft, y, tabWidth, disabled);
                    for (int s = 0; s < std::min(total, kCodeWrapBatch); ++s)
                    {
                        // Each segment draws its own range; a segment's leading spaces stay with it.
                        const char* from = segments[s].begin;
                        const char* to = s + 1 < std::min(total, kCodeWrapBatch) ? segments[s + 1].begin : end;
                        DrawCodeSegment(drawList, options.language, begin, end, from, to, startsInBlock, codeLeft,
                                        y + lineHeight * static_cast<float>(s), tabWidth, disabled);
                    }
                    drawNumber(i, lineTop);
                }
                y += lineHeight * static_cast<float>(total);
            }
        }

        // The copy button in the top-right corner of the visible area; it does not count for the
        // content size.
        if (options.copyButton)
        {
            const ImVec2 savedMax = view->DC.CursorMaxPos;
            const float button = ControlHeight(ControlSize::Small);
            ImGui::SetCursorScreenPos(ImVec2(visible.Max.x - button - Px(t.spacing.xs), visible.Min.y + Px(t.spacing.xs)));
            if (CopyButton("##copy", state.text.c_str(), {.size = ControlSize::Small}))
                copied = true;
            view->DC.CursorMaxPos = savedMax;
        }
        internal::EndScrollAreaCore();
    }
    ImGui::PopID();
    // The view takes its whole rectangle also when the area is out of sight.
    window->DC.CursorMaxPos = ImMax(window->DC.CursorMaxPos, bounds.Max);
    ImGui::EndGroup();
    internal::FinishWidgetGroup(viewId);
    internal::DrawFocusRing(parentList, bounds, radius, item.focusAmount);
    internal::DisplayCopiedFeedback(viewId, bounds);
    return copied;
}

} // namespace facet
