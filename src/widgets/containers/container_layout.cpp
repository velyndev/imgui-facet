// Layout helpers of the containers family (container_layout.h).

#include "widgets/containers/container_layout.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "facet/core/anim.h"
#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/row_layout.h"
#include "internal/state_storage.h"

namespace facet::internal
{
namespace
{

constexpr Icon kChevronRight("chevron-right");
constexpr Icon kChevronDown("chevron-down");
constexpr Icon kChevronUp("chevron-up");

// The largest share of a header row's free width its trailing text keeps; the label has the rest.
constexpr float kTrailingShare = 1.0f / 3.0f;

struct BackdropPool
{
    ImDrawListSplitter splitters[kContainerBackdropDepth];
    ImDrawList* drawLists[kContainerBackdropDepth] = {};
    int depth = 0;
};

BackdropPool& Backdrops()
{
    static BackdropPool pool;
    return pool;
}

void MergeTop(BackdropPool& pool)
{
    --pool.depth;
    pool.splitters[pool.depth].Merge(pool.drawLists[pool.depth]);
    pool.drawLists[pool.depth] = nullptr;
}

// Animation sub key of the open amount, below the item behaviour's keys.
constexpr int kCollapseSub = kItemAnimSubBase - 0x100;

// The content height a region measured in its last End.
struct CollapseMeasure
{
    float height = 0.0f;
};

struct SectionOpenState
{
    static constexpr const char* kStateName = "Section";
    bool open = false;
    bool known = false;  // set by a first use or read from the ini file
    void WriteIni(ImGuiTextBuffer& out) const { out.appendf("open=%d\n", open ? 1 : 0); }
    void ReadIni(const char* key, const char* value)
    {
        if (std::strcmp(key, "open") == 0)
        {
            open = std::atoi(value) != 0;
            known = true;
        }
    }
};

} // namespace

bool ContainerOpenState(ImGuiID id, bool defaultOpen, const NextItemData& next)
{
    SectionOpenState& state = GetPersistentState<SectionOpenState>(id);
    bool changed = false;
    if ((next.data.HasFlags & ImGuiNextItemDataFlags_HasOpen) != 0)
    {
        const bool always = next.data.OpenCond == 0 || (next.data.OpenCond & ImGuiCond_Always) != 0;
        if ((always || !state.known) && (state.open != next.data.OpenVal || !state.known))
        {
            state.open = next.data.OpenVal;
            changed = true;
        }
        state.known = true;
    }
    if (!state.known)
    {
        state.open = defaultOpen;
        state.known = true;
        changed = true;
    }
    if (changed)
        MarkStateDirty();
    return state.open;
}

void SetContainerOpenState(ImGuiID id, bool open)
{
    SectionOpenState& state = GetPersistentState<SectionOpenState>(id);
    if (state.open != open || !state.known)
    {
        state.open = open;
        state.known = true;
        MarkStateDirty();
    }
}

int BeginContainerBackdrop(ImDrawList* drawList)
{
    BackdropPool& pool = Backdrops();
    if (drawList == nullptr || pool.depth >= kContainerBackdropDepth)
        return -1;
    const int slot = pool.depth++;
    pool.drawLists[slot] = drawList;
    pool.splitters[slot].Split(drawList, 2);
    pool.splitters[slot].SetCurrentChannel(drawList, 1);
    return slot;
}

void ContainerBackdropBehind(int slot)
{
    BackdropPool& pool = Backdrops();
    if (slot < 0 || slot >= pool.depth)
        return;
    while (pool.depth > slot + 1)
        MergeTop(pool);
    pool.splitters[slot].SetCurrentChannel(pool.drawLists[slot], 0);
}

void EndContainerBackdrop(int slot)
{
    BackdropPool& pool = Backdrops();
    if (slot < 0 || slot >= pool.depth)
        return;
    while (pool.depth > slot)
        MergeTop(pool);
}

void ClearContainerBackdrops()
{
    BackdropPool& pool = Backdrops();
    while (pool.depth > 0)
        MergeTop(pool);
}

void BeginContainerInset(ContainerInset* inset, float width, float padding)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    *inset = ContainerInset{};
    inset->window = window;
    inset->start = window->DC.CursorPos;
    inset->right = inset->start.x + std::max(1.0f, width);
    inset->padding = padding;
    inset->savedWorkMaxX = window->WorkRect.Max.x;
    inset->savedContentMaxX = window->ContentRegionRect.Max.x;
    // The n-th inset of the frame: the same container gets the same column key in every frame
    // while the layout keeps its order.
    static int frame = -1;
    static int ordinal = 0;
    if (frame != GImGui->FrameCount)
    {
        frame = GImGui->FrameCount;
        ordinal = 0;
    }
    ++ordinal;
    inset->columnKey = ImHashData(&ordinal, sizeof(ordinal), ImHashStr("##facet.insetcolumn", 0, window->ID));
    ImGui::BeginGroup();
}

void OpenContainerInset(ContainerInset& inset)
{
    ImGuiWindow* window = inset.window;
    ImGui::Indent(inset.padding);
    const float innerRight = std::max(window->DC.CursorPos.x, inset.right - inset.padding);
    window->WorkRect.Max.x = std::min(window->WorkRect.Max.x, innerRight);
    window->ContentRegionRect.Max.x = std::min(window->ContentRegionRect.Max.x, innerRight);
    // Settings rows inside form a column of their own, so they do not keep room for the reset
    // buttons of rows outside the container.
    inset.savedColumnKey = SetRowColumnKey(inset.columnKey);
    inset.opened = true;
}

void RestoreContainerInset(const ContainerInset& inset)
{
    if (inset.window == nullptr)
        return;
    inset.window->WorkRect.Max.x = inset.savedWorkMaxX;
    inset.window->ContentRegionRect.Max.x = inset.savedContentMaxX;
    if (inset.opened)
        (void)SetRowColumnKey(inset.savedColumnKey);
}

ImRect EndContainerInset(const ContainerInset& inset, float bottomPadding, float minHeight)
{
    ImGuiContext& g = *GImGui;
    RestoreContainerInset(inset);
    if (inset.opened)
        ImGui::Unindent(inset.padding);
    // The last item already left the item spacing under it; the padding makes up the rest.
    const float spacing = g.Style.ItemSpacing.y;
    const float filler = std::max(0.0f, bottomPadding - spacing);
    const float height = std::max(filler, inset.start.y + minHeight - inset.window->DC.CursorPos.y);
    ImGui::Dummy(ImVec2(inset.right - inset.start.x, height));
    ImGui::EndGroup();
    return ImRect(g.LastItemData.Rect.Min, ImVec2(inset.right, g.LastItemData.Rect.Max.y));
}

float ContainerHeaderLabelOffset(TextStyle style)
{
    const ControlTextScope text(style);
    return Px(T().spacing.sm) * 2.0f + text.IconSize();
}

ItemState ContainerHeader(ImGuiID id, const ContainerHeaderDesc& desc, const ItemOptions& options)
{
    const Theme& theme = T();
    const ControlTextScope text(desc.style);
    const float gap = Px(theme.spacing.sm);
    const float iconSize = text.IconSize();
    const char* label = desc.label != nullptr ? desc.label : "";
    const char* labelEnd = desc.labelEnd != nullptr ? desc.labelEnd : label + std::strlen(label);
    const float labelWidth = labelEnd != label ? ImGui::CalcTextSize(label, labelEnd).x : 0.0f;
    const bool link = desc.look == ContainerHeaderStyle::Link;
    const float radius = Px(theme.radii.control);

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    float width = desc.width;
    const bool hasTrailing = desc.trailing != nullptr && desc.trailing[0] != '\0';
    float linkTrailing = 0.0f;  // the link's trailing text after its chevron, pixels
    if (link)
    {
        if (hasTrailing)
            linkTrailing = gap + ImGui::CalcTextSize(desc.trailing).x;
        width = std::ceil(labelWidth + (labelWidth > 0.0f ? gap * 0.5f : 0.0f) + iconSize + linkTrailing);
        const float available = ImGui::GetContentRegionAvail().x;
        if (available > 0.0f)
            width = std::min(width, available);
    }
    const float top = window->DC.CursorPos.y;
    ItemOptions itemOptions = options;
    itemOptions.baseline = CenteredTextY(top, desc.height) - top;
    const ItemState item = ItemBehavior(id, ImVec2(width, desc.height), itemOptions);

    bool cut = false;
    if (item.visible)
    {
        ImDrawList* drawList = window->DrawList;
        const ImRect& r = item.rect;
        const float centerY = r.GetCenter().y;
        const float textY = CenteredTextY(r.Min.y, r.GetHeight());
        if (link)
        {
            const ImVec4 color = item.disabled ? theme.colors.labelTertiary : theme.colors.accentText;
            const ImU32 packed = ToU32(color);
            // When narrow, the trailing text goes first, then the label is cut.
            const float chevronRoom = iconSize + (labelWidth > 0.0f ? gap * 0.5f : 0.0f);
            const FittedText shown = FitText(label, labelEnd, std::max(0.0f, r.GetWidth() - chevronRoom));
            cut = shown.truncated;
            drawList->AddText(ImVec2(r.Min.x, textY), packed, shown.text);
            const float chevronX = r.Min.x + shown.width + (labelWidth > 0.0f ? gap * 0.5f : 0.0f);
            DrawControlIcon(drawList, desc.open ? kChevronUp : kChevronDown, chevronX, centerY, iconSize, packed);
            if (hasTrailing && !cut)
            {
                const float tx = chevronX + iconSize + gap;
                const FittedText trailing = FitText(desc.trailing, nullptr, std::max(0.0f, r.Max.x - tx));
                drawList->AddText(ImVec2(tx, textY), ToU32(ControlSecondaryColor(item.disabled)), trailing.text);
            }
            // The underline is the hover cue, so the link does not rely on color alone.
            if (item.hoverAmount > 0.0f && shown.width > 0.0f)
                HairlineH(drawList, r.Min.x, r.Min.x + shown.width, textY + ImGui::GetFontSize(),
                          ToU32(WithAlpha(color, color.w * item.hoverAmount)));
            DrawItemFocusRing(item, radius);
        }
        else
        {
            if (!item.disabled)
            {
                const ImVec4 rest = WithAlpha(theme.colors.fill[2], 0.0f);
                const ImVec4 fill = StateFill(rest, theme.colors.fill[2], theme.colors.fill[1], item);
                if (fill.w > 0.0f)
                    FillRect(drawList, r.Min, r.Max, ToU32(fill), radius);
            }
            const ImU32 secondary = ToU32(ControlSecondaryColor(item.disabled));
            float x = r.Min.x + gap;
            DrawControlIcon(drawList, desc.open ? kChevronDown : kChevronRight, x, centerY, iconSize, secondary);
            x += iconSize + gap;
            if (desc.icon.IsValid())
            {
                DrawControlIcon(drawList, desc.icon, x, centerY, iconSize, secondary);
                x += iconSize + gap;
            }
            const float right = r.Max.x - gap;
            // The trailing text gives way first: it keeps at most a third of the free width.
            FittedText trailing;
            if (hasTrailing)
                trailing = FitText(desc.trailing, nullptr, std::max(0.0f, (right - x) * kTrailingShare));
            const float trailingRoom = hasTrailing ? trailing.width + gap : 0.0f;
            const FittedText shown = FitText(label, labelEnd, std::max(0.0f, right - x - trailingRoom));
            cut = shown.truncated;
            drawList->AddText(ImVec2(x, textY), ToU32(ControlLabelColor(item.disabled)), shown.text);
            if (hasTrailing)
            {
                const float tx = desc.trailingAfterLabel ? x + shown.width + gap : right - trailing.width;
                drawList->AddText(ImVec2(tx, textY), secondary, trailing.text);
            }
            DrawItemFocusRing(item, radius);
        }
    }
    CutTextTooltip(item, label, labelEnd, cut);
    return item;
}

float ContainerCollapseAmount(ImGuiID id, bool open)
{
    return Tween(id, kCollapseSub, open ? 1.0f : 0.0f, T().motion.durationStandard, Ease::OutCubic);
}

bool BeginContainerCollapse(ImGuiID id, bool open, ContainerCollapse* collapse)
{
    *collapse = ContainerCollapse{};
    const float amount = ContainerCollapseAmount(id, open);
    if (!open && amount <= 0.0f)
        return false;
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    collapse->window = window;
    collapse->id = id;
    collapse->amount = amount;
    collapse->top = window->DC.CursorPos.y;
    collapse->lineX = window->DC.CursorPos.x;
    collapse->savedMaxPos = window->DC.CursorMaxPos;
    // Measure the content from its own top.
    window->DC.CursorMaxPos.y = collapse->top;
    if (amount < 1.0f)
    {
        const float lastHeight = GetState<CollapseMeasure>(id).height;
        const float visible = std::floor(lastHeight * amount);
        ImGui::PushClipRect(ImVec2(window->ClipRect.Min.x, collapse->top),
                            ImVec2(window->ClipRect.Max.x, collapse->top + visible), true);
        collapse->clipped = true;
    }
    if (!open)
    {
        // A closing region is on its way out: Tab and the arrows skip it.
        ImGui::PushItemFlag(ImGuiItemFlags_NoTabStop | ImGuiItemFlags_NoNav, true);
        collapse->noFocus = true;
    }
    return true;
}

void EndContainerCollapse(const ContainerCollapse& collapse)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = collapse.window;
    if (window == nullptr || g.CurrentWindow != window)
        return;
    if (collapse.noFocus)
        ImGui::PopItemFlag();
    if (collapse.clipped)
        ImGui::PopClipRect();

    const float content = std::max(0.0f, window->DC.CursorMaxPos.y - collapse.top);
    GetState<CollapseMeasure>(collapse.id).height = content;
    // Fully open, the region takes the content and the spacing after its last item, as if the
    // content were laid out without it; on the way the two shrink together, so nothing jumps.
    const float spacing = content > 0.0f ? g.Style.ItemSpacing.y : 0.0f;
    const float laid = collapse.amount >= 1.0f ? content + spacing : std::floor((content + spacing) * collapse.amount);
    const float maxX = window->DC.CursorMaxPos.x;
    window->DC.CursorMaxPos = collapse.savedMaxPos;
    window->DC.CursorMaxPos.x = std::max(window->DC.CursorMaxPos.x, maxX);
    window->DC.CursorMaxPos.y = std::max(window->DC.CursorMaxPos.y, collapse.top + std::max(0.0f, laid - spacing));
    window->DC.CursorPosPrevLine = ImVec2(collapse.lineX, collapse.top);
    window->DC.CursorPos = ImVec2(IM_TRUNC(window->Pos.x + window->DC.Indent.x + window->DC.ColumnsOffset.x),
                                  IM_TRUNC(collapse.top + laid));
    window->DC.PrevLineSize.y = laid;
    window->DC.CurrLineSize.y = 0.0f;
    window->DC.PrevLineTextBaseOffset = 0.0f;
    window->DC.CurrLineTextBaseOffset = 0.0f;
    window->DC.IsSameLine = false;
    window->DC.IsSetPos = false;
}

} // namespace facet::internal
