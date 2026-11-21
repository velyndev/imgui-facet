// BeginInspectorPane, InspectorPaneNext and EndInspectorPane (containers.h): a region with a
// details pane at its trailing edge.
//
// The region is as wide as the available width and `height` tall (the available height). The main
// content is a child window over the region less the pane, the pane a child window at the trailing
// edge, so each scrolls on its own. The pane slides in and out (its shown width animates over
// motion.durationStandard, at once with reduced motion); its content keeps the pane's full width
// and is clipped while it slides. In a Compact width (core/layout.h) the main content keeps the
// whole region and the pane floats over it as a drawer, on the elevated surface with its shadow.
// The pane has a title row (the visible label part, the close button) and a resize band along its
// leading edge. Its width is kept in design units under the id and saved in the ini file.

#include "facet/widgets/containers.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/anim.h"
#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/effects.h"
#include "facet/core/layout.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/popup_host.h"
#include "internal/scope_stack.h"
#include "internal/state_storage.h"
#include "widgets/buttons/button_look.h"
#include "widgets/containers/container_layout.h"

namespace facet
{
namespace
{

constexpr int kPaneSlideSub = internal::kItemAnimSubBase - 0x300;
constexpr int kPaneDepth = 4;
constexpr int kPaneTitleSize = 96;
// The drawer of a Compact width floats at this elevation over the main content.
constexpr int kDrawerElevation = 2;

struct InspectorState
{
    static constexpr const char* kStateName = "InspectorPane";
    float width = 0.0f;  // units
    bool known = false;
    void WriteIni(ImGuiTextBuffer& out) const
    {
        if (known)
            out.appendf("width=%.2f\n", width);
    }
    void ReadIni(const char* key, const char* value)
    {
        if (std::strcmp(key, "width") != 0)
            return;
        const float w = std::strtof(value, nullptr);
        if (std::isfinite(w) && w > 0.0f)
        {
            width = w;
            known = true;
        }
    }
};

struct InspectorDrag
{
    bool active = false;
    ImGuiID item = 0;
    float startMouse = 0.0f;
    float startWidth = 0.0f;  // pixels
};

struct PaneEntry
{
    bool active = false;
    ImGuiWindow* outer = nullptr;
    ImGuiWindow* main = nullptr;
    ImGuiID id = 0;
    ImRect region;
    float paneWidth = 0.0f;   // pixels, the pane's full width
    float amount = 0.0f;      // 0 hidden to 1 shown
    bool compact = false;
    bool resizable = true;
    bool* visible = nullptr;
    bool nextCalled = false;
    bool paneOpen = false;
    char title[kPaneTitleSize] = {};
};

struct PaneStack
{
    PaneEntry entries[kPaneDepth];
    int depth = 0;
    int overflow = 0;
};

PaneStack& Panes()
{
    static PaneStack stack;
    return stack;
}

// The close button of the title row: a square plain button with the close icon.
bool PaneCloseButton(ImGuiWindow* window, const ImRect& rect)
{
    const internal::ControlMetrics m = internal::GetControlMetrics(ControlSize::Small);
    const internal::ItemState item = internal::PartBehavior(window->GetID("##close"), rect);
    if (item.visible)
    {
        const internal::ButtonColors colors = internal::ButtonLook(ButtonVariant::Plain, item);
        if (colors.fill.w > 0.0f)
            FillRect(window->DrawList, rect.Min, rect.Max, ToU32(colors.fill), m.radius);
        const internal::ControlTextScope text(m);
        internal::DrawControlIcon(window->DrawList, Icon("close"), std::round(rect.GetCenter().x - text.IconSize() * 0.5f),
                                  rect.GetCenter().y, text.IconSize(), ToU32(colors.content));
        internal::DrawItemFocusRing(item, m.radius);
    }
    if (internal::BeginPopupTooltip(item.id, item.rect, item.hovered, item.focusVisible))
    {
        ImGui::TextUnformatted(GetStrings().close);
        internal::EndPopupTooltip();
    }
    return item.pressed;
}

} // namespace

void BeginInspectorPane(const char* label, bool* visible, const InspectorPaneOptions& options)
{
    internal::PushScope("BeginInspectorPane");
    PaneStack& stack = Panes();
    if (stack.depth >= kPaneDepth)
    {
        ++stack.overflow;
        return;
    }
    PaneEntry& entry = stack.entries[stack.depth++];
    entry = PaneEntry{};
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return;
    if (label == nullptr)
        label = "";
    ImGuiContext& g = *GImGui;
    const Theme& t = T();
    const internal::NextItemData next = internal::TakeNextItemData();
    const float scale = Px(1.0f);
    const float width = internal::ResolveItemWidth(0.0f, next, kFill);
    const float available = ImGui::GetContentRegionAvail().y;
    const float height = options.height > 0.0f ? Px(options.height)
                                               : (available > 1.0f ? available : Px(t.metrics.panelHeight));
    const ImVec2 origin = window->DC.CursorPos;

    entry.active = true;
    entry.outer = window;
    entry.id = window->GetID(label);
    entry.region = ImRect(origin, ImVec2(origin.x + width, origin.y + height));
    entry.compact = WidthClassOf(width / scale) == WidthClass::Compact;
    entry.resizable = options.resizable;
    entry.visible = visible;
    const char* labelEnd = ImGui::FindRenderedTextEnd(label);
    const std::size_t length =
        std::min(static_cast<std::size_t>(labelEnd - label), static_cast<std::size_t>(kPaneTitleSize - 1));
    std::memcpy(entry.title, label, length);
    entry.title[length] = '\0';

    // The pane width: saved units, a drag in progress, then the limits.
    InspectorState& state = internal::GetPersistentState<InspectorState>(entry.id);
    InspectorDrag& drag = internal::GetState<InspectorDrag>(entry.id);
    const float minWidth = Px(options.minPaneWidth > 0.0f ? options.minPaneWidth : t.containers.inspectorMinWidth);
    float maxWidth = options.maxPaneWidth > 0.0f ? Px(options.maxPaneWidth) : width * 0.5f;
    // A drawer may grow over the main content, but leaves a strip of spacing.xxxl of it in view,
    // so the content behind stays visible and within reach of the pointer.
    if (entry.compact)
        maxWidth = std::max(maxWidth, width - Px(t.spacing.xxxl));
    maxWidth = std::min(maxWidth, entry.compact ? width - Px(t.spacing.xxxl) : width);
    const float lower = std::min(minWidth, maxWidth);
    float paneWidth = state.known ? state.width * scale
                                  : Px(options.paneWidth > 0.0f ? options.paneWidth : t.containers.inspectorWidth);
    if (drag.active && (g.ActiveId != drag.item || !ImGui::IsMouseDown(ImGuiMouseButton_Left)))
        drag.active = false;
    if (drag.active)
        paneWidth = drag.startWidth - (g.IO.MousePos.x - drag.startMouse);
    paneWidth = std::floor(std::clamp(paneWidth, lower, std::max(lower, maxWidth)));
    entry.paneWidth = paneWidth;
    if (drag.active || !state.known)
    {
        const float units = paneWidth / scale;
        if (!state.known || std::fabs(units - state.width) > 0.01f)
        {
            state.width = units;
            state.known = true;
            internal::MarkStateDirty();
        }
    }

    const bool open = visible == nullptr || *visible;
    entry.amount = Tween(entry.id, kPaneSlideSub, open ? 1.0f : 0.0f, t.motion.durationStandard, Ease::OutCubic);
    const float shown = entry.compact ? 0.0f : std::floor(paneWidth * entry.amount);

    // The main content.
    ImGui::SetCursorScreenPos(origin);
    ImGui::BeginChild(ImHashStr("##main", 0, entry.id), ImVec2(std::max(1.0f, width - shown), height), ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoBackground);
    entry.main = g.CurrentWindow;
}

bool InspectorPaneNext()
{
    PaneStack& stack = Panes();
    if (stack.overflow > 0 || stack.depth == 0)
        return false;
    PaneEntry& entry = stack.entries[stack.depth - 1];
    if (!entry.active || entry.nextCalled)
        return false;
    ImGuiContext& g = *GImGui;
    if (g.CurrentWindow != entry.main)
        return false;  // a window was left open in the main content; Dear ImGui reports that itself
    entry.nextCalled = true;
    ImGui::EndChild();
    if (entry.amount <= 0.0f)
        return false;

    const Theme& t = T();
    const float slide = std::floor(entry.paneWidth * entry.amount);
    const ImRect pane(ImVec2(entry.region.Max.x - slide, entry.region.Min.y),
                      ImVec2(entry.region.Max.x - slide + entry.paneWidth, entry.region.Max.y));
    const float pad = Px(t.spacing.lg);
    ImGui::SetCursorScreenPos(pane.Min);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(pad, pad));
    ImGui::BeginChild(ImHashStr("##pane", 0, entry.id), pane.GetSize(), ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_NoBackground);
    ImGui::PopStyleVar();
    entry.paneOpen = true;
    ImGuiWindow* window = g.CurrentWindow;
    ImDrawList* drawList = window->DrawList;

    // The surface: bgSecondary beside the main content, the elevated drawer over it when Compact.
    ImGui::PushClipRect(entry.outer->ClipRect.Min, entry.outer->ClipRect.Max, false);
    if (entry.compact)
    {
        DrawElevation(drawList, pane.Min, pane.Max, 0.0f, kDrawerElevation);
        FillRect(drawList, pane.Min, pane.Max, ToU32(t.colors.ElevatedSurface(kDrawerElevation)), 0.0f);
    }
    else
    {
        FillRect(drawList, pane.Min, pane.Max, ToU32(t.colors.bgSecondary), 0.0f);
    }
    ImGui::PopClipRect();
    const float thickness = HairlineThickness();
    FillRect(drawList, pane.Min, ImVec2(pane.Min.x + thickness, pane.Max.y), ToU32(t.colors.separator), 0.0f);

    // The resize band along the leading edge.
    if (entry.resizable)
    {
        const float band = Px(t.containers.resizeBorder);
        internal::ItemOptions options;
        options.press = internal::PressMode::Click;
        options.navigable = false;
        options.tabStop = false;
        options.hitTarget = false;
        options.handCursor = false;
        const ImRect handle(pane.Min, ImVec2(pane.Min.x + band, pane.Max.y));
        const internal::ItemState item = internal::PartBehavior(window->GetID("##resize"), handle, options);
        InspectorDrag& drag = internal::GetState<InspectorDrag>(entry.id);
        if (item.hovered || item.active)
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            FillRect(drawList, pane.Min, ImVec2(pane.Min.x + thickness * 2.0f, pane.Max.y), ToU32(t.colors.focusRing), 0.0f);
        }
        if (item.activated)
        {
            drag.active = true;
            drag.item = item.id;
            drag.startMouse = g.IO.MousePos.x;
            drag.startWidth = entry.paneWidth;
        }
    }

    // The title row: the title in Headline and the close button.
    const internal::ControlTextScope text(TextStyle::Headline);
    const float side = ControlHeight(ControlSize::Small);
    const ImVec2 rowMin = window->DC.CursorPos;
    const float rowRight = pane.Max.x - pad;
    float titleRight = rowRight;
    if (entry.visible != nullptr)
    {
        const ImRect close(ImVec2(rowRight - side, rowMin.y), ImVec2(rowRight, rowMin.y + side));
        if (PaneCloseButton(window, close))
            *entry.visible = false;
        titleRight = close.Min.x - Px(t.spacing.sm);
    }
    const internal::FittedText title = internal::FitText(entry.title, nullptr, std::max(0.0f, titleRight - rowMin.x));
    drawList->AddText(ImVec2(rowMin.x, CenteredTextY(rowMin.y, side)), ToU32(t.colors.label), title.text);
    const ImRect titleRect(rowMin, ImVec2(rowMin.x + title.width, rowMin.y + side));
    const bool hovered = g.HoveredWindow == window && ImGui::IsMouseHoveringRect(titleRect.Min, titleRect.Max);
    internal::CutTextTooltip(window->GetID("##title"), titleRect, hovered, false, entry.title, nullptr, title.truncated);
    ImGui::Dummy(ImVec2(std::max(1.0f, rowRight - rowMin.x), side));
    return true;
}

void EndInspectorPane()
{
    if (!internal::PopScope("BeginInspectorPane"))
        return;
    PaneStack& stack = Panes();
    if (stack.overflow > 0)
    {
        --stack.overflow;
        return;
    }
    if (stack.depth == 0)
        return;
    PaneEntry entry = stack.entries[--stack.depth];
    if (!entry.active)
        return;
    ImGuiContext& g = *GImGui;
    if (!entry.nextCalled && g.CurrentWindow == entry.main)
        ImGui::EndChild();
    else if (entry.paneOpen && g.CurrentWindow != nullptr && g.CurrentWindow != entry.outer &&
             g.CurrentWindow->ParentWindow == entry.outer)
        ImGui::EndChild();
    if (g.CurrentWindow != entry.outer)
        return;  // a window was left open inside the region; Dear ImGui reports that itself
    // The region is one item of its whole size.
    ImGui::SetCursorScreenPos(entry.region.Min);
    ImGui::Dummy(entry.region.GetSize());
}

namespace internal
{

void ClearInspectorPaneStack()
{
    PaneStack& stack = Panes();
    stack.depth = 0;
    stack.overflow = 0;
}

} // namespace internal

} // namespace facet
