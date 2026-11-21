// Legend (facet/widgets/charts.h): color swatches with labels that flow into rows (or stack in
// a column), each entry its own item. With `toggle`, an entry is a Tab stop and a click, Enter or
// Space flips its hidden member; with Alt held the entry is shown alone (or, when it already is,
// every entry comes back). A hidden entry shows a hollow swatch and a secondary label, so the
// state does not depend on color alone. The swatch and label look is the one of the passive
// legends under the charts (chart_parts.h).

#include "facet/widgets/charts.h"

#include <algorithm>
#include <climits>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/state_storage.h"
#include "widgets/charts/chart_parts.h"

namespace facet
{
namespace
{

// Where entry i goes: flowed into rows of `width` (horizontal) or one per row (vertical).
struct LegendFlow
{
    float width = 0.0f;
    float height = 0.0f;  // one entry
    float spacing = 0.0f;
    bool vertical = false;
    float x = 0.0f;
    float y = 0.0f;
    bool any = false;

    ImVec2 Next(float entryWidth)
    {
        if (any && (vertical || x + entryWidth > width))
        {
            x = 0.0f;
            y += height;
        }
        const ImVec2 at(x, y);
        x += entryWidth + spacing;
        any = true;
        return at;
    }
    float Total() const { return any ? y + height : height; }
};

// The entry rectangles of the last frame, for ChartLegendEntryRect.
struct LegendState
{
    ImRect entries[internal::kChartLegendTracked];
    int count = 0;
};

// Shows entry `only` alone, or every entry when it already is the only one shown. Returns true
// when a member changed.
bool LegendIsolate(std::span<LegendItem> items, int only)
{
    bool othersShown = false;
    for (int i = 0; i < static_cast<int>(items.size()); ++i)
        othersShown = othersShown || (i != only && !items[static_cast<std::size_t>(i)].hidden);
    const bool alone = !items[static_cast<std::size_t>(only)].hidden && !othersShown;
    bool changed = false;
    for (int i = 0; i < static_cast<int>(items.size()); ++i)
    {
        LegendItem& item = items[static_cast<std::size_t>(i)];
        const bool hidden = alone ? false : i != only;
        changed = changed || item.hidden != hidden;
        item.hidden = hidden;
    }
    return changed;
}

} // namespace

bool Legend(const char* id, std::span<LegendItem> items, const LegendOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (id == nullptr)
        id = "";
    const internal::NextItemData next = internal::TakeNextItemData();
    const internal::ForcedItemRequest forced = internal::TakeForcedItemState();
    const ImGuiID widgetId = window->GetID(id);
    const bool readOnly = options.readOnly || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    const bool groupDisabled = (next.itemFlags & ImGuiItemFlags_Disabled) != 0;
    const int count = static_cast<int>(std::min<std::size_t>(items.size(), static_cast<std::size_t>(INT_MAX)));

    internal::ControlTextScope text(internal::kChartTextStyle);
    const internal::ChartLegendMetrics m = internal::GetChartLegendMetrics();
    const float width = internal::ResolveItemWidth(options.width, next, kFill);
    const bool vertical = options.orientation == Orientation::Vertical;
    const ImVec2 origin(std::round(window->DC.CursorPos.x), std::round(window->DC.CursorPos.y));

    // Measure every entry in flow order.
    LegendFlow measure{width, m.height, m.spacing, vertical};
    for (int i = 0; i < count; ++i)
        measure.Next(internal::ChartLegendEntryWidth(internal::ChartVisibleLabel(items[static_cast<std::size_t>(i)].label),
                                                     m, width));
    const float height = measure.Total();

    ImGui::BeginGroup();
    ImGui::ItemSize(ImVec2(width, height));
    ImDrawList* drawList = window->DrawList;
    const Colors& c = T().colors;
    LegendState& kept = internal::GetState<LegendState>(widgetId);
    kept.count = std::min(count, internal::kChartLegendTracked);
    LegendFlow flow{width, m.height, m.spacing, vertical};
    int pressed = -1;
    bool alt = false;
    ImGuiID editor = widgetId;
    for (int i = 0; i < count; ++i)
    {
        LegendItem& item = items[static_cast<std::size_t>(i)];
        const internal::ChartLabelText label = internal::ChartVisibleLabel(item.label);
        const float entryWidth = internal::ChartLegendEntryWidth(label, m, width);
        const ImVec2 at = flow.Next(entryWidth);
        const ImRect rect(ImVec2(origin.x + at.x, origin.y + at.y),
                          ImVec2(origin.x + at.x + entryWidth, origin.y + at.y + m.height));
        if (i < internal::kChartLegendTracked)
            kept.entries[i] = rect;
        internal::ChartLegendLook look;
        look.color = internal::ChartDataColor(item.color, i, false);
        look.hidden = item.hidden;
        look.disabled = groupDisabled;

        if (!options.toggle)
        {
            if (ImGui::IsRectVisible(rect.Min, rect.Max))
                internal::DrawChartLegendEntry(drawList, rect, label, look, m);
            continue;
        }
        const ImGuiID entryId = internal::ChartEntryId(widgetId, item.id, i);
        internal::ItemOptions entryOptions;
        entryOptions.readOnly = readOnly;
        entryOptions.handCursor = !readOnly;
        entryOptions.forced = &forced;
        entryOptions.part = i;
        const internal::ItemState state = internal::PartBehavior(entryId, rect, entryOptions);
        look.disabled = state.disabled;
        if (state.pressed && !state.readOnly && !state.disabled)
        {
            pressed = i;
            alt = ImGui::GetIO().KeyAlt;
            editor = entryId;
        }
        if (!state.visible)
            continue;
        // Hover and press feedback behind the entry; nothing at rest, so a legend reads as text.
        if (!state.disabled && !state.readOnly && (state.hoverAmount > 0.0f || state.pressAmount > 0.0f))
        {
            const ImVec4 rest = WithAlpha(c.fill[2], 0.0f);
            FillRect(drawList, rect.Min, rect.Max, ToU32(internal::StateFill(rest, c.fill[2], c.fill[1], state)), m.radius);
        }
        const internal::FittedText shown = internal::DrawChartLegendEntry(drawList, rect, label, look, m);
        internal::DrawItemFocusRing(state, m.radius);
        internal::CutTextTooltip(state, label.text, label.end, shown.truncated);
    }

    // The write, after every entry was drawn with the state it had.
    bool written = false;
    if (pressed >= 0)
    {
        if (alt)
        {
            written = LegendIsolate(items, pressed);
        }
        else
        {
            LegendItem& item = items[static_cast<std::size_t>(pressed)];
            item.hidden = !item.hidden;
            written = true;
        }
    }
    window->DC.CursorMaxPos = ImMax(window->DC.CursorMaxPos, ImVec2(origin.x + width, origin.y + height));
    ImGui::EndGroup();
    internal::FinishWidgetGroup(widgetId);
    if (written)
        internal::MarkWidgetEdited(editor);
    return written;
}

namespace internal
{

ImRect ChartLegendEntryRect(ImGuiID legendId, int index)
{
    const LegendState* state = static_cast<const LegendState*>(GlobalStateStore().Find(legendId, StateTypeOf<LegendState>()));
    if (state == nullptr || index < 0 || index >= state->count)
        return ImRect();
    return state->entries[index];
}

} // namespace internal

} // namespace facet
