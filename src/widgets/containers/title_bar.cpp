// BeginTitleBar, EndTitleBar and TitleBarButtonPressed (containers.h): a title row.
//
// Layout from the leading edge: spacing.md, the icon, the title in Headline, the detail in
// Footnote and labelSecondary; from the trailing edge: the window buttons (square plain buttons
// of the control height), then the caller's items, placed so they end before the buttons with
// their width of the last frame (kept under the id). The title and the detail are cut with an
// ellipsis where the items begin; the detail gives way first. The bar's free area is the caption
// the app frame reports to the host (container_layout.h keeps the regions of each frame).

#include "facet/widgets/containers.h"

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

constexpr int kTitleBarDepth = 4;

struct TitleBarState
{
    float itemsWidth = 0.0f;  // the caller's items in the last frame, pixels
};

struct TitleBarEntry
{
    bool active = false;
    ImGuiWindow* window = nullptr;
    ImGuiID id = 0;
    ImRect bar;
    float itemsRight = 0.0f;  // where the caller's items end
    float itemsLeft = 0.0f;   // where they start, from the last frame's width
    bool windowButtons = false;
    bool maximized = false;
};

struct TitleBarStack
{
    TitleBarEntry entries[kTitleBarDepth];
    int depth = 0;
    int overflow = 0;
};

TitleBarStack& TitleBars()
{
    static TitleBarStack stack;
    return stack;
}

struct TitleBarFrames
{
    internal::TitleBarRegions current[internal::kTitleBarRegionCount];
    int currentCount = 0;
    internal::TitleBarRegions previous[internal::kTitleBarRegionCount];
    int previousCount = 0;
    WindowButton pressed = WindowButton::None;
};

TitleBarFrames& TitleBarRecords()
{
    static TitleBarFrames frames;
    return frames;
}

// The width of the three window buttons and the gaps between them, pixels.
float WindowButtonsWidth(const internal::ControlMetrics& m, float gap)
{
    return m.height * 3.0f + gap * 2.0f;
}

// One window button. Close turns to the destructive fill on hover and press, because it ends the
// app or window; the other two use the plain button look.
bool WindowButtonItem(ImGuiWindow* window, ImGuiID id, const ImRect& rect, Icon icon, const char* name, bool destructive,
                      const internal::ControlMetrics& m)
{
    const internal::ItemState item = internal::PartBehavior(id, rect);
    if (item.visible)
    {
        internal::ButtonColors colors = internal::ButtonLook(ButtonVariant::Plain, item);
        const float amount = std::max(item.hoverAmount, item.pressAmount);
        if (destructive && !item.disabled && amount > 0.0f)
        {
            const Colors& c = T().colors;
            colors.fill = WithAlpha(Mix(c.destructive, c.destructivePressed, item.pressAmount), amount);
            colors.content = Mix(colors.content, c.onDestructive, amount);
        }
        if (colors.fill.w > 0.0f)
            FillRect(window->DrawList, rect.Min, rect.Max, ToU32(colors.fill), m.radius);
        const internal::ControlTextScope text(m);
        internal::DrawControlIcon(window->DrawList, icon, std::round(rect.GetCenter().x - text.IconSize() * 0.5f),
                                  rect.GetCenter().y, text.IconSize(), ToU32(colors.content));
        internal::DrawItemFocusRing(item, m.radius);
    }
    if (internal::BeginPopupTooltip(item.id, item.rect, item.hovered, item.focusVisible))
    {
        ImGui::TextUnformatted(name);
        internal::EndPopupTooltip();
    }
    return item.pressed;
}

} // namespace

void BeginTitleBar(const char* label, const TitleBarOptions& options)
{
    internal::PushScope("BeginTitleBar");
    TitleBarStack& stack = TitleBars();
    if (stack.depth >= kTitleBarDepth)
    {
        ++stack.overflow;
        return;
    }
    TitleBarEntry& entry = stack.entries[stack.depth++];
    entry.active = false;
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return;
    if (label == nullptr)
        label = "";
    const Theme& t = T();
    const internal::NextItemData next = internal::TakeNextItemData();
    const float width = internal::ResolveItemWidth(options.width, next, kFill);
    const float height = options.height > 0.0f ? Px(options.height) : Px(t.containers.titleBarHeight);
    const internal::ControlMetrics m = internal::GetControlMetrics(ControlSize::Inherit);
    const float gap = Px(t.spacing.xs);
    const float pad = Px(t.spacing.md);
    const bool disabled = (next.itemFlags & ImGuiItemFlags_Disabled) != 0;

    entry.active = true;
    entry.window = window;
    entry.id = window->GetID(label);
    const ImVec2 origin = window->DC.CursorPos;
    entry.bar = ImRect(origin, ImVec2(origin.x + width, origin.y + height));
    entry.windowButtons = options.windowButtons;
    entry.maximized = options.maximized;
    const float buttons =
        options.windowButtons ? WindowButtonsWidth(m, gap) + (height - m.height) * 0.5f + Px(t.spacing.sm) : pad;
    entry.itemsRight = entry.bar.Max.x - buttons;
    const TitleBarState& state = internal::GetState<TitleBarState>(entry.id);
    entry.itemsLeft = std::max(entry.bar.Min.x, entry.itemsRight - state.itemsWidth);

    ImGui::BeginGroup();
    // Icon, title and detail, cut where the items begin; the detail gives way first.
    ImDrawList* drawList = window->DrawList;
    const char* labelEnd = ImGui::FindRenderedTextEnd(label);
    const float textRight = entry.itemsLeft - (state.itemsWidth > 0.0f ? Px(t.spacing.sm) : 0.0f);
    float x = entry.bar.Min.x + pad;
    bool cut = false;
    ImRect titleRect;
    if (ImGui::IsRectVisible(entry.bar.Min, entry.bar.Max))
    {
        const internal::ControlTextScope text(TextStyle::Headline);
        const float textY = CenteredTextY(entry.bar.Min.y, height);
        if (options.icon.IsValid())
        {
            internal::DrawControlIcon(drawList, options.icon, x, entry.bar.GetCenter().y, text.IconSize(),
                                      ToU32(internal::ControlSecondaryColor(disabled)));
            x += text.IconSize() + Px(t.spacing.sm);
        }
        const internal::FittedText title = internal::FitText(label, labelEnd, std::max(0.0f, textRight - x));
        cut = title.truncated;
        drawList->AddText(ImVec2(x, textY), ToU32(internal::ControlLabelColor(disabled)), title.text);
        titleRect = ImRect(ImVec2(x, entry.bar.Min.y), ImVec2(x + title.width, entry.bar.Max.y));
        x += title.width;
        if (options.detail != nullptr && options.detail[0] != '\0' && !cut)
        {
            const internal::ControlTextScope footnote(TextStyle::Footnote);
            const float dx = x + Px(t.spacing.sm);
            const internal::FittedText detail = internal::FitText(options.detail, nullptr, std::max(0.0f, textRight - dx));
            drawList->AddText(ImVec2(dx, CenteredTextY(entry.bar.Min.y, height)),
                              ToU32(internal::ControlSecondaryColor(disabled)), detail.text);
        }
    }
    const bool hovered = window == GImGui->HoveredWindow && ImGui::IsMouseHoveringRect(titleRect.Min, titleRect.Max);
    internal::CutTextTooltip(ImHashStr("##title", 0, entry.id), titleRect, hovered, false, label, labelEnd, cut);

    // The caller's items, vertically centered on the bar. The cursor is moved without marking it
    // as set by the caller, so a bar without items ends cleanly.
    window->DC.CursorPos = ImVec2(entry.itemsLeft, std::floor(entry.bar.GetCenter().y - m.height * 0.5f));
    ImGui::BeginGroup();
    // EndGroup grows a group to the last item, so an empty group would take the size of whatever
    // was submitted before the bar: start from an empty last item at the cursor.
    GImGui->LastItemData.Rect = ImRect(window->DC.CursorPos, window->DC.CursorPos);
}

void EndTitleBar()
{
    if (!internal::PopScope("BeginTitleBar"))
        return;
    TitleBarStack& stack = TitleBars();
    if (stack.overflow > 0)
    {
        --stack.overflow;
        return;
    }
    if (stack.depth == 0)
        return;
    TitleBarEntry& entry = stack.entries[--stack.depth];
    if (!entry.active)
        return;
    entry.active = false;
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = entry.window;
    if (g.CurrentWindow != window)
        return;  // the caller ended the window inside the bar; Dear ImGui reports that itself

    // The caller's items: their width for the next frame.
    ImGui::EndGroup();
    const ImRect group = g.LastItemData.Rect;
    const bool hasItems = group.GetWidth() > 0.0f;
    const ImRect items = hasItems ? group : ImRect();
    internal::GetState<TitleBarState>(entry.id).itemsWidth = hasItems ? group.GetWidth() : 0.0f;

    internal::TitleBarRegions regions;
    regions.rootWindow = window->RootWindow != nullptr ? window->RootWindow->ID : window->ID;
    regions.bar = entry.bar;
    regions.items = items;
    if (entry.windowButtons)
    {
        const Strings& strings = GetStrings();
        const internal::ControlMetrics m = internal::GetControlMetrics(ControlSize::Inherit);
        const float gap = Px(T().spacing.xs);
        const float inset = (entry.bar.GetHeight() - m.height) * 0.5f;
        const float top = std::floor(entry.bar.GetCenter().y - m.height * 0.5f);
        float x = entry.bar.Max.x - inset - WindowButtonsWidth(m, gap);
        const Icon icons[3] = {Icon("minimize"), Icon(entry.maximized ? "restore" : "maximize"), Icon("close")};
        const char* names[3] = {strings.minimize, entry.maximized ? strings.restore : strings.maximize, strings.close};
        const char* keys[3] = {"##minimize", "##maximize", "##close"};
        const WindowButton results[3] = {WindowButton::Minimize, WindowButton::Maximize, WindowButton::Close};
        for (int i = 0; i < 3; ++i)
        {
            const ImRect rect(ImVec2(x, top), ImVec2(x + m.height, top + m.height));
            if (WindowButtonItem(window, ImHashStr(keys[i], 0, entry.id), rect, icons[i], names[i], i == 2, m))
                TitleBarRecords().pressed = results[i];
            regions.buttons[i] = rect;
            x += m.height + gap;
        }
    }
    internal::RecordTitleBarRegions(regions);

    // The bar is one item of its whole size.
    window->DC.CursorMaxPos.x = std::max(window->DC.CursorMaxPos.x, entry.bar.Max.x);
    window->DC.CursorMaxPos.y = std::max(window->DC.CursorMaxPos.y, entry.bar.Max.y);
    ImGui::EndGroup();
}

WindowButton TitleBarButtonPressed()
{
    return TitleBarRecords().pressed;
}

namespace internal
{

void RecordTitleBarRegions(const TitleBarRegions& regions)
{
    TitleBarFrames& frames = TitleBarRecords();
    if (frames.currentCount < kTitleBarRegionCount)
        frames.current[frames.currentCount++] = regions;
}

int PreviousTitleBarRegions(const TitleBarRegions** regions)
{
    TitleBarFrames& frames = TitleBarRecords();
    *regions = frames.previous;
    return frames.previousCount;
}

void RotateTitleBarRegions()
{
    TitleBarFrames& frames = TitleBarRecords();
    for (int i = 0; i < frames.currentCount; ++i)
        frames.previous[i] = frames.current[i];
    frames.previousCount = frames.currentCount;
    frames.currentCount = 0;
}

void ClearTitleBarPress()
{
    TitleBarRecords().pressed = WindowButton::None;
}

void ClearTitleBarStack()
{
    TitleBarStack& stack = TitleBars();
    stack.depth = 0;
    stack.overflow = 0;
}

} // namespace internal

} // namespace facet
