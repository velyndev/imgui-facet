// DonutChart (facet/widgets/charts.h): a ring (or a pie) of slices proportional to their values,
// starting at the top and running clockwise, with the surface showing between slices so
// neighbouring colors never touch. The center of a ring shows the total, or the picked slice. The
// chart is one item and one Tab stop; the pointer or the arrow keys pick a slice, which grows out
// a little (quick motion, none while motion is reduced). Slices below minSliceFraction merge into
// one "other" slice in labelSecondary at the end.

#include "facet/widgets/charts.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/anim.h"
#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
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

constexpr float kPi = std::numbers::pi_v<float>;
// Slices start at the top.
constexpr float kDonutStart = -0.5f * kPi;
// Slices whose grow motion is animated; later slices grow at once.
constexpr int kDonutAnimated = 256;

double DonutValue(const DonutSlice& slice)
{
    return std::isfinite(slice.value) && slice.value > 0.0 ? slice.value : 0.0;
}

// The slices as drawn: the ones kept in order, then the merged "other" slice when any merged.
struct DonutParts
{
    std::span<const DonutSlice> slices;
    double total = 0.0;
    double minValue = 0.0;     // slices below this merge
    double other = 0.0;        // their sum
    int kept = 0;
    bool hasOther = false;
    const char* otherLabel = nullptr;

    bool Merged(int i) const { return minValue > 0.0 && DonutValue(slices[static_cast<std::size_t>(i)]) < minValue; }
    int Count() const { return kept + (hasOther ? 1 : 0); }
};

DonutParts MakeDonutParts(std::span<const DonutSlice> slices, const DonutChartOptions& o)
{
    DonutParts parts;
    parts.slices = slices;
    for (const DonutSlice& slice : slices)
        parts.total += DonutValue(slice);
    const float fraction = std::isfinite(o.minSliceFraction) ? std::clamp(o.minSliceFraction, 0.0f, 1.0f) : 0.0f;
    parts.minValue = parts.total * fraction;
    for (int i = 0; i < static_cast<int>(slices.size()); ++i)
    {
        if (parts.Merged(i))
        {
            parts.other += DonutValue(slices[static_cast<std::size_t>(i)]);
            parts.hasOther = parts.hasOther || DonutValue(slices[static_cast<std::size_t>(i)]) > 0.0;
        }
        else
        {
            ++parts.kept;
        }
    }
    parts.otherLabel = o.otherLabel != nullptr ? o.otherLabel : GetStrings().more;
    return parts;
}

// Drawn slice `k`: its value, label, color index (the slice index, so colors follow the data).
struct DonutPart
{
    double value = 0.0;
    const char* label = nullptr;
    ImU32 color = 0;
    int index = -1;  // the slice index; -1 for "other"
};

DonutPart DonutPartAt(const DonutParts& parts, int k)
{
    int seen = 0;
    for (int i = 0; i < static_cast<int>(parts.slices.size()); ++i)
    {
        if (parts.Merged(i))
            continue;
        if (seen++ == k)
        {
            const DonutSlice& slice = parts.slices[static_cast<std::size_t>(i)];
            return {DonutValue(slice), slice.label, slice.color, i};
        }
    }
    return {parts.other, parts.otherLabel, 0, -1};
}

ImVec4 DonutPartColor(const DonutPart& part, bool disabled)
{
    if (part.index < 0)
        return disabled ? T().colors.labelTertiary : T().colors.labelSecondary;
    return internal::ChartDataColor(part.color, part.index, disabled);
}

int DonutFormat(char* buffer, int size, double value, const DonutChartOptions& o)
{
    const ChartAxis axis{.format = o.format != nullptr ? o.format : "%g", .formatter = o.formatter, .formatterUser = o.formatterUser};
    return internal::ChartAxisText(axis, value, internal::TickSet{}, buffer, size);
}

struct DonutLegendData
{
    const DonutParts* parts = nullptr;
    bool disabled = false;
};

bool DonutLegendEntry(const void* data, int index, internal::ChartLabelText* label, internal::ChartLegendLook* look)
{
    const DonutLegendData& legend = *static_cast<const DonutLegendData*>(data);
    const DonutPart part = DonutPartAt(*legend.parts, index);
    *label = internal::ChartVisibleLabel(part.label);
    if (label->Empty())
        return false;
    look->color = DonutPartColor(part, false);
    look->disabled = legend.disabled;
    return true;
}

} // namespace

void DonutChart(const char* id, std::span<const DonutSlice> slices, const DonutChartOptions& o)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return;
    if (id == nullptr)
        id = "";
    const internal::NextItemData next = internal::TakeNextItemData();
    const Theme& t = T();
    const Colors& c = t.colors;
    const ImGuiID chartId = window->GetID(id);
    const DonutParts parts = MakeDonutParts(slices, o);
    const int count = parts.Count();
    const bool disabledFlag = (next.itemFlags & ImGuiItemFlags_Disabled) != 0;
    const DonutLegendData legendData{&parts, disabledFlag};

    // Width: the ring and, with a legend, the legend beside it, or under it when that does not fit.
    const float gap = std::round(Px(t.spacing.lg));
    float diameter = std::round(o.diameter > 0.0f ? Px(o.diameter) : Px(t.metrics.chartHeight));
    const float available = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    const float legendWidth = o.legend ? internal::ChartLegendColumnWidth(&DonutLegendEntry, &legendData, count, available) : 0.0f;
    const float content = diameter + (legendWidth > 0.0f ? gap + legendWidth : 0.0f);
    const float width = internal::ResolveItemWidth(0.0f, next, std::min(content, available));
    const bool beside = legendWidth > 0.0f && diameter + gap + legendWidth <= width + 0.5f;
    diameter = std::max(1.0f, std::min(diameter, beside ? width - gap - legendWidth : width));
    const float legendHeight =
        legendWidth > 0.0f ? internal::MeasureChartLegend(&DonutLegendEntry, &legendData, count, beside ? legendWidth : width, beside)
                           : 0.0f;
    const float height = beside ? std::max(diameter, legendHeight)
                                : diameter + (legendHeight > 0.0f ? std::round(Px(t.spacing.sm)) + legendHeight : 0.0f);
    const ImVec2 origin = window->DC.CursorPos;
    const ImRect bounds(origin, ImVec2(origin.x + width, origin.y + height));

    if (internal::DrawChartDataState(chartId, bounds, o.state, parts.total <= 0.0, o.emptyText, o.errorText))
    {
        ImGui::ItemSize(bounds.GetSize());
        internal::ApplyNextItemData(next);
        ImGui::ItemAdd(bounds, chartId, nullptr, ImGuiItemFlags_NoNav);
        return;
    }

    internal::ApplyNextItemData(next);
    internal::ItemOptions itemOptions;
    itemOptions.handCursor = false;
    itemOptions.hitTarget = false;
    const internal::ItemState item = internal::ItemBehavior(chartId, bounds.GetSize(), itemOptions);
    const bool disabled = item.disabled;
    internal::ChartReadoutState& readout = internal::GetState<internal::ChartReadoutState>(chartId);
    if (!item.visible)
        return;

    // Geometry: the hovered slice grows out by spacing.xs, inside the box.
    const float grow = std::round(Px(t.spacing.xs));
    const ImVec2 center(std::round(bounds.Min.x + diameter * 0.5f), std::round(bounds.Min.y + diameter * 0.5f));
    const float outer = std::max(1.0f, diameter * 0.5f - grow);
    const float hole = std::clamp(t.charts.donutHole, 0.0f, 0.95f);
    const float inner = o.pie ? 0.0f : outer * hole;

    // Picking: the keys, then the pointer over a slice.
    if (internal::ChartReadoutKeys(chartId, item.focused && !disabled, count, false, readout))
        readout.series = -1;
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const float dx = mouse.x - center.x;
    const float dy = mouse.y - center.y;
    const float distance = std::sqrt(dx * dx + dy * dy);
    const ImRect ring(ImVec2(center.x - outer - grow, center.y - outer - grow), ImVec2(center.x + outer + grow, center.y + outer + grow));
    bool show = false;
    if (!disabled && internal::ChartPointerReadout(item.hovered, ring, readout))
    {
        int picked = -1;
        if (distance >= inner && distance <= outer + grow)
        {
            float angle = std::atan2(dy, dx) - kDonutStart;
            while (angle < 0.0f)
                angle += 2.0f * kPi;
            const double at = angle / (2.0f * kPi) * parts.total;
            double sum = 0.0;
            for (int k = 0; k < count && picked < 0; ++k)
            {
                sum += DonutPartAt(parts, k).value;
                if (at < sum)
                    picked = k;
            }
        }
        readout.index = picked;
        show = picked >= 0;
    }
    else if (!disabled && readout.keyboard && item.focusVisible)
    {
        show = true;
    }
    else if (!disabled && item.hovered && readout.keyboard)
    {
        show = true;
    }
    if (!disabled && (Has(item.forced, internal::ForcedItemState::Hovered) || Has(item.forced, internal::ForcedItemState::Focused)))
    {
        if (readout.index < 0 || readout.index >= count)
            readout.index = 0;
        show = true;
    }
    show = show && readout.index >= 0 && readout.index < count;

    // The slices: the surface shows through a gap of charts.barGap between them.
    ImDrawList* drawList = window->DrawList;
    drawList->PushClipRect(bounds.Min, bounds.Max, true);
    const float gapPixels = Px(t.charts.barGap);
    double sum = 0.0;
    ImVec2 pickedPoint = center;
    for (int k = 0; k < count; ++k)
    {
        const DonutPart part = DonutPartAt(parts, k);
        const float a0 = kDonutStart + static_cast<float>(sum / parts.total) * 2.0f * kPi;
        sum += part.value;
        const float a1 = kDonutStart + static_cast<float>(sum / parts.total) * 2.0f * kPi;
        if (part.value <= 0.0)
            continue;
        const float target = show && readout.index == k ? 1.0f : 0.0f;
        const float amount = k < kDonutAnimated ? Tween(chartId, 1 + k, target, t.motion.durationFast) : target;
        const float r1 = outer + grow * amount;
        const float mid = (r1 + inner) * 0.5f;
        // The gap is an angle at the middle of the ring; a slice narrower than two gaps keeps its width.
        const float half = mid > 0.0f ? 0.5f * gapPixels / mid : 0.0f;
        const bool split = count > 1 && a1 - a0 > 4.0f * half;
        const float s0 = split ? a0 + half : a0;
        const float s1 = split ? a1 - half : a1;
        StrokeArc(drawList, center, mid, s0, s1, ToU32(DonutPartColor(part, disabled)), r1 - inner, false);
        if (show && readout.index == k)
        {
            const float am = (a0 + a1) * 0.5f;
            pickedPoint = ImVec2(center.x + std::cos(am) * mid, center.y + std::sin(am) * mid);
        }
    }

    // The center of a ring: the picked slice, else the center text or the total.
    char value[64];
    if (!o.pie && inner > 0.0f)
    {
        const float room = inner * std::numbers::sqrt2_v<float>;
        const ImU32 primary = ToU32(disabled ? c.labelTertiary : c.label);
        const ImU32 secondary = ToU32(disabled ? c.labelTertiary : c.labelSecondary);
        const char* top = nullptr;
        internal::ChartLabelText bottom;
        if (show)
        {
            const DonutPart part = DonutPartAt(parts, readout.index);
            DonutFormat(value, sizeof(value), part.value, o);
            top = value;
            bottom = internal::ChartVisibleLabel(part.label);
        }
        else if (o.centerText != nullptr)
        {
            top = o.centerText;
        }
        else
        {
            DonutFormat(value, sizeof(value), parts.total, o);
            top = value;
        }
        float topHeight = 0.0f, bottomHeight = 0.0f;
        {
            internal::ControlTextScope text(TextStyle::Title3);
            topHeight = ImGui::GetFontSize();
        }
        {
            internal::ControlTextScope text(internal::kChartTextStyle);
            bottomHeight = bottom.Empty() ? 0.0f : ImGui::GetFontSize();
        }
        float y = std::round(center.y - (topHeight + bottomHeight) * 0.5f);
        {
            internal::ControlTextScope text(TextStyle::Title3);
            const internal::FittedText shown = internal::FitText(top, nullptr, room);
            drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(std::round(center.x - shown.width * 0.5f), y),
                              primary, shown.text);
            y += topHeight;
        }
        if (!bottom.Empty())
        {
            internal::ControlTextScope text(internal::kChartTextStyle);
            const internal::FittedText shown = internal::FitText(bottom.text, bottom.end, room);
            drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(std::round(center.x - shown.width * 0.5f), y),
                              secondary, shown.text);
        }
    }
    else if (show)
    {
        // A pie has no hole: the picked slice is named in a readout box beside it.
        const DonutPart part = DonutPartAt(parts, readout.index);
        internal::ChartReadoutLine line;
        line.label = internal::ChartVisibleLabel(part.label);
        line.color = DonutPartColor(part, false);
        DonutFormat(line.value, sizeof(line.value), part.value, o);
        internal::DrawChartReadout(drawList, bounds, pickedPoint, nullptr, std::span<const internal::ChartReadoutLine>(&line, 1), 0);
    }

    if (legendHeight > 0.0f)
    {
        const ImVec2 at = beside ? ImVec2(bounds.Min.x + diameter + gap, std::round(bounds.Min.y + (height - legendHeight) * 0.5f))
                                 : ImVec2(bounds.Min.x, bounds.Min.y + diameter + std::round(Px(t.spacing.sm)));
        internal::DrawChartLegend(drawList, at, beside ? legendWidth : width, &DonutLegendEntry, &legendData, count, beside);
    }
    drawList->PopClipRect();
    internal::DrawItemFocusRing(item, Px(t.radii.element));
}

} // namespace facet
