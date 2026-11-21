// BeginLoadingOverlay and EndLoadingOverlay (overlays.h): dims and blocks a region while it loads.
//
// The content between the two calls is always laid out, inside one group, so the region keeps its
// size and its place whether it loads or not. While `loading` is set, the content runs inside
// ImGui::BeginDisabled: every item draws its disabled look (Facet widgets their disabled tokens), is
// skipped by Tab and takes no click. Over the region EndLoadingOverlay draws the scrim and a small
// card in the middle with the spinner (or a bar for a known progress) and the message, so the
// message reads at AA contrast whatever the content under it. The scrim and the card fade in and out
// over motion.durationStandard (at once under reduced motion); the blocking starts and ends at once.

#include "facet/widgets/overlays.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "core/core_internal.h"
#include "facet/core/anim.h"
#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/effects.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/scope_stack.h"
#include "widgets/overlays/overlay_parts.h"

namespace facet
{
namespace
{

constexpr int kLoadingDepth = 8;
constexpr int kLoadingMessageSize = 128;
constexpr int kLoadingFadeSub = 1;

struct LoadingEntry
{
    ImGuiID id = 0;
    bool active = false;    // a group was opened (inside a frame and a window)
    bool loading = false;
    float progress = kIndeterminate;
    char message[kLoadingMessageSize] = {};
};

struct LoadingStack
{
    LoadingEntry entries[kLoadingDepth];
    int depth = 0;
    int overflow = 0;
};

LoadingStack& Stack()
{
    static LoadingStack stack;
    return stack;
}

ImU32 Faded(const ImVec4& color, float amount)
{
    return ToU32(ImVec4(color.x, color.y, color.z, color.w * amount));
}

void DrawLoadingCard(ImDrawList* drawList, const ImRect& region, const LoadingEntry& entry, float amount)
{
    using namespace internal;
    const Theme& theme = T();
    const float padding = Px(theme.spacing.md);
    const float gap = Px(theme.spacing.sm);
    PushTextStyle(TextStyle::Footnote);
    const float lineHeight = ImGui::GetFontSize();
    const float maxInner = std::max(1.0f, region.GetWidth() - (padding + gap) * 2.0f);
    const FittedText shown = FitText(entry.message, nullptr, maxInner);
    const bool bar = entry.progress >= 0.0f;
    const float spinner = std::round(lineHeight * 1.5f);
    const float inner = std::min(maxInner, std::max(shown.width, Px(theme.metrics.controlHeight) * 3.0f));
    const float barHeight = Px(theme.metrics.progressHeightSmall);
    const float contentHeight = (bar ? barHeight : spinner) + gap + lineHeight;
    const ImVec2 size(inner + padding * 2.0f, contentHeight + padding * 2.0f);
    const ImVec2 center = region.GetCenter();
    const ImRect card(ImVec2(std::round(center.x - size.x * 0.5f), std::round(center.y - size.y * 0.5f)),
                      ImVec2(std::round(center.x + size.x * 0.5f), std::round(center.y + size.y * 0.5f)));
    const float radius = Px(theme.radii.card);
    drawList->PushClipRect(region.Min, region.Max, true);
    if (amount >= 1.0f)
        DrawElevation(drawList, card.Min, card.Max, radius, 1);
    FillRect(drawList, card.Min, card.Max, Faded(theme.colors.ElevatedSurface(1), amount), radius);
    StrokeRect(drawList, card.Min, card.Max, Faded(theme.colors.separator, amount), radius, HairlineThickness());
    float y = card.Min.y + padding;
    if (bar)
    {
        const ImVec2 trackMin(card.Min.x + padding, y);
        const ImVec2 trackMax(card.Max.x - padding, y + barHeight);
        FillRect(drawList, trackMin, trackMax, Faded(theme.colors.fill[1], amount), barHeight * 0.5f);
        const float share = std::clamp(entry.progress, 0.0f, 1.0f);
        if (share > 0.0f)
            FillRect(drawList, trackMin, ImVec2(trackMin.x + (trackMax.x - trackMin.x) * share, trackMax.y),
                     Faded(theme.colors.accent, amount), barHeight * 0.5f);
        y += barHeight + gap;
    }
    else
    {
        DrawControlSpinner(drawList, ImVec2(center.x, y + spinner * 0.5f), spinner, Faded(theme.colors.labelSecondary, amount));
        y += spinner + gap;
    }
    drawList->AddText(ImVec2(std::round(center.x - shown.width * 0.5f), y), Faded(theme.colors.label, amount), shown.text);
    drawList->PopClipRect();
    PopTextStyle();
}

} // namespace

namespace internal
{

void OverlayLoadingResetStack()
{
    // Facet's own stack only: Dear ImGui's group and disabled stacks of a Begin left open are the
    // host's to repair (docs/api-conventions.md rule 14).
    LoadingStack& stack = Stack();
    stack.depth = 0;
    stack.overflow = 0;
}

} // namespace internal

void BeginLoadingOverlay(const char* id, const LoadingOverlayOptions& options)
{
    using namespace internal;
    PushScope("BeginLoadingOverlay");
    LoadingStack& stack = Stack();
    if (stack.depth >= kLoadingDepth)
    {
        ReportUsageError("BeginLoadingOverlay nested deeper than %d", kLoadingDepth);
        ++stack.overflow;
        return;
    }
    LoadingEntry& entry = stack.entries[stack.depth++];
    entry = LoadingEntry{};
    ImGuiWindow* window = ControlWindow();
    if (window == nullptr)
        return;
    entry.active = true;
    entry.id = window->GetID(id != nullptr ? id : "");
    entry.loading = options.loading;
    entry.progress = options.progress;
    ImStrncpy(entry.message, options.message != nullptr ? options.message : GetStrings().loading, sizeof(entry.message));
    ImGui::BeginGroup();
    if (entry.loading)
        ImGui::BeginDisabled(true);
}

void EndLoadingOverlay()
{
    using namespace internal;
    if (!PopScope("BeginLoadingOverlay"))
        return;
    LoadingStack& stack = Stack();
    if (stack.overflow > 0)
    {
        --stack.overflow;
        return;
    }
    if (stack.depth <= 0)
        return;
    const LoadingEntry entry = stack.entries[--stack.depth];
    if (!entry.active)
        return;
    if (entry.loading)
        ImGui::EndDisabled();
    ImGui::EndGroup();
    const ImRect region(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    const float amount = Tween(entry.id, kLoadingFadeSub, entry.loading ? 1.0f : 0.0f, T().motion.durationStandard);
    if (amount <= 0.0f || region.GetWidth() <= 0.0f || region.GetHeight() <= 0.0f)
        return;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec4 scrim = T().colors.scrim;
    FillRect(drawList, region.Min, region.Max, ToU32(ImVec4(scrim.x, scrim.y, scrim.z, scrim.w * amount)),
             Px(T().radii.control));
    DrawLoadingCard(drawList, region, entry, amount);
}

} // namespace facet
