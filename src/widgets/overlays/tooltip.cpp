// ItemTooltip, BeginRichTooltip and EndRichTooltip (overlays.h): themed tooltips for the last item,
// as Tooltip levels of the popup host (popup_host.h, BeginPopupTooltip). The host owns the timing
// (overlay.tooltipDelay, the grace period that lets neighbours show theirs at once, a press or a key
// hiding it), the placement below and after the pointer or below a focused item, the surface and the
// motion. This file finds the last item, decides whether it may show a tooltip and draws the text.
//
// A tooltip shows while the item is hovered or has visible keyboard focus (the focus ring shows),
// never for a disabled item and never while another item is active or a popup above blocks the item
// (ImGui::IsItemHovered without flags). It takes no focus and no input; a rich tooltip with
// TooltipOptions::interactive takes the mouse only.

#include "facet/widgets/overlays.h"

#include <cstdarg>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/popup_host.h"
#include "internal/scope_stack.h"

namespace facet
{
namespace
{

// Formatted tooltip text is cut at this many bytes; it is formatted on the stack, never on the heap.
constexpr int kTooltipTextSize = 1024;

// The last item as a tooltip owner: its id (an item without one is named by its rectangle, as
// Dear ImGui names it for its own tooltips), its rectangle and when its tooltip may show.
struct OverlayTooltipTarget
{
    ImGuiID id = 0;
    ImRect rect;
    bool hovered = false;
    bool focusVisible = false;
};

bool LastItemTooltipTarget(OverlayTooltipTarget* out)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    const ImGuiContext& g = *GImGui;
    const ImGuiLastItemData& last = g.LastItemData;
    if (last.ID == 0 && last.Rect.GetWidth() <= 0.0f && last.Rect.GetHeight() <= 0.0f)
        return false;
    out->id = last.ID != 0 ? last.ID : window->GetIDFromRectangle(last.Rect);
    out->rect = last.Rect;
    const bool disabled = (last.ItemFlags & ImGuiItemFlags_Disabled) != 0;
    out->hovered = !disabled && ImGui::IsItemHovered(ImGuiHoveredFlags_None);
    out->focusVisible = !disabled && last.ID != 0 && g.NavId == last.ID && g.NavWindow == window &&
                        internal::KeyboardModality();
    return true;
}

// Captures: a page asks for the tooltip with ForcedItemState::Hovered, Focused or Open right before
// the tooltip call (after its item); it shows below the item for that frame, without input or delay.
bool BeginForcedTooltip(const OverlayTooltipTarget& target, bool* forced)
{
    using namespace internal;
    const ForcedItemRequest request = TakeForcedItemState();
    *forced = Has(request.state, ForcedItemState::Hovered) || Has(request.state, ForcedItemState::Focused) ||
              Has(request.state, ForcedItemState::Open);
    if (!*forced)
        return false;
    PopupOptions options;
    options.kind = PopupKind::Tooltip;
    options.takeFocus = false;
    const ImGuiID level = PopupTooltipLevel(target.id);
    ForcePopupLevelOpen(level, {.rect = target.rect, .side = PopupSide::Below}, options);
    return BeginPopupLevel(level);
}

} // namespace

void ItemTooltip(const char* fmt, ...)
{
    OverlayTooltipTarget target;
    if (fmt == nullptr || !LastItemTooltipTarget(&target))
        return;
    bool forced = false;
    const bool shown = BeginForcedTooltip(target, &forced);
    // Ask the host every frame (also while the item is not hovered), so it can close the tooltip.
    if (!forced && !internal::BeginPopupTooltip(target.id, target.rect, target.hovered, target.focusVisible))
        return;
    if (forced && !shown)
        return;
    char text[kTooltipTextSize];
    va_list args;
    va_start(args, fmt);
    ImFormatStringV(text, sizeof(text), fmt, args);
    va_end(args);
    PushTextStyle(TextStyle::Footnote);
    ImGui::PushStyleColor(ImGuiCol_Text, T().colors.label);
    ImGui::TextUnformatted(text);  // wraps at the host's wrap position (overlay.tooltipMaxWidth)
    ImGui::PopStyleColor();
    PopTextStyle();
    internal::EndPopupTooltip();
}

bool BeginRichTooltip(const TooltipOptions& options)
{
    OverlayTooltipTarget target;
    if (!LastItemTooltipTarget(&target))
        return false;
    bool forced = false;
    const bool shown = BeginForcedTooltip(target, &forced);
    if (forced && !shown)
        return false;
    internal::PopupTooltipDesc desc;
    desc.maxWidth = options.maxWidth > 0.0f ? Px(options.maxWidth) : 0.0f;
    desc.interactive = options.interactive;
    if (!forced && !internal::BeginPopupTooltip(target.id, target.rect, target.hovered, target.focusVisible, desc))
        return false;
    internal::PushScope("BeginRichTooltip");
    return true;
}

void EndRichTooltip()
{
    if (!internal::PopScope("BeginRichTooltip"))
        return;
    internal::EndPopupTooltip();
}

} // namespace facet
