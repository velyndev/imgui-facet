// BeginPanels, BeginPanel and their Ends (layout.h): resizable panels with headers.
//
// Layout: the panels share the view (`width` x `height`, 0 fills the available room; with no height
// left, metrics.panelHeight) along their orientation, with a divider band of layout.dividerBand
// between neighbours. A collapsed panel takes a strip of layout.panelHeaderHeight; the expanded
// panels share the rest in the proportion of their `sizes` entries (normalized; all 0 share it
// equally). A panel collapses and expands over motion.durationStandard (at once with reduced
// motion): its length moves between the two, so its neighbours grow and shrink with it. In a
// Compact width horizontal panels stack.
//
// Each expanded panel starts with a header row of layout.panelHeaderHeight: the chevron, the icon
// and the title (Headline), cut with an ellipsis and a tooltip when narrow, and a hairline under it.
// A collapsible panel's header is one item (the containers family's header row) that collapses the
// panel on a click, Enter or Space; a panel that is not collapsible draws its header as text. A
// collapsed panel shows its strip: side by side, an icon button with the chevron at the top whose
// tooltip and name are the title; stacked, the header row alone. The content is a scroll area of the
// scroll core under the header. The collapsed state is kept under the panel's id in the Dear ImGui
// ini file; the view keeps the last frame's states by index for its layout.
//
// Dividers sit between two expanded neighbours and are submitted with the second one, so Tab goes
// from a panel's content to the divider to the next panel. Dragging one, or its arrow keys (Shift
// fine, Ctrl coarse), Home and End, moves room between its two neighbours only and writes their two
// entries of `sizes`, keeping their sum. A neighbour never gets shorter than layout.minRowControl
// while both fit it. The new sizes take effect in the next frame.

#include "facet/widgets/layout.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include <imgui.h>
#include <imgui_internal.h>

#include "core/core_internal.h"
#include "facet/core/anim.h"
#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/layout.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "facet/widgets/buttons.h"
#include "internal/control.h"
#include "internal/scope_stack.h"
#include "internal/scroll.h"
#include "internal/state_storage.h"
#include "widgets/containers/container_layout.h"
#include "widgets/layout/layout_internal.h"

namespace facet
{
namespace
{

// Panels per view that keep their state; further panels are reported and skipped.
constexpr int kPanelsMax = 16;
// Animation sub key of a panel's collapse amount (one per index).
constexpr int kPanelsCollapseSub = 0x100;

// The collapsed states of the last frame, by index, under the view's id.
struct PanelsMemory
{
    bool collapsed[kPanelsMax] = {};
};

struct PanelsEntry
{
    bool active = false;
    ImGuiWindow* window = nullptr;
    ImGuiID id = 0;
    std::span<float> sizes;
    int count = 0;
    bool vertical = false;
    ImVec2 origin = {0.0f, 0.0f};
    ImVec2 size = {0.0f, 0.0f};
    float band = 0.0f;
    float strip = 0.0f;
    float start[kPanelsMax] = {};   // offset of each panel along the axis
    float length[kPanelsMax] = {};  // each panel's length along the axis
    float amount[kPanelsMax] = {};  // 0 expanded to 1 collapsed
    int index = 0;                  // panels begun
    bool panelOpen = false;
    bool panelArea = false;
    PanelsMemory next;              // this frame's collapsed states
};

struct PanelsStack
{
    PanelsEntry entries[internal::kScopeStackDepth];
    int depth = 0;
    int overflow = 0;
};

PanelsStack& PanelViews()
{
    static PanelsStack stack;
    return stack;
}

// The dividers of the panels view ended last, and of the one in progress.
struct PanelsDividers
{
    internal::LayoutDividerRecord current[kPanelsMax];
    internal::LayoutDividerRecord last[kPanelsMax];
};

PanelsDividers& Dividers()
{
    static PanelsDividers dividers;
    return dividers;
}

PanelsEntry* TopPanels()
{
    PanelsStack& stack = PanelViews();
    if (stack.overflow > 0 || stack.depth == 0)
        return nullptr;
    PanelsEntry& entry = stack.entries[stack.depth - 1];
    return entry.active ? &entry : nullptr;
}

float PanelShare(std::span<float> sizes, int i)
{
    const float value = sizes[static_cast<size_t>(i)];
    return value > 0.0f ? value : 0.0f;
}

ImRect PanelRect(const PanelsEntry& e, int i)
{
    if (e.vertical)
        return ImRect(ImVec2(e.origin.x, e.origin.y + e.start[i]),
                      ImVec2(e.origin.x + e.size.x, e.origin.y + e.start[i] + e.length[i]));
    return ImRect(ImVec2(e.origin.x + e.start[i], e.origin.y),
                  ImVec2(e.origin.x + e.start[i] + e.length[i], e.origin.y + e.size.y));
}

ImRect PanelBand(const PanelsEntry& e, int i)  // the band before panel i
{
    const float s = e.start[i] - e.band;
    if (e.vertical)
        return ImRect(ImVec2(e.origin.x, e.origin.y + s), ImVec2(e.origin.x + e.size.x, e.origin.y + s + e.band));
    return ImRect(ImVec2(e.origin.x + s, e.origin.y), ImVec2(e.origin.x + s + e.band, e.origin.y + e.size.y));
}

// Runs the divider before panel i, between panels i - 1 and i, and writes their sizes.
void RunPanelDivider(PanelsEntry& e, int i)
{
    const int a = i - 1;
    const bool movable = e.amount[a] <= 0.0f && e.amount[i] <= 0.0f;
    const ImRect band = PanelBand(e, i);
    const ImGuiAxis axis = e.vertical ? ImGuiAxis_Y : ImGuiAxis_X;
    if (!movable)
    {
        // Beside a collapsed panel there is nothing to move: the hairline alone.
        const ImU32 color = ToU32(T().colors.separator);
        const ImVec2 center = band.GetCenter();
        const float half = HairlineThickness() * 0.5f;
        if (e.vertical)
            HairlineH(e.window->DrawList, band.Min.x, band.Max.x, std::round(center.y - half), color);
        else
            HairlineV(e.window->DrawList, std::round(center.x - half), band.Min.y, band.Max.y, color);
        return;
    }
    const ImGuiID dividerId = ImHashData(&i, sizeof(i), ImHashStr("##divider", 0, e.id));
    const internal::LayoutDividerInput input =
        internal::LayoutDivider(dividerId, band, axis, e.length[a], (e.vertical ? e.origin.y : e.origin.x) + e.start[a]);
    internal::DrawLayoutDivider(input.item, band, axis);
    Dividers().current[i] = {dividerId, band, e.vertical};
    const float pair = e.length[a] + e.length[i];
    const float minimum = std::min(Px(T().layout.minRowControl), pair * 0.5f);
    float target = e.length[a];
    if (input.dragging)
        target = input.dragTarget;
    else if (input.steps != 0)
        target = e.length[a] + input.stepPixels * static_cast<float>(input.steps);
    else if (input.home)
        target = minimum;
    else if (input.end)
        target = pair - minimum;
    else
        return;
    target = std::clamp(target, minimum, pair - minimum);
    if (std::fabs(target - e.length[a]) < 0.5f || pair <= 0.0f)
        return;
    const float sum = PanelShare(e.sizes, a) + PanelShare(e.sizes, i);
    const float total = sum > 0.0f ? sum : 1.0f;
    e.sizes[static_cast<size_t>(a)] = total * target / pair;
    e.sizes[static_cast<size_t>(i)] = total * (pair - target) / pair;
}

// Draws the title of a header that is not an item: icon and title, cut with a tooltip.
void DrawPanelTitle(const ImRect& row, const char* label, const char* labelEnd, Icon icon, bool disabled, ImGuiID id)
{
    const Theme& theme = T();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const internal::ControlTextScope text(TextStyle::Headline);
    float x = row.Min.x + Px(theme.spacing.sm);
    const float centerY = row.GetCenter().y;
    if (icon.IsValid())
    {
        internal::DrawControlIcon(drawList, icon, x, centerY, text.IconSize(), ToU32(internal::ControlSecondaryColor(disabled)));
        x += text.IconSize() + Px(theme.spacing.sm);
    }
    const internal::FittedText shown = internal::FitText(label, labelEnd, std::max(0.0f, row.Max.x - Px(theme.spacing.sm) - x));
    drawList->AddText(ImVec2(x, CenteredTextY(row.Min.y, row.GetHeight())), ToU32(internal::ControlLabelColor(disabled)),
                      shown.text);
    const ImRect anchor(ImVec2(x, row.Min.y), ImVec2(x + shown.width, row.Max.y));
    const bool hovered = GImGui->HoveredWindow == ImGui::GetCurrentWindow() && ImGui::IsMouseHoveringRect(anchor.Min, anchor.Max);
    internal::CutTextTooltip(id, anchor, hovered, false, label, labelEnd, shown.truncated);
}

} // namespace

void BeginPanels(const char* id, std::span<float> sizes, const PanelsOptions& options)
{
    internal::PushScope("BeginPanels");
    PanelsStack& stack = PanelViews();
    if (stack.depth >= internal::kScopeStackDepth)
    {
        ++stack.overflow;
        return;
    }
    PanelsEntry& e = stack.entries[stack.depth++];
    e = PanelsEntry{};
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return;
    if (id == nullptr)
        id = "";
    const Theme& theme = T();
    e.active = true;
    e.window = window;
    e.id = window->GetID(id);
    e.sizes = sizes;
    e.count = std::min(static_cast<int>(sizes.size()), kPanelsMax);
    const internal::NextItemData next = internal::TakeNextItemData();
    const ImVec2 available = ImGui::GetContentRegionAvail();
    e.size.x = internal::ResolveItemWidth(options.width, next, kFill);
    e.size.y = options.height > 0.0f ? Px(options.height) : (available.y >= 1.0f ? available.y : Px(theme.metrics.panelHeight));
    e.vertical = options.orientation == Orientation::Vertical || WidthClassOf(e.size.x / Scale()) == WidthClass::Compact;
    e.band = Px(theme.layout.dividerBand);
    e.strip = Px(theme.layout.panelHeaderHeight);

    // Lengths: collapsed strips first, the expanded panels share the rest by their sizes.
    const PanelsMemory& memory = internal::GetState<PanelsMemory>(e.id);
    const float total = e.vertical ? e.size.y : e.size.x;
    float room = std::max(0.0f, total - e.band * static_cast<float>(std::max(0, e.count - 1)));
    float weights = 0.0f;
    int expanded = 0;
    for (int i = 0; i < e.count; ++i)
    {
        e.amount[i] = Tween(e.id, kPanelsCollapseSub + i, memory.collapsed[i] ? 1.0f : 0.0f, theme.motion.durationStandard);
        room -= e.strip * e.amount[i];
        weights += PanelShare(sizes, i) * (1.0f - e.amount[i]);
        expanded += e.amount[i] < 1.0f ? 1 : 0;
    }
    room = std::max(0.0f, room);
    float at = 0.0f;
    for (int i = 0; i < e.count; ++i)
    {
        const float open = 1.0f - e.amount[i];
        const float share = weights > 0.0f ? PanelShare(sizes, i) * open / weights
                                           : (expanded > 0 ? open / static_cast<float>(expanded) : 0.0f);
        e.start[i] = at;
        e.length[i] = std::max(0.0f, e.strip * e.amount[i] + room * share);
        at += e.length[i] + e.band;
    }
    ImGui::BeginGroup();
    e.origin = window->DC.CursorPos;
    ImGui::PushOverrideID(e.id);
    for (internal::LayoutDividerRecord& record : Dividers().current)
        record = {};
}

bool BeginPanel(const char* label, const PanelOptions& options)
{
    PanelsEntry* e = TopPanels();
    const char* innermost = internal::InnermostScope();
    if (e == nullptr || innermost == nullptr || std::strcmp(innermost, "BeginPanels") != 0)
    {
        if (internal::ControlWindow() != nullptr)
            internal::ReportUsageError("BeginPanel outside BeginPanels (or inside a container nested in one)");
        return false;
    }
    if (GImGui->CurrentWindow != e->window)
        return false;
    if (e->index >= e->count)
    {
        internal::ReportUsageError("BeginPanel called more often than BeginPanels has sizes");
        return false;
    }
    if (label == nullptr)
        label = "";
    const int i = e->index++;
    if (i > 0)
        RunPanelDivider(*e, i);

    const Theme& theme = T();
    ImGuiWindow* window = e->window;
    const ImGuiID panelId = window->GetID(label);
    const internal::NextItemData next = internal::TakeNextItemData();
    bool open = options.collapsible ? internal::ContainerOpenState(panelId, true, next) : true;
    const ImRect rect = PanelRect(*e, i);
    const bool disabled = (GImGui->CurrentItemFlags & ImGuiItemFlags_Disabled) != 0;
    const char* labelEnd = ImGui::FindRenderedTextEnd(label);

    // The header row, or the strip of a collapsed panel side by side.
    const ImRect header(rect.Min, ImVec2(rect.Max.x, std::min(rect.Max.y, rect.Min.y + e->strip)));
    const bool strip = !e->vertical && !open;
    ImGui::SetCursorScreenPos(rect.Min);
    if (strip)
    {
        // The button's name and tooltip are the title; a panel without one is named by Strings::expand.
        char fallback[64];
        const char* name = label;
        if (labelEnd == label)
        {
            std::snprintf(fallback, sizeof(fallback), "%s###%s", GetStrings().expand, label);
            name = fallback;
        }
        if (IconButton(name, Icon("chevron-right")))
        {
            internal::SetContainerOpenState(panelId, true);
            open = true;
        }
    }
    else if (options.collapsible)
    {
        internal::ContainerHeaderDesc desc;
        desc.label = label;
        desc.labelEnd = labelEnd;
        desc.icon = options.icon;
        desc.style = TextStyle::Headline;
        desc.open = open;
        desc.width = std::max(1.0f, header.GetWidth());
        desc.height = header.GetHeight();
        if (internal::ContainerHeader(panelId, desc).pressed)
        {
            open = !open;
            internal::SetContainerOpenState(panelId, open);
        }
    }
    else
    {
        ImGui::Dummy(header.GetSize());
        DrawPanelTitle(header, label, labelEnd, options.icon, disabled, panelId);
    }
    e->next.collapsed[i] = !open;
    if (!open || e->amount[i] >= 1.0f)
        return false;
    // The header (or strip) stays the last item after BeginPanel, as after any widget call.
    const ImGuiLastItemData headerItem = GImGui->LastItemData;

    // The hairline under the header, then the content under it.
    HairlineH(window->DrawList, header.Min.x, header.Max.x, std::round(header.Max.y - HairlineThickness()),
              ToU32(theme.colors.separator));
    const ImRect content(ImVec2(rect.Min.x, header.Max.y), rect.Max);
    if (content.GetWidth() < 1.0f || content.GetHeight() < 1.0f)
        return false;
    ImGui::SetCursorScreenPos(content.Min);
    internal::ScrollAreaDesc desc;
    desc.size = content.GetSize();
    if (!internal::BeginScrollAreaCore(ImHashStr("##content", 0, panelId), desc))
        return false;
    GImGui->LastItemData = headerItem;
    e->panelOpen = true;
    internal::PushScope("BeginPanel");
    return true;
}

void EndPanel()
{
    if (!internal::PopScope("BeginPanel"))
        return;
    PanelsEntry* e = TopPanels();
    if (e == nullptr || !e->panelOpen)
        return;
    internal::EndScrollAreaCore();
    e->panelOpen = false;
}

void EndPanels()
{
    if (!internal::PopScope("BeginPanels"))
        return;
    PanelsStack& stack = PanelViews();
    if (stack.overflow > 0)
    {
        --stack.overflow;
        return;
    }
    if (stack.depth == 0)
        return;
    PanelsEntry& e = stack.entries[--stack.depth];
    if (!e.active || GImGui->CurrentWindow != e.window)
        return;  // the caller ended a window inside the panels; Dear ImGui reports that itself
    ImGui::PopID();
    // Panels that were not submitted this frame keep the state they had.
    PanelsMemory& memory = internal::GetState<PanelsMemory>(e.id);
    for (int i = 0; i < e.index; ++i)
        memory.collapsed[i] = e.next.collapsed[i];
    ImGui::SetCursorScreenPos(e.origin);
    ImGui::Dummy(e.size);
    ImGui::EndGroup();
    PanelsDividers& dividers = Dividers();
    std::copy(std::begin(dividers.current), std::end(dividers.current), std::begin(dividers.last));
}

namespace internal
{

LayoutDividerRecord LastPanelsDivider(int index)
{
    if (index < 1 || index >= kPanelsMax)
        return {};
    return Dividers().last[index];
}

void ClearPanelsStack()
{
    PanelsStack& stack = PanelViews();
    stack.depth = 0;
    stack.overflow = 0;
}

} // namespace internal

} // namespace facet
