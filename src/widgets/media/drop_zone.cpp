// DropZone (facet/widgets/media.h): a dashed area that takes Dear ImGui drag and drop payloads of the
// accepted types through the drag and drop core (src/internal/drag_drop.h), and can be activated
// like a button (click, Enter, Space), for example to open a file dialog. Files from the operating
// system reach it only when the host submits them as a payload.

#include "facet/widgets/media.h"

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
#include "internal/drag_drop.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/popup_host.h"
#include "widgets/media/media_look.h"

namespace facet
{
namespace
{

// The default height in control heights (the header promises four).
constexpr float kDropZoneRows = 4.0f;
// Segments of each rounded corner of the dashed outline.
constexpr int kDropZoneCornerSegments = 8;
constexpr int kDropZoneOutlinePoints = (kDropZoneCornerSegments + 1) * 4;

struct MediaDropZoneLast
{
    const ImGuiPayload* payload = nullptr;
    int frame = -1;
};

MediaDropZoneLast& DropZoneLast()
{
    static MediaDropZoneLast last;
    return last;
}

// The outline of a rounded rectangle as a closed polyline, clockwise from the top-left corner.
int DropZoneOutline(const ImRect& r, float radius, ImVec2 (&points)[kDropZoneOutlinePoints])
{
    const float pi = 3.14159265358979323846f;
    const ImVec2 centers[4] = {ImVec2(r.Min.x + radius, r.Min.y + radius), ImVec2(r.Max.x - radius, r.Min.y + radius),
                               ImVec2(r.Max.x - radius, r.Max.y - radius), ImVec2(r.Min.x + radius, r.Max.y - radius)};
    int n = 0;
    for (int corner = 0; corner < 4; ++corner)
    {
        const float start = pi + static_cast<float>(corner) * pi * 0.5f;
        for (int i = 0; i <= kDropZoneCornerSegments; ++i)
        {
            const float a = start + pi * 0.5f * static_cast<float>(i) / kDropZoneCornerSegments;
            points[n++] = ImVec2(centers[corner].x + std::cos(a) * radius, centers[corner].y + std::sin(a) * radius);
        }
    }
    return n;
}

// Dashes of `dash` pixels with `gap` pixels between them along the closed outline.
void DrawDropZoneDashes(ImDrawList* drawList, const ImRect& r, float radius, float dash, float gap, float thickness, ImU32 color)
{
    ImVec2 points[kDropZoneOutlinePoints];
    const int n = DropZoneOutline(r, radius, points);
    const float period = dash + gap;
    float along = 0.0f;  // distance into the current period
    for (int i = 0; i < n; ++i)
    {
        const ImVec2 a = points[i];
        const ImVec2 b = points[(i + 1) % n];
        const float length = std::hypot(b.x - a.x, b.y - a.y);
        float done = 0.0f;
        while (done < length)
        {
            const bool on = along < dash;
            const float left = on ? dash - along : period - along;
            const float take = std::min(left, length - done);
            if (on && take > 0.0f)
            {
                const float t0 = done / length;
                const float t1 = (done + take) / length;
                drawList->AddLine(ImVec2(a.x + (b.x - a.x) * t0, a.y + (b.y - a.y) * t0),
                                  ImVec2(a.x + (b.x - a.x) * t1, a.y + (b.y - a.y) * t1), color, thickness);
            }
            done += take;
            along += take;
            if (along >= period)
                along -= period;
        }
    }
}

} // namespace

DropZoneAction DropZone(const char* label, std::span<const char* const> acceptTypes, const DropZoneOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return DropZoneAction::None;
    if (label == nullptr)
        label = "";
    const internal::NextItemData next = internal::TakeNextItemData();
    const ImGuiID id = window->GetID(label);
    const Theme& t = T();
    const Colors& c = t.colors;

    const float width = std::round(internal::ResolveItemWidth(options.width, next, kFill));
    const float height = std::max(1.0f, std::round(options.height > 0.0f ? Px(options.height)
                                                                          : ControlHeight(ControlSize::Regular) * kDropZoneRows));
    const ImVec2 origin(std::round(window->DC.CursorPos.x), std::round(window->DC.CursorPos.y));
    window->DC.CursorPos = origin;
    const ImVec2 size(width, height);
    const ImRect rect(origin, ImVec2(origin.x + width, origin.y + height));

    internal::ItemState item;
    internal::ApplyNextItemData(next);
    if (options.activatable)
    {
        item = internal::ItemBehavior(id, size);
    }
    else
    {
        ImGui::ItemSize(size);
        item.id = id;
        item.rect = rect;
        item.visible = ImGui::ItemAdd(rect, id, nullptr, ImGuiItemFlags_NoNav);
        item.disabled = (GImGui->LastItemData.ItemFlags & ImGuiItemFlags_Disabled) != 0;
        item.hovered = item.visible && ImGui::IsItemHovered() && !item.disabled;
    }

    // The drop: a disabled zone refuses every payload, so the not-allowed cursor shows over it.
    internal::DragDropZoneOptions zoneOptions;
    zoneOptions.accept = !item.disabled;
    const internal::DragDropZoneResult drop = internal::DragDropZone(id, rect, acceptTypes, zoneOptions);
    DropZoneAction action = DropZoneAction::None;
    if (drop.delivered && drop.payload != nullptr && !item.disabled)
    {
        action = DropZoneAction::Dropped;
        DropZoneLast() = {drop.payload, ImGui::GetFrameCount()};
    }
    else if (options.activatable && item.pressed && !item.disabled)
    {
        action = DropZoneAction::Activated;
    }

    // Draw: the dashed outline, the wash of hover and of an accepted drag, the icon and the text.
    const char* text = options.hint != nullptr ? options.hint : label;
    const char* textEnd = options.hint != nullptr ? nullptr : ImGui::FindRenderedTextEnd(label);
    bool truncated = false;
    if (item.visible)
    {
        ImDrawList* drawList = window->DrawList;
        const float radius = std::min(Px(t.radii.card), 0.5f * std::min(width, height));
        const bool target = drop.hovered && !drop.rejected;
        if (target)
        {
            internal::DragDropDrawTarget(drawList, rect, radius);
        }
        else
        {
            const ImVec4 clear = WithAlpha(c.fill[2], 0.0f);
            const ImVec4 wash = options.activatable && !item.disabled ? internal::StateFill(clear, c.fill[2], c.fill[1], item) : clear;
            if (wash.w > 0.0f)
                FillRect(drawList, rect.Min, rect.Max, ToU32(wash), radius);
            const float stroke = HairlineThickness();
            ImRect line = rect;
            line.Expand(-stroke * 0.5f);
            DrawDropZoneDashes(drawList, line, std::max(0.0f, radius - stroke * 0.5f), std::max(1.0f, Px(t.media.dashLength)),
                               std::max(1.0f, Px(t.media.dashGap)), stroke,
                               ToU32(item.disabled ? c.separator : ControlBoundaryColor(c)));
        }

        const internal::ControlTextScope scope(TextStyle::Body);
        const float pad = PxSnapped(t.spacing.md);
        const internal::FittedText shown = internal::FitText(text, textEnd, std::max(0.0f, width - pad * 2.0f));
        truncated = shown.truncated;
        const float lineHeight = std::ceil(ImGui::GetTextLineHeight());
        const float gap = PxSnapped(t.spacing.sm);
        float iconSize = std::floor(Px(t.metrics.emptyIconSize));
        if (iconSize + gap + lineHeight + pad * 2.0f > height)
            iconSize = std::floor(Px(t.spacing.xl));
        const bool showIcon = options.icon.IsValid() && iconSize + gap + lineHeight + pad * 2.0f <= height;
        const float block = (showIcon ? iconSize + gap : 0.0f) + lineHeight;
        float y = std::round(rect.GetCenter().y - block * 0.5f);
        const ImU32 ink = ToU32(item.disabled ? c.labelTertiary : (target ? c.label : c.labelSecondary));
        drawList->PushClipRect(rect.Min, rect.Max, true);
        if (showIcon)
        {
            DrawIcon(drawList, options.icon, ImVec2(std::round(rect.GetCenter().x - iconSize * 0.5f), y), iconSize, ink);
            y += iconSize + gap;
        }
        drawList->AddText(ImVec2(std::round(rect.GetCenter().x - shown.width * 0.5f), y), ink, shown.text);
        drawList->PopClipRect();
        if (options.activatable)
            internal::DrawItemFocusRing(item, radius);
    }

    // With a hint, the visible label part is the tooltip; a cut text shows in full.
    const internal::MediaText alt = internal::MediaVisibleText(label);
    if (options.hint != nullptr)
    {
        if (!alt.IsEmpty() && internal::BeginPopupTooltip(id, rect, item.hovered, item.focusVisible))
        {
            Text(TextStyle::Footnote, "%s", alt.text);
            internal::EndPopupTooltip();
        }
    }
    else
    {
        internal::CutTextTooltip(item, text, textEnd, truncated);
    }
    return action;
}

const ImGuiPayload* DropZonePayload()
{
    const MediaDropZoneLast& last = DropZoneLast();
    if (last.payload == nullptr || ImGui::GetCurrentContext() == nullptr || last.frame != ImGui::GetFrameCount())
        return nullptr;
    return last.payload;
}

} // namespace facet
