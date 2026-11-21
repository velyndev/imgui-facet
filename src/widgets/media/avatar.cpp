// Avatar (facet/widgets/media.h): a round picture of a person, team or device, with the initials of
// its name (or the user icon) while there is no ready image, a status dot and a selection ring. One
// item, a Tab stop only when activatable. Keeps no state.

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
#include "widgets/display/display_look.h"
#include "widgets/media/media_look.h"

namespace facet
{
namespace
{

// The status dot grows with the avatar: this share of the diameter, never smaller than the status
// dot token, so it stays visible on small avatars and in proportion on large ones.
constexpr float kAvatarStatusShare = 0.28f;
// The ring around the status dot in the surface color, in hairlines: it separates the dot from
// the picture under it.
constexpr float kAvatarStatusRingHairlines = 2.0f;

} // namespace

bool Avatar(const char* label, ImageHandle image, const AvatarOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const internal::NextItemData next = internal::TakeNextItemData();
    const ImGuiID id = window->GetID(label);
    const float diameter = internal::MediaAvatarDiameter(options.diameter, options.size);
    const ImVec2 origin(std::round(window->DC.CursorPos.x), std::round(window->DC.CursorPos.y));
    window->DC.CursorPos = origin;
    const ImVec2 size(diameter, diameter);
    const ImRect box(origin, ImVec2(origin.x + diameter, origin.y + diameter));

    internal::ItemState item;
    internal::ApplyNextItemData(next);
    if (options.activatable)
    {
        item = internal::ItemBehavior(id, size);
    }
    else
    {
        ImGui::ItemSize(size);
        item.id = id;
        item.rect = box;
        item.visible = ImGui::ItemAdd(box, id, nullptr, ImGuiItemFlags_NoNav);
        item.disabled = (GImGui->LastItemData.ItemFlags & ImGuiItemFlags_Disabled) != 0;
        item.hovered = item.visible && ImGui::IsItemHovered() && !item.disabled;
    }

    const internal::MediaText name = internal::MediaVisibleText(label);
    if (item.visible)
    {
        const Theme& t = T();
        const Colors& c = t.colors;
        ImDrawList* drawList = window->DrawList;
        internal::MediaAvatarLook look;
        look.squircle = options.squircle;
        look.disabled = item.disabled;
        look.selected = options.selected;
        internal::MediaDrawAvatar(drawList, box, name.text, image, look);
        const float radius = internal::MediaAvatarRadius(diameter, options.squircle);
        if (options.activatable)
        {
            const ImVec4 clear = WithAlpha(c.fill[2], 0.0f);
            const ImVec4 wash = internal::StateFill(clear, c.fill[2], c.fill[1], item);
            if (wash.w > 0.0f && !item.disabled)
            {
                if (options.squircle)
                    FillRect(drawList, box.Min, box.Max, ToU32(wash), radius);
                else
                    FillRoundRect(drawList, box.Min, box.Max, ToU32(wash), radius);
            }
        }
        if (options.showStatus)
        {
            // On the lower right, on the circle's edge, ringed in the surface color.
            const float dot = std::max(PxSnapped(t.metrics.statusDotSize), std::round(diameter * kAvatarStatusShare));
            const float ring = std::max(1.0f, std::round(HairlineThickness() * kAvatarStatusRingHairlines));
            const float inset = options.squircle ? dot * 0.5f : diameter * 0.5f * (1.0f - std::sqrt(0.5f));
            const float at = std::min(std::round(diameter - inset), diameter - dot * 0.5f);
            const ImVec2 dotCenter(box.Min.x + at, box.Min.y + at);
            drawList->AddCircleFilled(dotCenter, dot * 0.5f + ring, ToU32(c.bg));
            const ImVec4 color = internal::DisplayToneLook(options.status, item.disabled).glyph;
            internal::DrawDisplayDot(drawList, dotCenter, dot, ToU32(color), false);
        }
        if (options.activatable)
            internal::DrawFocusRing(drawList, box, radius, item.focusAmount, 0,
                                    options.squircle ? internal::RingShape::Rect : internal::RingShape::Round);
    }

    // The name is the tooltip on hover and on keyboard focus.
    if (!name.IsEmpty() && internal::BeginPopupTooltip(id, box, item.hovered, item.focusVisible))
    {
        Text(TextStyle::Footnote, "%s", name.text);
        internal::EndPopupTooltip();
    }
    return options.activatable && item.pressed && !item.disabled;
}

} // namespace facet
