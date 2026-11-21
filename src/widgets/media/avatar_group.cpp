// AvatarGroup (facet/widgets/media.h): overlapping avatars with a "+N" chip for the members that do
// not show, as one item and one Tab stop. Hovering or focusing it lists every name in a tooltip under
// the visible label part. Keeps no state.

#include "facet/widgets/media.h"

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
#include "internal/text_util.h"
#include "widgets/media/media_look.h"

namespace facet
{
namespace
{

// The ring of surface color around each avatar of the group, in hairlines: it separates an avatar
// from the one it overlaps.
constexpr float kAvatarGroupRingHairlines = 2.0f;

struct AvatarGroupLayout
{
    int shown = 0;           // avatars drawn
    int hidden = 0;          // members counted by the chip
    float step = 0.0f;       // pixels from one avatar to the next
    float chipWidth = 0.0f;  // 0 without a chip
    float width = 0.0f;
    char chip[32] = {};
};

AvatarGroupLayout LayoutAvatarGroup(int count, int maxVisible, float diameter, float available)
{
    AvatarGroupLayout l;
    l.step = std::max(1.0f, diameter - std::round(diameter * T().media.avatarOverlap));
    const auto chipFor = [&](int hidden) {
        const internal::TextToken tokens[] = {{"count", hidden}};
        internal::FormatTokens(l.chip, sizeof(l.chip), GetStrings().moreCount, tokens);
        const internal::ControlMetrics small = internal::GetControlMetrics(ControlSize::Small);
        const internal::ControlTextScope text(small);
        return std::max(diameter, std::ceil(ImGui::CalcTextSize(l.chip).x + small.paddingX * 2.0f));
    };
    const auto widthOf = [&](int shown, int hidden) {
        float w = shown > 0 ? diameter + static_cast<float>(shown - 1) * l.step : 0.0f;
        if (hidden > 0)
            w += (shown > 0 ? l.step - diameter : 0.0f) + chipFor(hidden);
        return w;
    };
    if (count <= 0)
    {
        l.width = diameter;
        return l;
    }
    int shown = maxVisible > 0 ? std::min(count, maxVisible) : count;
    // With maxVisible 0 the group fits the width: avatars give way to the chip until both fit.
    if (maxVisible <= 0)
    {
        while (shown > 1 && widthOf(shown, count - shown) > available)
            --shown;
    }
    l.shown = shown;
    l.hidden = count - shown;
    if (l.hidden > 0)
        l.chipWidth = chipFor(l.hidden);
    l.width = widthOf(l.shown, l.hidden);
    return l;
}

} // namespace

bool AvatarGroup(const char* label, std::span<const AvatarItem> items, const AvatarGroupOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const internal::NextItemData next = internal::TakeNextItemData();
    const ImGuiID id = window->GetID(label);
    const Theme& t = T();
    const Colors& c = t.colors;
    const float diameter = internal::MediaAvatarDiameter(options.diameter, options.size);
    const int count = static_cast<int>(items.size());
    const AvatarGroupLayout layout =
        LayoutAvatarGroup(count, options.maxVisible, diameter, std::max(diameter, ImGui::GetContentRegionAvail().x));

    const ImVec2 origin(std::round(window->DC.CursorPos.x), std::round(window->DC.CursorPos.y));
    window->DC.CursorPos = origin;
    const ImVec2 size(std::round(layout.width), diameter);
    internal::ApplyNextItemData(next);
    const internal::ItemState item = internal::ItemBehavior(id, size);

    if (item.visible)
    {
        ImDrawList* drawList = window->DrawList;
        const float ring = std::max(1.0f, std::round(HairlineThickness() * kAvatarGroupRingHairlines));
        const ImVec4 clear = WithAlpha(c.fill[2], 0.0f);
        const ImVec4 wash = item.disabled ? clear : internal::StateFill(clear, c.fill[2], c.fill[1], item);
        if (count == 0)
        {
            // No members: an empty outline holds the place, the tooltip says so.
            StrokeRoundRect(drawList, origin, ImVec2(origin.x + diameter, origin.y + diameter),
                            ToU32(item.disabled ? c.separator : ControlBoundaryColor(c)), diameter * 0.5f, HairlineThickness());
        }
        for (int i = 0; i < layout.shown; ++i)
        {
            const AvatarItem& member = items[static_cast<std::size_t>(i)];
            const ImVec2 min(origin.x + std::round(static_cast<float>(i) * layout.step), origin.y);
            const ImRect slot(min, ImVec2(min.x + diameter, min.y + diameter));
            // Each later avatar lies over the one before it, separated by a ring of the surface that
            // stays inside the slot, so nothing leaves the item.
            if (i > 0)
                FillRoundRect(drawList, slot.Min, slot.Max, ToU32(c.bg), diameter * 0.5f);
            ImRect picture = slot;
            picture.Expand(-ring);
            internal::MediaAvatarLook look;
            look.disabled = item.disabled;
            const internal::MediaText name = internal::MediaVisibleText(member.label);
            internal::MediaDrawAvatar(drawList, picture, name.text, member.image, look);
            if (wash.w > 0.0f)
                FillRoundRect(drawList, picture.Min, picture.Max, ToU32(wash), picture.GetWidth() * 0.5f);
        }
        if (layout.hidden > 0)
        {
            const float x = layout.shown > 0 ? origin.x + std::round(static_cast<float>(layout.shown) * layout.step) : origin.x;
            const ImRect slot(ImVec2(x, origin.y), ImVec2(x + layout.chipWidth, origin.y + diameter));
            if (layout.shown > 0)
                FillRoundRect(drawList, slot.Min, slot.Max, ToU32(c.bg), diameter * 0.5f);
            ImRect chip = slot;
            chip.Expand(-ring);
            const ImVec4 fill = item.disabled ? c.fill[3] : internal::StateFill(c.fill[1], c.fill[0], c.fill[0], item);
            FillRoundRect(drawList, chip.Min, chip.Max, ToU32(fill), chip.GetHeight() * 0.5f);
            const internal::ControlTextScope text(internal::GetControlMetrics(ControlSize::Small));
            const ImVec2 textSize = ImGui::CalcTextSize(layout.chip);
            drawList->AddText(ImVec2(std::round(chip.GetCenter().x - textSize.x * 0.5f), CenteredTextY(slot.Min.y, diameter)),
                              ToU32(internal::ControlLabelColor(item.disabled)), layout.chip);
        }
        internal::DrawItemFocusRing(item, diameter * 0.5f, 0, internal::RingShape::Round);
    }

    // Every name, under the visible label part as the heading.
    if (internal::BeginPopupTooltip(id, item.rect, item.hovered, item.focusVisible))
    {
        const internal::MediaText heading = internal::MediaVisibleText(label);
        if (!heading.IsEmpty())
            Text(TextStyle::Headline, "%s", heading.text);
        if (count == 0)
            Text(TextStyle::Footnote, "%s", GetStrings().empty);
        for (const AvatarItem& member : items)
        {
            const internal::MediaText name = internal::MediaVisibleText(member.label);
            if (!name.IsEmpty())
                Text(TextStyle::Footnote, "%s", name.text);
        }
        internal::EndPopupTooltip();
    }
    return item.pressed && !item.disabled;
}

} // namespace facet
