// Parts shared by the inventory, card and people composites; see cards_look.h.

#include "widgets/composites/cards_look.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/drag_drop.h"
#include "internal/popup_host.h"
#include "widgets/media/media_look.h"

namespace facet::internal
{
namespace
{

// The cooldown sweep is drawn as two convex halves, each an arc of at most half a turn.
constexpr int kInventorySweepSegments = 24;

// What the source of the drag in progress wrote about its item.
struct InventoryDragRecord
{
    InventoryDragged dragged;
    int frame = -1;
    char label[kMediaTextSize] = {};
    InventoryItem item;  // label points into `label`
};

InventoryDragRecord& DragRecord()
{
    static InventoryDragRecord record;
    return record;
}

// A small chip on the tile (the count, the lock): bgElevated with the separator hairline, so it
// reads over any picture in both themes. A capsule (a circle when it is square), the shape of every
// badge.
void DrawTileChip(ImDrawList* drawList, ImVec2 min, ImVec2 size)
{
    const Colors& c = T().colors;
    const ImVec2 max(min.x + size.x, min.y + size.y);
    const float radius = 0.5f * std::min(size.x, size.y);
    FillRect(drawList, min, max, ToU32(c.bgElevated), radius);
    DrawSurfaceOutline(drawList, ImRect(min, max), radius);
}

// The remaining part of a cooldown as a scrim pie over the middle of ox, clockwise from the top,
// as large as the shorter side, so it reads as a clock running down over the picture.
void DrawCooldown(ImDrawList* drawList, const ImRect& box, float remaining)
{
    const float share = std::clamp(remaining, 0.0f, 1.0f);
    if (share <= 0.0f)
        return;
    const ImVec2 center = box.GetCenter();
    const float radius = 0.5f * std::min(box.GetWidth(), box.GetHeight());
    const ImU32 color = ToU32(T().colors.scrim);
    const float start = -IM_PI * 0.5f;
    const float end = start + share * IM_PI * 2.0f;
    float from = start;
    while (from < end)
    {
        const float to = std::min(end, from + IM_PI);
        drawList->PathLineTo(center);
        drawList->PathArcTo(center, radius, from, to, kInventorySweepSegments);
        drawList->PathFillConvex(color);
        from = to;
    }
}

} // namespace

float InventoryTileRadius()
{
    return Px(T().radii.control);
}

InventoryTileDrawn InventoryDrawTile(ImDrawList* drawList, const ImRect& rect, const InventoryItem& item,
                                     const InventoryTileLook& look)
{
    InventoryTileDrawn drawn;
    if (drawList == nullptr || rect.GetWidth() < 1.0f || rect.GetHeight() < 1.0f)
        return drawn;
    const Theme& t = T();
    const Colors& c = t.colors;
    const float radius = std::min(InventoryTileRadius(), 0.5f * std::min(rect.GetWidth(), rect.GetHeight()));
    const bool empty = item.label == nullptr;
    // A lifted item leaves its place faded: the ghost carries it, the place says where it came from.
    const float opacity = look.lifted ? std::clamp(1.0f - t.drag.ghostOpacity, 0.0f, 1.0f) : 1.0f;
    const ImVec2 min(std::round(rect.Min.x), std::round(rect.Min.y));
    const ImVec2 max(std::round(rect.Max.x), std::round(rect.Max.y));

    // The slot: its fill and one quiet hairline, so an empty slot reads by its fill like any control;
    // a selected slot takes accentSubtle (unless the grid's row fill already holds it) and the accent
    // outline at the same width.
    const ImRect box(min, max);
    if (!look.selected)
        FillRect(drawList, min, max, ToU32(look.disabled ? c.fill[3] : c.fill[2]), radius);
    else if (!look.selectionFilled)
        FillRect(drawList, min, max, ToU32(c.accentSubtle), radius);
    DrawOutline(drawList, box, radius,
                ControlOutlineColor({.hoverAmount = look.hoverAmount, .selected = look.selected, .disabled = look.disabled}));

    const float pad = PxSnapped(t.spacing.xs);
    const ImRect inner(min.x + pad, min.y + pad, max.x - pad, max.y - pad);
    if (inner.GetWidth() < 1.0f || inner.GetHeight() < 1.0f)
        return drawn;

    if (empty)
    {
        if (look.placeholderIcon.IsValid())
        {
            const float side = std::floor(std::min({inner.GetWidth(), inner.GetHeight(), Px(t.metrics.emptyIconSize)}));
            DrawIcon(drawList, look.placeholderIcon,
                     ImVec2(std::round(inner.GetCenter().x - side * 0.5f), std::round(inner.GetCenter().y - side * 0.5f)),
                     side, ToU32(c.labelTertiary));
        }
    }
    else
    {
        DrawGroup group(drawList, opacity);
        ControlTextScope caption(TextStyle::Caption);
        const float lineHeight = std::ceil(ImGui::GetTextLineHeight());
        const float tint = item.tint != 0 ? std::max(1.0f, PxSnapped(t.composites.inventoryTintHeight)) : 0.0f;
        // The title goes under the picture when the picture keeps at least a line's height.
        const bool title = inner.GetHeight() - tint >= lineHeight * 2.0f + pad;
        ImRect picture(inner.Min, ImVec2(inner.Max.x, inner.Max.y - tint - (title ? lineHeight + pad : 0.0f)));
        if (picture.GetHeight() < 1.0f)
            picture.Max.y = picture.Min.y + 1.0f;

        if (item.image.IsValid())
        {
            MediaDrawOptions options;
            options.fit = ImageFit::Contain;
            options.radius = Px(t.radii.element);
            options.opacity = look.disabled ? kDisabledPictureOpacity : 1.0f;
            MediaDrawImageSlot(drawList, MediaImageOf(item.image), picture, options, "");
        }
        else if (item.icon.IsValid())
        {
            const float side = std::floor(std::min({picture.GetWidth(), picture.GetHeight(), Px(t.metrics.emptyIconSize)}));
            DrawIcon(drawList, item.icon,
                     ImVec2(std::round(picture.GetCenter().x - side * 0.5f), std::round(picture.GetCenter().y - side * 0.5f)),
                     side, ToU32(ControlLabelColor(look.disabled)));
        }
        if (item.cooldown > 0.0f)
            DrawCooldown(drawList, picture, item.cooldown);

        const float chipPad = PxSnapped(t.spacing.xs);
        const float chipHeight = lineHeight;
        if (item.locked)
        {
            const float side = std::floor(caption.IconSize());
            // A circle around the lock glyph.
            const float diameter = std::max(chipHeight, side + chipPad);
            const ImVec2 size(diameter, diameter);
            DrawTileChip(drawList, inner.Min, size);
            DrawIcon(drawList, Icon("lock"),
                     ImVec2(std::round(inner.Min.x + (size.x - side) * 0.5f), std::round(inner.Min.y + (size.y - side) * 0.5f)),
                     side, ToU32(ControlSecondaryColor(look.disabled)));
        }
        if (item.count > 1)
        {
            char text[16];
            std::snprintf(text, sizeof(text), "%d", item.count);
            const FittedText shown = FitText(text, nullptr, std::max(0.0f, inner.GetWidth() - chipPad * 2.0f));
            // A capsule, never narrower than a circle, so one digit sits in a round badge.
            const ImVec2 size(std::max(chipHeight, std::ceil(shown.width) + chipPad * 2.0f), chipHeight);
            const ImVec2 at(inner.Max.x - size.x, inner.Min.y);
            DrawTileChip(drawList, at, size);
            const float textX = std::round(at.x + (size.x - shown.width) * 0.5f);
            drawList->AddText(ImVec2(textX, at.y), ToU32(ControlLabelColor(look.disabled)), shown.text);
        }
        if (title)
        {
            const char* end = ImGui::FindRenderedTextEnd(item.label);
            const FittedText shown = FitText(item.label, end, inner.GetWidth());
            drawn.titleCut = shown.truncated;
            const float y = inner.Max.y - tint - lineHeight;
            drawList->AddText(ImVec2(std::round(inner.GetCenter().x - shown.width * 0.5f), y),
                              ToU32(ControlLabelColor(look.disabled)), shown.text);
        }
        if (tint > 0.0f)
        {
            // Category color as data; the bar sits on the bottom edge inside the boundary.
            const float hair = HairlineThickness();
            FillRect(drawList, ImVec2(min.x + hair, max.y - hair - tint), ImVec2(max.x - hair, max.y - hair),
                     look.disabled ? ToU32(c.labelTertiary) : item.tint, std::max(0.0f, radius - hair),
                     ImDrawFlags_RoundBottom);
        }
    }

    return drawn;
}

void InventoryTileTooltip(ImGuiID id, const ImRect& rect, bool hovered, bool focusVisible, const InventoryItem& item,
                          const char* emptyName)
{
    if (!hovered && !focusVisible)
        return;
    const Strings& strings = GetStrings();
    if (!BeginPopupTooltip(id, rect, hovered, focusVisible))
        return;
    const char* name = item.label;
    if (name == nullptr)
        name = emptyName != nullptr && emptyName[0] != '\0' ? emptyName : strings.emptySlot;
    const MediaText shown = MediaVisibleText(name);
    StyledText(shown.text, {.style = TextStyle::Footnote});
    if (item.label != nullptr && item.locked)
        StyledText(strings.locked, {.style = TextStyle::Footnote, .secondary = true});
    EndPopupTooltip();
}

// ---- Dragging ---------------------------------------------------------------------------------

namespace
{

// The ghost: the tile as it was picked up, from the record its source wrote.
void DrawInventoryGhost(ImDrawList* drawList, const ImRect& rect, const ImGuiPayload& payload)
{
    const InventoryDragRecord& record = DragRecord();
    const InventoryPayload* data = InventoryPayloadOf(&payload);
    if (data == nullptr || data->source != record.dragged.source || data->slot != record.dragged.slot)
        return;
    InventoryDrawTile(drawList, rect, record.item, {});
}

} // namespace

const InventoryPayload* InventoryPayloadOf(const ImGuiPayload* payload)
{
    if (payload == nullptr || payload->Data == nullptr || payload->DataSize != static_cast<int>(sizeof(InventoryPayload)))
        return nullptr;
    return static_cast<const InventoryPayload*>(payload->Data);
}

bool InventoryDragSource(const InventorySourceDesc& desc)
{
    if (desc.item == nullptr || desc.item->label == nullptr || desc.item->locked || desc.type == nullptr)
        return false;
    const InventoryItem& item = *desc.item;
    if (desc.forced)
    {
        // Captures: the pointer over the middle of the next tile down and along, so its target shows.
        const ImRect& rect = GImGui->LastItemData.Rect;
        DragDropForceNextSource(ImVec2(rect.GetWidth(), rect.GetHeight()));
    }
    const InventoryPayload payload{desc.owner, desc.slot, std::max(1, item.count)};
    DragDropPreview preview;
    preview.previewText = item.label;
    preview.icon = item.icon;
    preview.image = item.image;
    preview.draw = &DrawInventoryGhost;
    preview.keyboard = true;
    preview.pickUpChord = desc.pickUp;
    if (!DragDropBeginSource(desc.type, &payload, sizeof(payload), preview))
        return false;
    InventoryDragRecord& record = DragRecord();
    record.dragged = {desc.owner, desc.slot, payload.count, item.stackKey, item.maxStack};
    record.frame = ImGui::GetFrameCount();
    const char* end = ImGui::FindRenderedTextEnd(item.label);
    const std::size_t length = std::min(static_cast<std::size_t>(end - item.label), sizeof(record.label) - 1);
    std::memcpy(record.label, item.label, length);
    record.label[length] = '\0';
    record.item = item;
    record.item.label = record.label;
    DragDropEndSource();
    return true;
}

const InventoryDragged* InventoryDraggedOf(const ImGuiPayload* payload)
{
    const InventoryPayload* data = InventoryPayloadOf(payload);
    const InventoryDragRecord& record = DragRecord();
    // The record stays valid while its source scrolls away mid-drag; the next drag overwrites it.
    if (data == nullptr || record.frame < 0 || data->source != record.dragged.source || data->slot != record.dragged.slot)
        return nullptr;
    return &record.dragged;
}

InventoryResult InventoryDropRule(const InventoryPayload& dragged, const InventoryDragged* stack, bool split,
                                  const InventoryItem& target, int to)
{
    InventoryResult result;
    result.source = dragged.source;
    result.from = dragged.slot;
    result.to = to;
    const int moving = std::max(1, dragged.count);
    const int part = split && moving >= 2 ? moving / 2 : moving;
    if (target.label == nullptr)
    {
        result.action = part < moving ? InventoryAction::Split : InventoryAction::Moved;
        result.count = part;
        return result;
    }
    const bool sameKind = stack != nullptr && stack->stackKey != 0 && stack->stackKey == target.stackKey;
    if (sameKind)
    {
        const int room = target.maxStack > 0 ? target.maxStack - std::max(0, target.count) : part;
        if (room > 0)
        {
            result.action = InventoryAction::Merged;
            result.count = std::min(part, room);
            return result;
        }
    }
    result.action = InventoryAction::Swapped;
    result.count = moving;
    return result;
}

InventoryDropOutcome InventoryDropTarget(const InventoryTargetDesc& desc)
{
    InventoryDropOutcome outcome;
    if (desc.type == nullptr || desc.item == nullptr)
        return outcome;
    DragDropInfo info;
    const bool dragging = DragDropActive(desc.type, &info);
    const InventoryPayload* incoming = dragging ? InventoryPayloadOf(info.payload) : nullptr;
    const bool ownPlace = incoming != nullptr && incoming->source == desc.owner && incoming->slot == desc.slot;
    DragDropZoneOptions options;
    options.accept = desc.accept && (!desc.item->locked || ownPlace) && (!dragging || incoming != nullptr);
    options.allowSplit = true;
    const DragDropZoneResult zone = DragDropZone(desc.zoneId, desc.rect, desc.type, options);
    outcome.hovered = zone.hovered;
    outcome.keyboard = zone.keyboard;
    const InventoryPayload* payload = InventoryPayloadOf(zone.payload);
    const bool same = payload != nullptr && payload->source == desc.owner && payload->slot == desc.slot;
    if (zone.hovered && !same && desc.draw)
    {
        if (ImDrawList* drawList = ImGui::GetWindowDrawList())
            DragDropDrawTarget(drawList, desc.rect, InventoryTileRadius());
    }
    if (zone.delivered && payload != nullptr)
    {
        outcome.delivered = true;
        if (!same)
            outcome.result = InventoryDropRule(*payload, InventoryDraggedOf(zone.payload),
                                               zone.effect == DragDropEffect::Split, *desc.item, desc.slot);
    }
    return outcome;
}

} // namespace facet::internal
