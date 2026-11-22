// UserChip (facet/widgets/composites.h): the signed-in account as a pill with its avatar, name and
// a description line. One activatable item. The avatar is drawn with the avatar look of the media
// family (initials while there is no ready image, a skeleton while it loads) and the status dot of
// the display family, so it matches every Avatar.

#include "facet/widgets/composites.h"

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
#include "widgets/display/display_look.h"
#include "widgets/media/media_look.h"

namespace facet
{
namespace
{

// The status dot of the chip's avatar: this share of the avatar diameter, never smaller than the
// status dot token, as on Avatar.
constexpr float kUserChipStatusShare = 0.28f;
// The ring around the dot, in hairlines, in the window surface.
constexpr float kUserChipStatusRingHairlines = 2.0f;

TextStyle UserChipDetailStyle(ControlSize size)
{
    return ResolveControlSize(size) == ControlSize::Small ? TextStyle::Caption : TextStyle::Footnote;
}

} // namespace

bool UserChip(const char* label, ImageHandle avatar, const UserChipOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const internal::NextItemData next = internal::TakeNextItemData();
    const Theme& t = T();
    const Colors& c = t.colors;
    const internal::ControlMetrics m = internal::GetControlMetrics(options.size);
    const ImGuiID id = window->GetID(label);
    const char* end = ImGui::FindRenderedTextEnd(label);
    const bool hasDetail = !options.compact && options.description != nullptr && options.description[0] != '\0';
    const TextStyle detailStyle = UserChipDetailStyle(options.size);
    const float inset = PxSnapped(t.spacing.xs);

    // Measure: one line of the name, or the name over the description.
    float nameLine = 0.0f;
    float nameWidth = 0.0f;
    {
        const internal::ControlTextScope text(m);
        nameLine = std::ceil(ImGui::GetTextLineHeight());
        nameWidth = std::ceil(ImGui::CalcTextSize(label, end).x);
    }
    float detailLine = 0.0f;
    float detailWidth = 0.0f;
    if (hasDetail)
    {
        const internal::ControlTextScope text(detailStyle);
        detailLine = std::ceil(ImGui::GetTextLineHeight());
        detailWidth = std::ceil(ImGui::CalcTextSize(options.description).x);
    }
    const float height = hasDetail ? std::max(m.height, nameLine + detailLine + inset * 2.0f) : m.height;
    const float diameter = std::max(2.0f, std::round((height - inset * 2.0f) * 0.5f) * 2.0f);
    const float content = options.compact ? height
                                          : inset + diameter + m.iconGap + std::max(nameWidth, detailWidth) + m.paddingX;
    const float width = options.compact ? height : internal::ResolveItemWidth(0.0f, next, content);

    internal::ApplyNextItemData(next);
    const internal::ItemState item = internal::ItemBehavior(id, ImVec2(width, height));
    const float radius = height * 0.5f;
    bool cut = false;
    if (item.visible)
    {
        ImDrawList* drawList = window->DrawList;
        const ImRect& rect = item.rect;
        if (!options.compact)
            FillRoundRect(drawList, rect.Min, rect.Max, ToU32(internal::NeutralFill(item)), radius);
        const ImVec2 at(std::round(rect.Min.x + (options.compact ? 0.0f : inset)),
                        std::round(rect.GetCenter().y - (options.compact ? height : diameter) * 0.5f));
        const float side = options.compact ? height : diameter;
        const ImRect box(at, ImVec2(at.x + side, at.y + side));
        internal::MediaAvatarLook look;
        look.disabled = item.disabled;
        internal::MediaDrawAvatar(drawList, box, internal::MediaVisibleText(label).text, avatar, look);
        if (options.compact)
        {
            const ImVec4 wash = internal::StateFill(WithAlpha(c.fill[2], 0.0f), c.fill[2], c.fill[1], item);
            if (!item.disabled && wash.w > 0.0f)
                FillRoundRect(drawList, box.Min, box.Max, ToU32(wash), side * 0.5f);
        }
        if (options.showStatus)
        {
            const float dot = std::max(PxSnapped(t.metrics.statusDotSize), std::round(side * kUserChipStatusShare));
            const float ring = std::max(1.0f, std::round(HairlineThickness() * kUserChipStatusRingHairlines));
            const float offset = side * 0.5f * (1.0f - std::sqrt(0.5f));
            const float along = std::min(std::round(side - offset), side - dot * 0.5f);
            const ImVec2 center(box.Min.x + along, box.Min.y + along);
            drawList->AddCircleFilled(center, dot * 0.5f + ring, ToU32(c.bg));
            internal::DrawDisplayDot(drawList, center, dot, ToU32(internal::DisplayToneLook(options.status, item.disabled).glyph),
                                     false);
        }
        if (!options.compact)
        {
            const float x = box.Max.x + m.iconGap;
            const float room = std::max(0.0f, rect.Max.x - m.paddingX - x);
            const float block = nameLine + detailLine;
            float y = std::round(rect.GetCenter().y - block * 0.5f);
            {
                const internal::ControlTextScope text(m);
                const internal::FittedText shown = internal::FitText(label, end, room);
                cut = shown.truncated;
                drawList->AddText(ImVec2(x, y), ToU32(internal::ControlLabelColor(item.disabled)), shown.text);
            }
            if (hasDetail)
            {
                y += nameLine;
                const internal::ControlTextScope text(detailStyle);
                const internal::FittedText shown = internal::FitText(options.description, nullptr, room);
                cut = cut || shown.truncated;
                drawList->AddText(ImVec2(x, y), ToU32(internal::ControlSecondaryColor(item.disabled)), shown.text);
            }
        }
        internal::DrawFocusRing(drawList, rect, radius, item.focusAmount, 0, internal::RingShape::Round);
    }

    // The name, and the description, when the chip shows only the avatar or cut them.
    if ((options.compact || cut) && end != label &&
        internal::BeginPopupTooltip(id, item.rect, item.hovered, item.focusVisible))
    {
        Text(TextStyle::Footnote, "%.*s", static_cast<int>(end - label), label);
        if (options.description != nullptr && options.description[0] != '\0')
            StyledText(options.description, {.style = TextStyle::Footnote, .secondary = true});
        internal::EndPopupTooltip();
    }
    return item.pressed && !item.disabled;
}

} // namespace facet
