// Demo page: Drag and drop. Prototypes built only from the item behaviour, the focus ring and the
// drag and drop core: a reorderable list whose rows move apart to show the gap, two lists that
// exchange items with an insertion line (Ctrl copies), a grid of slots that swap, stack and split
// (Shift), a small tree with before, after and into drops, a zone that takes list items only, and
// a long list in a scroll area. Every prototype also works from the keyboard: Space picks the
// focused item up, the arrows move it, Space or Enter drops it, Escape puts it back. The page
// applies every reported drop to its own data, as a widget's caller would.

#include <algorithm>
#include <cstdio>
#include <span>

#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "gallery.h"
#include "internal/drag_drop.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/row_layout.h"
#include "internal/text_util.h"
#include "pages/pages.h"

namespace
{

namespace fi = facet::internal;
using facet::demo::ForcedItemState;
using facet::demo::Note;
using facet::demo::SectionHeader;

enum CaptureState
{
    kDefault,
    kDragging,
    kTransfer,
    kGrid,
    kTree,
    kRejected,
};

constexpr const char* kRowType = "demo.row";
constexpr const char* kSlotType = "demo.slot";
constexpr const char* kNodeType = "demo.node";

constexpr int kMaxItems = 32;
constexpr int kLongCount = 30;
constexpr int kLongVisibleRows = 5;
constexpr int kGridColumns = 3;
constexpr int kGridSlots = 6;
constexpr int kTreeNodes = 6;

// What a list row carries: its list, its index there and its stable key.
struct RowPayload
{
    int list = -1;
    int index = -1;
    int key = 0;
};

struct ListModel
{
    int tag = 0;
    int keys[kMaxItems] = {};
    int count = 0;
};

ListModel MakeList(int tag, int first, int count)
{
    ListModel list;
    list.tag = tag;
    list.count = count;
    for (int i = 0; i < count; ++i)
        list.keys[i] = first + i;
    return list;
}

// A grid slot: the item number (0 for an empty slot) and how many of it.
struct Slot
{
    int item = 0;
    int count = 0;
};

// A tree node; `order` lists the nodes depth first, so a node's subtree follows it.
struct TreeNode
{
    const char* label = "";
    int parent = -1;
    bool group = false;
    bool open = true;
};

struct PageState
{
    ListModel list = MakeList(0, 1, 5);
    ListModel left = MakeList(1, 1, 4);
    ListModel right = MakeList(2, 5, 2);
    ListModel longList = MakeList(3, 1, kLongCount);
    int nextKey = 7;  // the number of the next copy in the two lists
    Slot grid[kGridSlots] = {{1, 3}, {}, {2, 1}, {2, 2}, {3, 1}, {}};
    TreeNode nodes[kTreeNodes] = {{"Group 1", -1, true, true}, {"Item 1", 0, false, true}, {"Item 2", 0, false, true},
                                  {"Group 2", -1, true, false}, {"Item 3", 3, false, true}, {"Item 4", -1, false, true}};
    int order[kTreeNodes] = {0, 1, 2, 3, 4, 5};
    int openedByDrag = -1;   // a group the drag opened, closed again when the drag is canceled
    int onlyDrops = 0;       // drops the list-only zone took
    int onlyLastKey = 0;
    int longDroppedAt = -1;  // where the last drop in the long list put the row
    ImGuiID longWindow = 0;

    // Rectangles of the last frame, for the forced drags of captures.
    ImRect rightZone;
    ImRect leftRow1;
    ImRect gridCells[kGridSlots];
    ImRect treeRows[kTreeNodes];
    ImRect onlyZone;
};

PageState page;

void ItemLabel(char* out, std::size_t size, int key)
{
    std::snprintf(out, size, "Item %d", key);
}

// ---- Lists ----------------------------------------------------------------------------------------

struct ListLook
{
    bool line = false;          // an insertion line instead of rows that move apart
    bool allowCopy = false;     // Ctrl copies
    const char* probe = "";
    int forcedRow = -1;         // captures: the row shown as dragged
    ImVec2 forcedOffset = ImVec2(0.0f, 0.0f);  // pixels from that row's center
};

struct ListDrop
{
    bool delivered = false;
    RowPayload payload;
    int insert = -1;
    fi::DragDropEffect effect = fi::DragDropEffect::Move;
};

// A list of rows, each a drag source, over one ordered zone. Rows are laid out in data order and
// drawn where they move to: their positions spring toward layout plus gap shift, keyed by the
// stable item key, so they stay still when the page applies a move.
ListDrop ProtoList(facet::demo::PageCtx& ctx, const char* id, const ListModel& model, float width, const ListLook& look,
                   ImRect* zoneOut = nullptr, ImRect* rowOut = nullptr, int rowOutIndex = -1)
{
    const facet::Theme& t = facet::T();
    const facet::Colors& c = t.colors;
    ImGui::PushID(id);
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImDrawList* drawList = window->DrawList;
    const ImGuiID zoneId = ImGui::GetID("zone");
    const float rowHeight = facet::Px(t.list.rowHeight);
    const float pad = facet::Px(t.spacing.xs);
    const float spacing = ImGui::GetStyle().ItemSpacing.y;
    const float radius = facet::Px(t.radii.control);
    const ImVec2 origin(window->Pos.x - window->Scroll.x, window->Pos.y - window->Scroll.y);

    fi::DragDropInfo drag;
    const bool dragging = fi::DragDropActive(kRowType, &drag);

    // The surface, sized from the row count (an empty list keeps one row of room to drop into).
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const int rows = std::max(1, model.count);
    const float height = rows * rowHeight + (rows - 1) * spacing + pad * 2.0f;
    const ImRect zoneRect(start, ImVec2(start.x + width, start.y + height));
    facet::FillRect(drawList, zoneRect.Min, zoneRect.Max, facet::ToU32(c.bgSecondary), facet::Px(t.radii.card));

    // Pass 1: the rows as items and sources, laid out in data order.
    ImRect slots[kMaxItems];
    fi::ItemState states[kMaxItems];
    int lifted = -1;
    char label[32];
    char probe[48];
    ImGui::SetCursorScreenPos(ImVec2(start.x + pad, start.y + pad));
    ImGui::BeginGroup();
    for (int i = 0; i < model.count; ++i)
    {
        ImGui::PushID(model.keys[i]);
        if (ctx.capture && i == look.forcedRow)
            ctx.ForceNextItemState(ForcedItemState::Dragging);
        states[i] = fi::ItemBehavior(ImGui::GetID("row"), ImVec2(width - pad * 2.0f, rowHeight));
        slots[i] = states[i].rect;
        std::snprintf(probe, sizeof(probe), "%s.%d", look.probe, i);
        ctx.Probe(probe);
        if (rowOut != nullptr && i == rowOutIndex)
            *rowOut = slots[i];
        ItemLabel(label, sizeof(label), model.keys[i]);
        if (Has(states[i].forced, ForcedItemState::Dragging))
            fi::DragDropForceNextSource(look.forcedOffset);
        const RowPayload payload{model.tag, i, model.keys[i]};
        if (fi::DragDropBeginSource(kRowType, &payload, sizeof(payload),
                                    {.previewText = label, .icon = facet::Icon("grip")}))
        {
            lifted = i;
            fi::DragDropEndSource();
        }
        ImGui::PopID();
    }
    if (model.count == 0)
        ImGui::Dummy(ImVec2(width - pad * 2.0f, rowHeight));
    ImGui::EndGroup();
    ImGui::SetCursorScreenPos(ImVec2(start.x, zoneRect.Max.y));
    ImGui::Dummy(ImVec2(width, 0.0f));
    if (zoneOut != nullptr)
        *zoneOut = zoneRect;

    fi::DragDropZoneOptions options;
    options.slots = std::span<const ImRect>(slots, static_cast<std::size_t>(model.count));
    options.allowCopy = look.allowCopy;
    const fi::DragDropZoneResult drop = fi::DragDropZone(zoneId, zoneRect, kRowType, options);
    std::snprintf(probe, sizeof(probe), "%s.zone", look.probe);
    facet::demo::ProbeRect(ctx, probe, zoneId, zoneRect.Min, zoneRect.Max);

    // Which row of this list is dragged, from the payload (the caller's own data).
    int source = -1;
    const ImGuiPayload* payload = drop.payload != nullptr ? drop.payload : (dragging ? drag.payload : nullptr);
    if (payload != nullptr && payload->DataSize == static_cast<int>(sizeof(RowPayload)))
    {
        const RowPayload* row = static_cast<const RowPayload*>(payload->Data);
        if (row->list == model.tag)
            source = row->index;
    }
    const int insert = drop.hovered ? drop.insertIndex : -1;
    const float dragHeight = drag.sourceSize.y > 0.0f ? drag.sourceSize.y : rowHeight;
    const float extent = dragHeight + spacing;

    // Pass 2: draw. The landing place first, then the rows where they move to.
    if (!look.line && insert >= 0 && model.count > 0)
    {
        const ImRect gap = fi::DragDropGapRect(options.slots, source, insert, facet::Orientation::Vertical, dragHeight);
        facet::FillRect(drawList, gap.Min, gap.Max, facet::ToU32(c.fill[2]), radius);
    }
    facet::PushTextStyle(facet::TextStyle::Body);
    const float iconSize = ImGui::GetFontSize();
    for (int i = 0; i < model.count; ++i)
    {
        const fi::ItemState& row = states[i];
        float target = slots[i].Min.y - origin.y;
        if (i == lifted)
            target = drag.pointer.y - drag.grabOffset.y - origin.y;  // after the drop it settles from the ghost
        else if (!look.line)
            target += fi::DragDropGapShift(i, source, insert, extent);
        const float y = origin.y + fi::DragDropAnimatedPosition(zoneId, static_cast<ImGuiID>(model.keys[i]), target);
        if (i == lifted || !row.visible)
            continue;
        const ImRect rect(slots[i].Min.x, y, slots[i].Max.x, y + slots[i].GetHeight());
        const float feedback = std::max(row.hoverAmount, row.pressAmount);
        if (feedback > 0.0f)
        {
            const ImVec4 fill = facet::Mix(c.fill[2], c.fill[1], row.pressAmount);
            facet::FillRect(drawList, rect.Min, rect.Max, facet::ToU32(facet::WithAlpha(fill, fill.w * feedback)), radius);
        }
        const float inset = facet::Px(t.spacing.sm);
        facet::DrawIcon(drawList, facet::Icon("grip"), ImVec2(rect.Min.x + inset, rect.Min.y + (rect.GetHeight() - iconSize) * 0.5f),
                        iconSize, facet::ToU32(c.labelSecondary));
        ItemLabel(label, sizeof(label), model.keys[i]);
        drawList->AddText(ImVec2(rect.Min.x + inset * 2.0f + iconSize, facet::CenteredTextY(rect.Min.y, rect.GetHeight())),
                          facet::ToU32(c.label), label);
        fi::DrawFocusRing(drawList, rect, radius, row.focusAmount);
    }
    if (model.count == 0)
    {
        const char* empty = "No items. Drop one here.";
        const ImVec2 size = ImGui::CalcTextSize(empty);
        drawList->AddText(ImVec2(zoneRect.GetCenter().x - size.x * 0.5f, zoneRect.GetCenter().y - size.y * 0.5f),
                          facet::ToU32(c.labelSecondary), empty);
    }
    facet::PopTextStyle();
    if (look.line && insert >= 0)
        fi::DragDropDrawInsertLine(drawList, fi::DragDropInsertLineRect(options.slots, insert, facet::Orientation::Vertical));
    if (drop.hovered && model.count == 0)
        fi::DragDropDrawTarget(drawList, zoneRect, facet::Px(t.radii.card));
    ImGui::PopID();

    ListDrop result;
    if (drop.delivered && drop.payload != nullptr && drop.payload->DataSize == static_cast<int>(sizeof(RowPayload)))
    {
        result.delivered = true;
        result.payload = *static_cast<const RowPayload*>(drop.payload->Data);
        result.insert = drop.insertIndex;
        result.effect = drop.effect;
    }
    return result;
}

ListModel* ListByTag(int tag)
{
    switch (tag)
    {
    case 0:
        return &page.list;
    case 1:
        return &page.left;
    case 2:
        return &page.right;
    case 3:
        return &page.longList;
    default:
        return nullptr;
    }
}

// Applies a drop on `target` as the caller of a reorderable or transfer list would. Returns the
// index the row ends up at, or -1.
int ApplyListDrop(ListModel& target, const ListDrop& drop)
{
    ListModel* from = ListByTag(drop.payload.list);
    if (!drop.delivered || from == nullptr || drop.payload.index < 0 || drop.payload.index >= from->count)
        return -1;
    if (from == &target && drop.effect != fi::DragDropEffect::Copy)
    {
        const facet::ReorderMove move = fi::DragDropReorderMove(drop.payload.index, drop.insert);
        facet::ApplyReorderMove(std::span<int>(target.keys, static_cast<std::size_t>(target.count)), move);
        return move.IsValid() ? move.to : drop.payload.index;
    }
    if (target.count >= kMaxItems)
        return -1;
    const int key = drop.effect == fi::DragDropEffect::Copy ? page.nextKey++ : from->keys[drop.payload.index];
    if (drop.effect != fi::DragDropEffect::Copy)
    {
        for (int i = drop.payload.index; i + 1 < from->count; ++i)
            from->keys[i] = from->keys[i + 1];
        --from->count;
    }
    const int at = std::clamp(drop.insert, 0, target.count);
    for (int i = target.count; i > at; --i)
        target.keys[i] = target.keys[i - 1];
    target.keys[at] = key;
    ++target.count;
    return at;
}

float ListWidth()
{
    return std::min(ImGui::GetContentRegionAvail().x, facet::Px(facet::T().metrics.cardWidth));
}

// ---- Grid -------------------------------------------------------------------------------------

struct SlotPayload
{
    int cell = -1;
};

void ApplyGridDrop(int from, int to, fi::DragDropEffect effect)
{
    if (from == to || from < 0 || to < 0 || page.grid[from].item == 0)
        return;
    Slot& a = page.grid[from];
    Slot& b = page.grid[to];
    const bool split = effect == fi::DragDropEffect::Split && a.count > 1;
    const int moved = split ? a.count / 2 : a.count;
    if (b.item == 0 || b.item == a.item)
    {
        b.item = a.item;
        b.count += moved;
        a.count -= moved;
        if (a.count == 0)
            a.item = 0;
    }
    else if (!split)
    {
        std::swap(a, b);
    }
}

void ProtoGrid(facet::demo::PageCtx& ctx, int forcedCell, ImVec2 forcedOffset)
{
    const facet::Theme& t = facet::T();
    const facet::Colors& c = t.colors;
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImDrawList* drawList = window->DrawList;
    const float gap = facet::Px(t.spacing.sm);
    const float available = ImGui::GetContentRegionAvail().x;
    const float size = std::min(facet::Px(t.metrics.slotSize), (available - gap * (kGridColumns - 1)) / kGridColumns);
    const float radius = facet::Px(t.radii.control);
    ImGui::PushID("grid");

    fi::ItemState cells[kGridSlots];
    fi::DragDropZoneResult drops[kGridSlots];
    int lifted = -1;
    char label[32];
    char probe[32];
    const ImVec2 start = ImGui::GetCursorScreenPos();
    for (int i = 0; i < kGridSlots; ++i)
    {
        const int column = i % kGridColumns;
        const int row = i / kGridColumns;
        ImGui::SetCursorScreenPos(ImVec2(start.x + column * (size + gap), start.y + row * (size + gap)));
        ImGui::PushID(i);
        if (ctx.capture && i == forcedCell)
            ctx.ForceNextItemState(ForcedItemState::Dragging);
        cells[i] = fi::ItemBehavior(ImGui::GetID("cell"), ImVec2(size, size));
        page.gridCells[i] = cells[i].rect;
        std::snprintf(probe, sizeof(probe), "dd.grid.%d", i);
        ctx.Probe(probe);
        const Slot& slot = page.grid[i];
        if (slot.item != 0)
        {
            ItemLabel(label, sizeof(label), slot.item);
            if (Has(cells[i].forced, ForcedItemState::Dragging))
                fi::DragDropForceNextSource(forcedOffset);
            const SlotPayload payload{i};
            if (fi::DragDropBeginSource(kSlotType, &payload, sizeof(payload), {.previewText = label, .count = slot.count}))
            {
                lifted = i;
                fi::DragDropEndSource();
            }
        }
        fi::DragDropZoneOptions options;
        options.allowSplit = true;
        drops[i] = fi::DragDropZone(ImGui::GetID("zone"), cells[i].rect, kSlotType, options);
        ImGui::PopID();
    }
    ImGui::SetCursorScreenPos(ImVec2(start.x, start.y + (kGridSlots / kGridColumns) * (size + gap) - gap));
    ImGui::Dummy(ImVec2(kGridColumns * (size + gap) - gap, 0.0f));

    // Which cell the dragged item comes from, and where it would go.
    int source = lifted;
    int target = -1;
    fi::DragDropEffect effect = fi::DragDropEffect::Move;
    for (int i = 0; i < kGridSlots; ++i)
    {
        if (!drops[i].hovered && !drops[i].delivered)
            continue;
        const SlotPayload* payload = static_cast<const SlotPayload*>(drops[i].payload->Data);
        source = payload->cell;
        target = i;
        effect = drops[i].effect;
        if (drops[i].delivered)
        {
            ApplyGridDrop(source, target, effect);
            source = -1;
            target = -1;
        }
    }

    // A swap: the target's item leaves its slot and shows faintly in the slot it would move to.
    const bool swapping = source >= 0 && target >= 0 && target != source && page.grid[target].item != 0 &&
                          page.grid[target].item != page.grid[source].item && effect != fi::DragDropEffect::Split;
    facet::PushTextStyle(facet::TextStyle::Footnote);
    for (int i = 0; i < kGridSlots; ++i)
    {
        const ImRect& rect = cells[i].rect;
        facet::FillRect(drawList, rect.Min, rect.Max, facet::ToU32(c.bgSecondary), radius);
        const Slot& slot = page.grid[i];
        if (i == target)
            fi::DragDropDrawTarget(drawList, rect, radius);
        else if (cells[i].hoverAmount > 0.0f && slot.item != 0)
            facet::FillRect(drawList, rect.Min, rect.Max,
                            facet::ToU32(facet::WithAlpha(c.fill[2], c.fill[2].w * cells[i].hoverAmount)), radius);
        const bool showItem = slot.item != 0 && i != source && !(swapping && i == target);
        const bool swapPreview = swapping && i == source;
        const Slot& shown = swapPreview ? page.grid[target] : slot;
        if (showItem || swapPreview)
        {
            ItemLabel(label, sizeof(label), shown.item);
            const ImVec2 text = ImGui::CalcTextSize(label);
            const ImU32 color = facet::ToU32(swapPreview ? c.labelSecondary : c.label);
            drawList->AddText(ImVec2(rect.GetCenter().x - text.x * 0.5f, rect.GetCenter().y - text.y * 0.5f), color, label);
            char count[16];
            if (i == target && source >= 0 && page.grid[source].item == slot.item)
            {
                // Stack preview: the count it would grow by.
                const int moved = effect == fi::DragDropEffect::Split ? page.grid[source].count / 2 : page.grid[source].count;
                std::snprintf(count, sizeof(count), "%d+%d", slot.count, moved);
            }
            else
            {
                std::snprintf(count, sizeof(count), "%d", shown.count);
            }
            const ImVec2 countSize = ImGui::CalcTextSize(count);
            const float inset = facet::Px(t.spacing.xs);
            drawList->AddText(ImVec2(rect.Max.x - inset - countSize.x, rect.Max.y - inset - countSize.y),
                              facet::ToU32(c.labelSecondary), count);
        }
        fi::DrawFocusRing(drawList, rect, radius, cells[i].focusAmount);
    }
    facet::PopTextStyle();
    ImGui::PopID();
}

// ---- Tree -------------------------------------------------------------------------------------

struct NodePayload
{
    int node = -1;
};

bool IsInSubtree(int node, int root)
{
    for (int n = node; n >= 0; n = page.nodes[n].parent)
        if (n == root)
            return true;
    return false;
}

// The node and its descendants: a run in `order` that starts at the node.
int SubtreeLength(int start)
{
    const int root = page.order[start];
    int length = 1;
    while (start + length < kTreeNodes && IsInSubtree(page.order[start + length], root))
        ++length;
    return length;
}

int OrderIndex(int node)
{
    for (int i = 0; i < kTreeNodes; ++i)
        if (page.order[i] == node)
            return i;
    return -1;
}

enum class TreePlace
{
    Before,
    After,
    Into,
};

// Moves `node` with its subtree before, after or into `target`.
void ApplyTreeDrop(int node, int target, TreePlace place)
{
    if (node == target || IsInSubtree(target, node))
        return;
    // Take the subtree out.
    int moved[kTreeNodes];
    const int from = OrderIndex(node);
    const int length = SubtreeLength(from);
    for (int i = 0; i < length; ++i)
        moved[i] = page.order[from + i];
    int rest[kTreeNodes];
    int count = 0;
    for (int i = 0; i < kTreeNodes; ++i)
        if (i < from || i >= from + length)
            rest[count++] = page.order[i];
    // Where it goes in what is left.
    int targetIndex = 0;
    while (targetIndex < count && rest[targetIndex] != target)
        ++targetIndex;
    int at = targetIndex;
    if (place != TreePlace::Before)
    {
        at = targetIndex + 1;
        while (at < count && IsInSubtree(rest[at], target))
            ++at;
    }
    page.nodes[node].parent = place == TreePlace::Into ? target : page.nodes[target].parent;
    if (place == TreePlace::Into)
        page.nodes[target].open = true;
    int out = 0;
    for (int i = 0; i < at; ++i)
        page.order[out++] = rest[i];
    for (int i = 0; i < length; ++i)
        page.order[out++] = moved[i];
    for (int i = at; i < count; ++i)
        page.order[out++] = rest[i];
}

int Depth(int node)
{
    int depth = 0;
    for (int n = page.nodes[node].parent; n >= 0; n = page.nodes[n].parent)
        ++depth;
    return depth;
}

bool Visible(int node)
{
    for (int n = page.nodes[node].parent; n >= 0; n = page.nodes[n].parent)
        if (!page.nodes[n].open)
            return false;
    return true;
}

const char* NodeProbe(int node)
{
    static const char* const kProbes[kTreeNodes] = {"dd.tree.g1", "dd.tree.i1", "dd.tree.i2",
                                                    "dd.tree.g2", "dd.tree.i3", "dd.tree.i4"};
    return kProbes[node];
}

void ProtoTree(facet::demo::PageCtx& ctx, int forcedNode, ImVec2 forcedOffset)
{
    const facet::Theme& t = facet::T();
    const facet::Colors& c = t.colors;
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImDrawList* drawList = window->DrawList;
    const float width = ListWidth();
    const float rowHeight = facet::Px(t.list.rowHeightSmall);
    const float radius = facet::Px(t.radii.control);
    ImGui::PushID("tree");

    fi::DragDropInfo drag;
    const bool dragging = fi::DragDropActive(kNodeType, &drag);
    const int dragged = dragging && drag.payload->DataSize == static_cast<int>(sizeof(NodePayload))
                            ? static_cast<const NodePayload*>(drag.payload->Data)->node
                            : -1;

    struct RowDraw
    {
        int node;
        fi::ItemState item;
        fi::DragDropZoneResult row;
        fi::DragDropZoneResult into;
    };
    RowDraw rows[kTreeNodes];
    int rowCount = 0;
    int lifted = -1;
    int delivered = -1;
    TreePlace deliveredPlace = TreePlace::Before;
    int deliveredNode = -1;
    char probe[48];

    // The surface, the same as the lists'.
    int visibleCount = 0;
    for (int i = 0; i < kTreeNodes; ++i)
        visibleCount += Visible(page.order[i]) ? 1 : 0;
    const float pad = facet::Px(t.spacing.xs);
    const float spacing = ImGui::GetStyle().ItemSpacing.y;
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float height = visibleCount * rowHeight + std::max(0, visibleCount - 1) * spacing + pad * 2.0f;
    facet::FillRect(drawList, start, ImVec2(start.x + width, start.y + height), facet::ToU32(c.bgSecondary),
                    facet::Px(t.radii.card));
    ImGui::SetCursorScreenPos(ImVec2(start.x + pad, start.y + pad));
    ImGui::BeginGroup();
    for (int i = 0; i < kTreeNodes; ++i)
    {
        const int node = page.order[i];
        if (!Visible(node))
            continue;
        const TreeNode& n = page.nodes[node];
        ImGui::PushID(node);
        if (ctx.capture && node == forcedNode)
            ctx.ForceNextItemState(ForcedItemState::Dragging);
        RowDraw& row = rows[rowCount++];
        row.node = node;
        row.item = fi::ItemBehavior(ImGui::GetID("row"), ImVec2(width - pad * 2.0f, rowHeight));
        page.treeRows[node] = row.item.rect;
        ctx.Probe(NodeProbe(node));
        if (row.item.pressed && n.group)
            page.nodes[node].open = !n.open;
        if (Has(row.item.forced, ForcedItemState::Dragging))
            fi::DragDropForceNextSource(forcedOffset);
        const NodePayload payload{node};
        if (fi::DragDropBeginSource(kNodeType, &payload, sizeof(payload),
                                    {.previewText = n.label, .icon = facet::Icon(n.group ? "folder" : "file")}))
        {
            lifted = node;
            fi::DragDropEndSource();
        }
        // A node never goes before, after or into itself or its own subtree.
        const bool allowed = dragged < 0 || !IsInSubtree(node, dragged);
        fi::DragDropZoneOptions rowOptions;
        const ImRect slot[] = {row.item.rect};
        rowOptions.slots = slot;
        rowOptions.accept = allowed;
        row.row = fi::DragDropZone(ImGui::GetID("row zone"), row.item.rect, kNodeType, rowOptions);
        if (n.group)
        {
            const ImRect into = fi::DragDropIntoRect(row.item.rect);
            fi::DragDropZoneOptions intoOptions;
            intoOptions.accept = allowed;
            row.into = fi::DragDropZone(ImGui::GetID("into"), into, kNodeType, intoOptions);
            std::snprintf(probe, sizeof(probe), "%s.into", NodeProbe(node));
            facet::demo::ProbeRect(ctx, probe, ImGui::GetID("into"), into.Min, into.Max);
            // Resting on a closed group opens it; a cancel closes it again.
            if (row.into.hoverOpen && !n.open)
            {
                page.nodes[node].open = true;
                page.openedByDrag = node;
            }
        }
        const fi::DragDropZoneResult& hit = row.into.delivered ? row.into : row.row;
        if (hit.delivered && hit.payload != nullptr)
        {
            delivered = node;
            deliveredNode = static_cast<const NodePayload*>(hit.payload->Data)->node;
            deliveredPlace = row.into.delivered ? TreePlace::Into : (hit.insertIndex == 0 ? TreePlace::Before : TreePlace::After);
        }
        ImGui::PopID();
    }
    ImGui::EndGroup();
    ImGui::SetCursorScreenPos(ImVec2(start.x, start.y + height));
    ImGui::Dummy(ImVec2(width, 0.0f));
    if (fi::DragDropCanceled() && page.openedByDrag >= 0)
        page.nodes[page.openedByDrag].open = false;
    if (!dragging)
        page.openedByDrag = -1;

    facet::PushTextStyle(facet::TextStyle::Body);
    const float iconSize = ImGui::GetFontSize();
    for (int r = 0; r < rowCount; ++r)
    {
        const RowDraw& row = rows[r];
        const TreeNode& n = page.nodes[row.node];
        const ImRect& rect = row.item.rect;
        if (row.into.hovered)
            fi::DragDropDrawTarget(drawList, rect, radius);
        else if (row.item.hoverAmount > 0.0f && row.node != lifted)
            facet::FillRect(drawList, rect.Min, rect.Max,
                            facet::ToU32(facet::WithAlpha(c.fill[2], c.fill[2].w * row.item.hoverAmount)), radius);
        // The lifted node stays in place, faded, so the tree keeps its shape around it.
        const ImU32 text = facet::ToU32(row.node == lifted ? c.labelTertiary : c.label);
        float x = rect.Min.x + facet::Px(t.spacing.xs) + Depth(row.node) * facet::Px(t.list.indent);
        const float iconY = rect.Min.y + (rect.GetHeight() - iconSize) * 0.5f;
        if (n.group)
            facet::DrawIcon(drawList, facet::Icon(n.open ? "chevron-down" : "chevron-right"), ImVec2(x, iconY), iconSize,
                            facet::ToU32(c.labelSecondary));
        x += iconSize + facet::Px(t.spacing.xs);
        facet::DrawIcon(drawList, facet::Icon(n.group ? "folder" : "file"), ImVec2(x, iconY), iconSize,
                        facet::ToU32(c.labelSecondary));
        x += iconSize + facet::Px(t.spacing.sm);
        drawList->AddText(ImVec2(x, facet::CenteredTextY(rect.Min.y, rect.GetHeight())), text, n.label);
        if (row.row.hovered)
        {
            const ImRect slot[] = {rect};
            ImRect line = fi::DragDropInsertLineRect(slot, row.row.insertIndex, facet::Orientation::Vertical);
            line.Min.x = rect.Min.x + Depth(row.node) * facet::Px(t.list.indent);
            fi::DragDropDrawInsertLine(drawList, line);
        }
        fi::DrawFocusRing(drawList, rect, radius, row.item.focusAmount);
    }
    facet::PopTextStyle();
    ImGui::PopID();
    if (delivered >= 0 && deliveredNode >= 0)
        ApplyTreeDrop(deliveredNode, delivered, deliveredPlace);
}

// ---- The zone that takes list items only ------------------------------------------------------

void ListOnlyZone(facet::demo::PageCtx& ctx)
{
    const facet::Theme& t = facet::T();
    const facet::Colors& c = t.colors;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const float width = ListWidth();
    const float height = facet::Px(t.list.rowHeight) * 2.0f;
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImRect rect(pos, ImVec2(pos.x + width, pos.y + height));
    ImGui::Dummy(rect.GetSize());
    const ImGuiID id = ImGui::GetID("only");
    page.onlyZone = rect;
    const fi::DragDropZoneResult drop = fi::DragDropZone(id, rect, kRowType);
    facet::demo::ProbeRect(ctx, "dd.only", id, rect.Min, rect.Max);
    if (drop.delivered && drop.payload->DataSize == static_cast<int>(sizeof(RowPayload)))
    {
        ++page.onlyDrops;
        page.onlyLastKey = static_cast<const RowPayload*>(drop.payload->Data)->key;
    }
    const float radius = facet::Px(t.radii.card);
    facet::FillRect(drawList, rect.Min, rect.Max, facet::ToU32(c.bgSecondary), radius);
    facet::StrokeRect(drawList, rect.Min, rect.Max, facet::ToU32(c.separator), radius, facet::HairlineThickness());
    if (drop.hovered)
        fi::DragDropDrawTarget(drawList, rect, radius);
    char text[64];
    if (page.onlyDrops > 0)
        std::snprintf(text, sizeof(text), "Takes list items. Last: Item %d", page.onlyLastKey);
    else
        std::snprintf(text, sizeof(text), "Takes list items only");
    facet::PushTextStyle(facet::TextStyle::Footnote);
    char cut[64];
    const fi::TruncateResult fit = fi::TruncateText(ImGui::GetFont(), ImGui::GetFontSize(), text, nullptr,
                                                    width - facet::Px(t.spacing.lg), fi::TruncateMode::End, cut, sizeof(cut));
    drawList->AddText(ImVec2(rect.GetCenter().x - fit.width * 0.5f, facet::CenteredTextY(rect.Min.y, height)),
                      facet::ToU32(c.labelSecondary), cut);
    facet::PopTextStyle();
}

// Captures: the pointer offset from the center of `from` to a point a little right of `to` (and
// below it, unless `below` is false), where a hand would hold an item it drags there.
ImVec2 CenterOffset(const ImRect& from, ImVec2 to, bool below = true)
{
    const facet::Theme& t = facet::T();
    const float down = below ? facet::Px(t.spacing.sm) : 0.0f;
    return ImVec2(to.x + facet::Px(t.spacing.xxl) - from.GetCenter().x, to.y + down - from.GetCenter().y);
}

} // namespace

FACET_DEMO_PAGE("drag-drop", "Drag and drop", "Blocks", 140, "default", "dragging", "transfer", "grid", "tree", "rejected")
{
    const facet::Theme& t = facet::T();
    const int state = ctx.capture ? ctx.captureState : kDefault;
    if (ctx.capture)
    {
        // Every capture starts from the same data.
        const PageState fresh;
        page.list = fresh.list;
        page.left = fresh.left;
        page.right = fresh.right;
        std::copy(std::begin(fresh.grid), std::end(fresh.grid), std::begin(page.grid));
        std::copy(std::begin(fresh.nodes), std::end(fresh.nodes), std::begin(page.nodes));
        std::copy(std::begin(fresh.order), std::end(fresh.order), std::begin(page.order));
    }
    facet::demo::PageHeader("Drag and drop",
                            "Typed drags on Dear ImGui's drag and drop: a press becomes a drag after a few units of "
                            "travel, a ghost of the item follows the pointer, and drop zones show where it would land. "
                            "Escape or a right click puts it back. Space picks a focused item up, the arrows move it, "
                            "Space or Enter drops it.");
    Note("Prototypes built only from the item behaviour, the focus ring and the drag and drop core. Each one applies "
         "the drops the core reports to its own data, as a widget's caller would; the core never moves data.");

    // ---- Reorderable list ----
    SectionHeader("Reorderable list");
    {
        const float pitch = facet::Px(t.list.rowHeight) + ImGui::GetStyle().ItemSpacing.y;
        ListLook look;
        look.probe = "dd.list";
        if (state == kDragging)
        {
            look.forcedRow = 0;
            look.forcedOffset = ImVec2(facet::Px(t.spacing.xxl), pitch * 2.6f);
        }
        const ListDrop drop = ProtoList(ctx, "list", page.list, ListWidth(), look);
        ApplyListDrop(page.list, drop);
        if (state == kDragging)
            ctx.RevealInCapture(0.5f);
        Note("The rows move apart to show the gap; the order changes only on the drop.");
    }

    // ---- Two lists ----
    SectionHeader("Two lists");
    Note("An insertion line marks the place; Ctrl copies. The zone under the lists takes list rows only and refuses "
         "grid items.");
    {
        const float width = std::min(ListWidth(), (ImGui::GetContentRegionAvail().x - facet::Px(t.spacing.lg)) * 0.5f);
        const bool sideBySide = width * 2.0f + facet::Px(t.spacing.lg) <= ImGui::GetContentRegionAvail().x &&
                                width >= facet::Px(t.metrics.cardWidth) * 0.5f;
        const float listWidth = sideBySide ? width : ListWidth();
        ListLook leftLook;
        leftLook.line = true;
        leftLook.allowCopy = true;
        leftLook.probe = "dd.a";
        if (state == kTransfer)
        {
            leftLook.forcedRow = 1;
            leftLook.forcedOffset = CenterOffset(page.leftRow1, ImVec2(page.rightZone.Min.x + listWidth * 0.4f,
                                                                      page.rightZone.Min.y + facet::Px(t.list.rowHeight) * 1.5f));
        }
        ListLook rightLook = leftLook;
        rightLook.probe = "dd.b";
        rightLook.forcedRow = -1;
        ImGui::BeginGroup();
        const ListDrop toLeft = ProtoList(ctx, "left", page.left, listWidth, leftLook, nullptr, &page.leftRow1, 1);
        ImGui::EndGroup();
        if (sideBySide)
            ImGui::SameLine(0.0f, facet::Px(t.spacing.lg));
        ImGui::BeginGroup();
        const ListDrop toRight = ProtoList(ctx, "right", page.right, listWidth, rightLook, &page.rightZone);
        ImGui::EndGroup();
        ApplyListDrop(page.left, toLeft);
        ApplyListDrop(page.right, toRight);
        if (state == kTransfer)
            ctx.RevealInCapture(0.5f);
        ListOnlyZone(ctx);
        if (state == kRejected)
            ctx.RevealInCapture(0.4f);
    }

    // ---- Grid ----
    SectionHeader("Grid of slots");
    {
        int forcedCell = -1;
        ImVec2 offset(0.0f, 0.0f);
        if (state == kGrid)
        {
            forcedCell = 0;
            offset = CenterOffset(page.gridCells[0], page.gridCells[4].GetCenter());
        }
        else if (state == kRejected)
        {
            forcedCell = 0;
            offset = CenterOffset(page.gridCells[0], page.onlyZone.GetCenter());
        }
        ProtoGrid(ctx, forcedCell, offset);
        if (state == kGrid)
            ctx.RevealInCapture(0.5f);
        Note("Drop on an empty slot to move, on the same item to stack, on another item to swap. Hold Shift to move "
             "half a stack.");
    }

    // ---- Tree ----
    SectionHeader("Tree");
    {
        int forcedNode = -1;
        ImVec2 offset(0.0f, 0.0f);
        if (state == kTree)
        {
            forcedNode = 5;
            // The middle of Group 1's row: the part that puts the item inside.
            offset = CenterOffset(page.treeRows[5], page.treeRows[0].GetCenter(), false);
        }
        ProtoTree(ctx, forcedNode, offset);
        if (state == kTree)
            ctx.RevealInCapture(0.5f);
        Note("The top and bottom of a row place the item before or after it, the middle of a group puts it inside. "
             "Resting on a closed group opens it. A group never goes inside itself.");
    }

    // ---- Long list ----
    SectionHeader("Long list in a scroll area");
    {
        const float pitch = facet::Px(t.list.rowHeight) + ImGui::GetStyle().ItemSpacing.y;
        const float width = ListWidth();
        ImGui::PushStyleColor(ImGuiCol_ChildBg, facet::ToU32(t.colors.bgSecondary));
        // Flattened navigation: Tab reaches the rows from the page as if they were on it.
        if (ImGui::BeginChild("##long", ImVec2(width, pitch * kLongVisibleRows + facet::Px(t.spacing.xs) * 2.0f),
                              ImGuiChildFlags_NavFlattened))
        {
            page.longWindow = ImGui::GetCurrentWindow()->ID;
            ListLook look;
            look.probe = "dd.long";
            const ListDrop drop = ProtoList(ctx, "long", page.longList, ImGui::GetContentRegionAvail().x, look);
            const int at = ApplyListDrop(page.longList, drop);
            if (drop.delivered)
                page.longDroppedAt = at;
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        facet::demo::ProbeRect(ctx, "dd.long", page.longWindow, ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        Note("Near the top or bottom edge a drag asks the scroll area to scroll (edge scrolling of the scroll core). "
             "Asked in the last frame: %s.",
             fi::DragDropRequestedEdgeScroll(page.longWindow) ? "yes" : "no");
    }

    SectionHeader("Motion");
    fi::BeginRow("Reduce motion", {.description = "Rows then jump to their places."});
    bool reduce = t.motion.reduceMotion;
    if (facet::demo::ProtoToggle(ctx, "##value", &reduce, "dd.reduce"))
    {
        facet::Theme theme = facet::T();
        theme.motion.reduceMotion = reduce;
        facet::SetTheme(theme, false);
    }
    fi::EndRow();
}

namespace
{

int ListKey(const ListModel& list, int index)
{
    return index < list.count ? list.keys[index] : -1;
}

} // namespace

FACET_DEMO_SELFTEST("drag-drop")
{
    using T = facet::demo::Target;
    test.Do([] { page = {}; });

    // A click is not a drag.
    test.Click("dd.list.1");
    test.Expect("a click leaves the order", [] { return ListKey(page.list, 0) == 1 && ListKey(page.list, 1) == 2; });

    // The pointer: Item 1 into the gap before the fourth row.
    test.DragAndHold({T::Item("dd.list.0"), T::Item("dd.list.3", 0.5f, 0.2f)}, 0.4f, 0.1f);
    test.Expect("Item 1 lands after Item 3",
                [] { return ListKey(page.list, 0) == 2 && ListKey(page.list, 1) == 3 && ListKey(page.list, 2) == 1; });

    // A right click puts it back.
    test.MouseMove(T::Item("dd.list.4"));
    test.MouseDown();
    test.MouseMove(T::Item("dd.list.3"));
    test.MouseMove(T::Item("dd.list.1", 0.5f, 0.2f));
    test.Click("dd.list.1", ImGuiMouseButton_Right);
    test.MouseUp();
    test.Expect("a right click cancels", [] { return ListKey(page.list, 4) == 5 && ListKey(page.list, 1) == 3; });

    // Escape puts it back too.
    test.MouseMove(T::Item("dd.list.4"));
    test.MouseDown();
    test.MouseMove(T::Item("dd.list.2"));
    test.MouseMove(T::Item("dd.list.0", 0.5f, 0.2f));
    test.KeyPress(ImGuiKey_Escape);
    test.MouseUp();
    test.Expect("Escape cancels", [] { return ListKey(page.list, 4) == 5 && ListKey(page.list, 0) == 2; });

    // The keyboard: the focused row is picked up, moved down two places and dropped.
    test.Click("dd.list.0");
    test.KeyPress(ImGuiKey_Space);
    test.KeyPress(ImGuiKey_DownArrow);
    test.KeyPress(ImGuiKey_DownArrow);
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("Space, Down, Down, Enter moves Item 2 to the third place",
                [] { return ListKey(page.list, 0) == 3 && ListKey(page.list, 1) == 1 && ListKey(page.list, 2) == 2; });
    test.ExpectFocused("dd.list.2");
    test.KeyPress(ImGuiKey_Space);
    test.KeyPress(ImGuiKey_UpArrow);
    test.KeyPress(ImGuiKey_Escape);
    test.Expect("Escape puts a keyboard drag back", [] { return ListKey(page.list, 2) == 2; });

    // Two lists: the last row of A into the first gap of B, then back with the keyboard.
    test.DragAndHold({T::Item("dd.a.3"), T::Item("dd.b.0", 0.5f, 0.2f)}, 0.5f, 0.1f);
    test.Expect("Item 4 moves to the top of B", [] {
        return page.left.count == 3 && page.right.count == 3 && ListKey(page.right, 0) == 4;
    });
    // Side by side, Right reaches B; stacked, Down from A's last place does. Either way the item
    // ends in B.
    test.Click("dd.a.2");
    test.KeyPress(ImGuiKey_Space);
    test.KeyPress(ImGuiKey_RightArrow);
    test.KeyPress(ImGuiKey_DownArrow);
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("the keyboard moves Item 3 into B", [] {
        bool inRight = false;
        for (int i = 0; i < page.right.count; ++i)
            inRight = inRight || page.right.keys[i] == 3;
        return page.left.count == 2 && page.right.count == 4 && inRight;
    });

    // The zone of one type: a list row is taken, a grid item is not. The zone in the middle of the
    // view keeps the rows above it and the grid below it in sight on small screens.
    test.Reveal("dd.only");
    test.DragAndHold({T::Item("dd.b.3"), T::Item("dd.only")}, 0.4f, 0.1f);
    test.Expect("the zone takes a list row",
                [] { return page.onlyDrops == 1 && page.onlyLastKey == ListKey(page.right, 3); });
    test.DragAndHold({T::Item("dd.grid.0"), T::Item("dd.only")}, 0.4f, 0.1f);
    test.Expect("the zone refuses a grid item", [] { return page.onlyDrops == 1 && page.grid[0].item == 1; });

    // The grid: swap two items, stack two of the same, then the keyboard.
    test.DragAndHold({T::Item("dd.grid.0"), T::Item("dd.grid.4")}, 0.4f, 0.1f);
    test.Expect("Item 1 and Item 3 swap", [] { return page.grid[0].item == 3 && page.grid[4].item == 1; });
    test.DragAndHold({T::Item("dd.grid.2"), T::Item("dd.grid.3")}, 0.4f, 0.1f);
    test.Expect("the two Item 2 stacks merge", [] { return page.grid[2].item == 0 && page.grid[3].count == 3; });
    test.Click("dd.grid.4");
    test.KeyPress(ImGuiKey_Space);
    test.KeyPress(ImGuiKey_RightArrow);
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("the keyboard moves Item 1 to the empty slot",
                [] { return page.grid[5].item == 1 && page.grid[4].item == 0; });

    // The tree: Item 4 into Group 1; resting on the closed Group 2 opens it.
    test.Reveal("dd.tree.i2");
    test.DragAndHold({T::Item("dd.tree.i4"), T::Item("dd.tree.g1.into")}, 0.4f, 0.1f);
    test.Expect("Item 4 goes into Group 1", [] { return page.nodes[5].parent == 0; });
    test.DragAndHold({T::Item("dd.tree.i1"), T::Item("dd.tree.g2.into")}, 0.4f, 0.8f);
    test.Expect("resting on Group 2 opened it and Item 1 is inside",
                [] { return page.nodes[3].open && page.nodes[1].parent == 3; });
    test.DragAndHold({T::Item("dd.tree.g2"), T::Item("dd.tree.i3", 0.5f, 0.5f)}, 0.4f, 0.1f);
    test.Expect("a group does not go inside itself", [] { return page.nodes[3].parent == -1; });

    // The long list: a drag held at the bottom edge asks for edge scrolling, and the drop lands
    // where the gap was shown.
    test.Reveal("dd.long");
    test.MouseMove(T::Item("dd.long.0"));
    test.MouseDown();
    test.MouseMove(T::Item("dd.long.1"));
    test.MouseMove(T::Item("dd.long", 0.5f, 0.97f));
    test.WaitSeconds(0.3f);
    test.Expect("the drag asks the list to scroll", [] { return fi::DragDropRequestedEdgeScroll(page.longWindow); });
    test.MouseUp();
    test.Expect("the row lands in the shown gap", [] {
        return page.longDroppedAt >= kLongVisibleRows - 2 && ListKey(page.longList, page.longDroppedAt) == 1;
    });
}
