// BeginFlow, FlowNext and EndFlow (layout.h): items of different widths that wrap onto new lines.
//
// Every item is a group started by FlowNext and measured when the next one starts (or at the End).
// The sizes of the last frame are kept under the id; BeginFlow breaks them into lines of the flow's
// width with the gap between items and lines, places each line by `align` and, with maxLines,
// finds the first item that does not fit: on the last allowed line an item must leave room for the
// "+N" chip when more items follow it. From that item on FlowNext returns false, and EndFlow draws
// the chip (a neutral Badge with Strings::moreCount) after the last item shown. A line starts one
// gap below the tallest item of the line above, measured in the same frame. Items the last frame did
// not know yet (the first frame, or items added since) go after the previous item and wrap when it
// already ended past the width. The item widths are stored in a vector under the id, which grows
// when the item count grows and is not touched otherwise.

#include "facet/widgets/layout.h"

#include <algorithm>
#include <cstring>
#include <span>

#include <imgui.h>
#include <imgui_internal.h>

#include "core/core_internal.h"
#include "facet/core/scale.h"
#include "facet/widgets/display.h"
#include "internal/control.h"
#include "internal/scope_stack.h"
#include "internal/state_storage.h"
#include "internal/text_util.h"
#include "widgets/layout/layout_internal.h"

namespace facet
{
namespace
{

// The measured sizes of the last frame and the layout BeginFlow derived from them.
struct FlowMeasure
{
    ImVector<ImVec2> sizes;     // pixels, per item
    ImVector<float> offsets;    // x of each item from the flow's left edge
    ImVector<int> lines;        // the line of each item
    int count = 0;              // items submitted in the last frame
    float chipWidth = 0.0f;     // the "+N" chip of the last frame
    float scale = 0.0f;
    int shown = 0;              // items that fit this frame (the rest are counted in the chip)
    float chipOffset = 0.0f;    // where the chip goes: x and line
    int chipLine = 0;
};

struct FlowEntry
{
    bool active = false;
    ImGuiWindow* window = nullptr;
    ImGuiID id = 0;
    ImVec2 origin = {0.0f, 0.0f};
    float width = 0.0f;
    float gap = 0.0f;
    int maxLines = 0;
    FlowMeasure* measure = nullptr;
    int index = 0;            // items asked for so far
    int open = -1;            // the item whose group is open
    int skipped = 0;
    int line = 0;
    float lineTop = 0.0f;
    float lineBottom = 0.0f;
    float lastRight = 0.0f;   // the right edge of the last item shown, from the left edge
    int lastLine = 0;
    internal::LayoutWorkRect work;
};

struct FlowStack
{
    FlowEntry entries[internal::kScopeStackDepth];
    int depth = 0;
    int overflow = 0;
};

FlowStack& Flows()
{
    static FlowStack stack;
    return stack;
}

float FlowAlignShare(LayoutAlign align)
{
    return align == LayoutAlign::Center ? 0.5f : (align == LayoutAlign::End ? 1.0f : 0.0f);
}

// Breaks the last frame's sizes into lines (see the top of this file).
void LayoutFlow(FlowMeasure& m, float width, float gap, int maxLines, LayoutAlign align)
{
    const int n = m.count;
    if (m.offsets.Size < n)
    {
        m.offsets.resize(n);
        m.lines.resize(n);
    }
    m.shown = n;
    m.chipOffset = 0.0f;
    m.chipLine = 0;
    int line = 0;
    float x = 0.0f;
    int lineStart = 0;
    const auto finishLine = [&](int end, float used, bool last) {
        const int items = end - lineStart;
        if (items <= 0)
            return;
        const float free = std::max(0.0f, width - used);
        if (align == LayoutAlign::Stretch)
        {
            if (!last && items > 1)
                for (int k = 1; k < items; ++k)
                    m.offsets[lineStart + k] += free * static_cast<float>(k) / static_cast<float>(items - 1);
            return;
        }
        const float shift = free * FlowAlignShare(align);
        for (int k = lineStart; k < end; ++k)
            m.offsets[k] += shift;
        if (m.chipLine == line && end == m.shown && m.shown < n)
            m.chipOffset += shift;
    };
    for (int i = 0; i < n; ++i)
    {
        const float w = i < m.sizes.Size ? m.sizes[i].x : 0.0f;
        if (x > 0.0f && x + w > width)
        {
            finishLine(i, x - gap, false);
            ++line;
            x = 0.0f;
            lineStart = i;
        }
        const bool lastLine = maxLines > 0 && line >= maxLines - 1;
        const bool more = i < n - 1;
        if ((maxLines > 0 && line >= maxLines) || (lastLine && more && x > 0.0f && x + w + gap + m.chipWidth > width))
        {
            m.shown = i;
            break;
        }
        m.offsets[i] = x;
        m.lines[i] = line;
        x += w + gap;
    }
    if (m.shown < n)
    {
        m.chipOffset = x;
        m.chipLine = line;
        finishLine(m.shown, x + m.chipWidth, true);
    }
    else
    {
        finishLine(n, x - gap, true);
    }
}

void CloseFlowItem(FlowEntry& entry)
{
    if (entry.open < 0)
        return;
    ImGui::EndGroup();
    const ImRect rect = GImGui->LastItemData.Rect;
    FlowMeasure& m = *entry.measure;
    if (m.sizes.Size <= entry.open)
        m.sizes.resize(entry.open + 1, ImVec2(0.0f, 0.0f));
    m.sizes[entry.open] = rect.GetSize();
    entry.lineBottom = std::max(entry.lineBottom, rect.Max.y);
    entry.lastRight = rect.Max.x - entry.origin.x;
    entry.lastLine = entry.line;
    entry.open = -1;
}

FlowEntry* TopFlow()
{
    FlowStack& stack = Flows();
    if (stack.overflow > 0 || stack.depth == 0)
        return nullptr;
    return &stack.entries[stack.depth - 1];
}

void MoveFlowToLine(FlowEntry& entry, int line)
{
    while (entry.line < line)
    {
        entry.lineTop = entry.lineBottom + entry.gap;
        entry.lineBottom = entry.lineTop;
        ++entry.line;
    }
}

} // namespace

void BeginFlow(const char* id, const FlowOptions& options)
{
    internal::PushScope("BeginFlow");
    FlowStack& stack = Flows();
    if (stack.depth >= internal::kScopeStackDepth)
    {
        ++stack.overflow;
        return;
    }
    FlowEntry& entry = stack.entries[stack.depth++];
    entry = FlowEntry{};
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return;
    if (id == nullptr)
        id = "";
    const internal::NextItemData next = internal::TakeNextItemData();
    entry.active = true;
    entry.window = window;
    entry.id = window->GetID(id);
    entry.width = internal::ResolveItemWidth(options.width, next, kFill);
    entry.gap = internal::LayoutGapPixels(options.gap);
    entry.maxLines = std::max(0, options.maxLines);
    FlowMeasure& m = internal::GetState<FlowMeasure>(entry.id);
    if (m.scale != Scale())
    {
        // Sizes measured at another scale are no guide.
        m.sizes.resize(0);
        m.count = 0;
        m.chipWidth = 0.0f;
        m.scale = Scale();
    }
    LayoutFlow(m, entry.width, entry.gap, entry.maxLines, options.align);
    entry.measure = &m;
    ImGui::BeginGroup();
    entry.origin = window->DC.CursorPos;
    entry.lineTop = entry.origin.y;
    entry.lineBottom = entry.origin.y;
    entry.work = internal::NarrowLayoutWorkRect(window, entry.origin.x + entry.width);
}

bool FlowNext()
{
    FlowEntry* entry = TopFlow();
    const char* innermost = internal::InnermostScope();
    if (entry == nullptr || innermost == nullptr || std::strcmp(innermost, "BeginFlow") != 0)
    {
        internal::ReportUsageError("FlowNext outside a flow (or inside a container nested in one)");
        return false;
    }
    if (!entry->active || GImGui->CurrentWindow != entry->window)
        return false;
    CloseFlowItem(*entry);
    FlowMeasure& m = *entry->measure;
    const int i = entry->index++;
    if (entry->skipped > 0 || (i >= m.shown && i < m.count))
    {
        ++entry->skipped;
        return false;
    }
    float x = 0.0f;
    if (i < m.count)
    {
        MoveFlowToLine(*entry, m.lines[i]);
        x = m.offsets[i];
    }
    else
    {
        // An item the last frame did not know: after the previous one, on a new line when that one
        // already ended past the width.
        x = i == 0 ? 0.0f : entry->lastRight + entry->gap;
        if (i > 0 && x >= entry->width)
        {
            if (entry->maxLines > 0 && entry->line + 1 >= entry->maxLines)
            {
                ++entry->skipped;
                return false;
            }
            MoveFlowToLine(*entry, entry->line + 1);
            x = 0.0f;
        }
    }
    ImGui::SetCursorScreenPos(ImVec2(entry->origin.x + x, entry->lineTop));
    ImGui::BeginGroup();
    entry->open = i;
    return true;
}

void EndFlow()
{
    if (!internal::PopScope("BeginFlow"))
        return;
    FlowStack& stack = Flows();
    if (stack.overflow > 0)
    {
        --stack.overflow;
        return;
    }
    if (stack.depth == 0)
        return;
    FlowEntry& entry = stack.entries[--stack.depth];
    if (!entry.active)
        return;
    if (GImGui->CurrentWindow != entry.window)
    {
        internal::RestoreLayoutWorkRect(entry.work);
        return;  // the caller ended the window inside the flow; Dear ImGui reports that itself
    }
    CloseFlowItem(entry);
    FlowMeasure& m = *entry.measure;

    // The "+N" chip after the last item shown.
    float chipWidth = 0.0f;
    if (entry.skipped > 0)
    {
        char text[32];
        const internal::TextToken token{"count", entry.skipped};
        internal::FormatTokens(text, sizeof(text), GetStrings().moreCount, std::span<const internal::TextToken>(&token, 1));
        float x = entry.lastRight > 0.0f ? entry.lastRight + entry.gap : 0.0f;
        if (m.shown < m.count && m.shown == entry.index - entry.skipped)
        {
            MoveFlowToLine(entry, m.chipLine);
            x = m.chipOffset;
        }
        ImGui::SetCursorScreenPos(ImVec2(entry.origin.x + x, entry.lineTop));
        Badge(text);
        const ImRect chip = GImGui->LastItemData.Rect;
        chipWidth = chip.GetWidth();
        entry.lineBottom = std::max(entry.lineBottom, chip.Max.y);
    }
    internal::RestoreLayoutWorkRect(entry.work);
    ImGui::SetCursorScreenPos(entry.origin);
    ImGui::Dummy(ImVec2(entry.width, entry.lineBottom - entry.origin.y));
    ImGui::EndGroup();

    // The chip keeps its last width while no item is hidden, so the line break that first hides one
    // leaves room for it.
    if (chipWidth > 0.0f)
        m.chipWidth = chipWidth;
    m.count = entry.index;
    if (m.sizes.Size > m.count)
        m.sizes.resize(m.count);
}

namespace internal
{

void ClearFlowStack()
{
    FlowStack& stack = Flows();
    for (int i = stack.depth - 1; i >= 0; --i)
        if (stack.entries[i].active)
            RestoreLayoutWorkRect(stack.entries[i].work);
    stack.depth = 0;
    stack.overflow = 0;
}

} // namespace internal

} // namespace facet
