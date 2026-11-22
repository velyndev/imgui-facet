// MediaCard (facet/widgets/composites.h): a card with a cover image, a title, a description and
// badges that activates as one item.
//
// It is assembled from finished widgets: a Card surface (containers.h), the cover as an ImageView
// (media.h, so it has the loading and failed states of the media family), the description as wrapped
// StyledText and the badges as Badge items in a Flow (display.h, layout.h). The card adds the layout
// (the cover on top; beside the text when the card is narrower than composites.mediaCardHorizontalBelow),
// the title cut to one line, and one item over the whole card for hover, press, the focus ring, the
// accent ring of `selected` and activation by click, Enter or Space.

#include "facet/widgets/composites.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "facet/widgets/containers.h"
#include "facet/widgets/display.h"
#include "facet/widgets/layout.h"
#include "facet/widgets/media.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/list_view.h"

namespace facet
{
namespace
{

// The cover of a narrow card takes at most this share of its inner width, so the text keeps the rest.
constexpr float kMediaCardCoverMaxShare = 0.5f;

// The title line: Headline, cut with an ellipsis. Returns whether it was cut.
bool MediaCardTitle(const char* label, const char* end, bool disabled)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    const internal::ControlTextScope text(TextStyle::Headline);
    const float width = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    const float height = std::ceil(ImGui::GetTextLineHeight());
    const ImVec2 at = window->DC.CursorPos;
    ImGui::Dummy(ImVec2(width, height));
    if (end == label)
        return false;
    const internal::FittedText shown = internal::FitText(label, end, width);
    window->DrawList->AddText(at, ToU32(internal::ControlLabelColor(disabled)), shown.text);
    return shown.truncated;
}

} // namespace

bool MediaCard(const char* label, ImageHandle cover, const MediaCardOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const internal::NextItemData next = internal::TakeNextItemData();
    const internal::ForcedItemRequest forced = internal::TakeForcedItemState();
    const Theme& t = T();
    const Colors& c = t.colors;
    const ImGuiID cardId = window->GetID(label);
    const char* end = ImGui::FindRenderedTextEnd(label);
    const float available = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    const float width = internal::ResolveItemWidth(options.width, next, std::min(Px(t.metrics.cardWidth), available));
    const bool horizontal = width < Px(t.composites.mediaCardHorizontalBelow);
    const bool disabled = (next.itemFlags & ImGuiItemFlags_Disabled) != 0;

    ImGui::BeginGroup();
    ImGui::PushID(label);
    BeginCard("##card", {.width = width / Scale()});
    bool titleCut = false;
    {
        const float inner = std::max(1.0f, ImGui::GetContentRegionAvail().x);
        const ImageViewOptions coverLook{.fit = options.fit, .corners = Radius::Element};
        if (horizontal)
        {
            // A narrow card: a square cover beside the text.
            const float side = std::min(Px(t.metrics.slotSize), std::floor(inner * kMediaCardCoverMaxShare));
            ImageViewOptions look = coverLook;
            look.width = side / Scale();
            look.height = side / Scale();
            ImageView("##cover", cover, look);
            ImGui::SameLine(0.0f, Px(t.spacing.md));
        }
        else
        {
            ImageViewOptions look = coverLook;
            look.width = inner / Scale();
            look.height = std::round(inner * t.composites.mediaCoverAspect) / Scale();
            ImageView("##cover", cover, look);
        }
        ImGui::BeginGroup();
        titleCut = MediaCardTitle(label, end, disabled);
        if (options.description != nullptr)
            StyledText(options.description, {.style = TextStyle::Footnote, .secondary = true, .wrap = true});
        if (!options.badges.empty())
        {
            BeginFlow("##badges", {.gap = Gap::Tight});
            for (const char* badge : options.badges)
                if (FlowNext())
                    Badge(badge);
            EndFlow();
        }
        ImGui::EndGroup();
    }
    EndCard();
    ImGui::PopID();
    const ImRect frame(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());

    // One item over the whole card.
    internal::ApplyNextItemData(next);
    internal::ItemOptions itemOptions;
    itemOptions.forced = &forced;
    itemOptions.hitTarget = false;
    const internal::ItemState item = internal::PartBehavior(cardId, frame, itemOptions);
    const float radius = Px(t.radii.card);
    if (item.visible)
    {
        ImDrawList* drawList = window->DrawList;
        const ImVec4 wash = internal::StateFill(WithAlpha(c.fill[2], 0.0f), c.fill[2], c.fill[1], item);
        if (!item.disabled && wash.w > 0.0f)
            FillRect(drawList, frame.Min, frame.Max, ToU32(wash), radius);
        // The selected card look of the row rule: accentSubtle over the card and the accent outline
        // at the one border width.
        if (options.selected)
            internal::DrawListRowFill(drawList, frame, radius,
                                      {.selected = true, .disabled = item.disabled, .tile = true});
        internal::DrawItemFocusRing(item, radius);
    }
    ImGui::EndGroup();
    internal::FinishWidgetGroup(cardId);
    internal::CutTextTooltip(item, label, end, titleCut);
    return item.pressed && !item.disabled;
}

} // namespace facet
