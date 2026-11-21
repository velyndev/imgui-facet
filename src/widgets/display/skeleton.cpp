// Skeleton (facet/widgets/display.h): a loading placeholder in the shape of the content it stands
// for (text lines, a block, a circle) with the shared shimmer sweep, flat while motion is reduced.
// Not interactive and keeps no state: one item that is not a Tab stop.

#include "facet/widgets/display.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "internal/control.h"
#include "internal/item.h"
#include "internal/loading.h"
#include "widgets/display/display_look.h"

namespace facet
{
namespace
{

// A text bar's height as a share of its line: about the height of lower-case letters and
// ascenders, so stacked bars read as lines of text with the line gap between them.
constexpr float kSkeletonBarShare = 0.6f;
// The last of several text lines ends early, like the last line of a paragraph.
constexpr float kSkeletonLastLineShare = 0.6f;

float SkeletonRadius(Radius corners)
{
    const Radii& r = T().radii;
    switch (corners)
    {
    case Radius::Element:
        return Px(r.element);
    case Radius::Control:
        return Px(r.control);
    case Radius::Card:
        return Px(r.card);
    case Radius::Popover:
        return Px(r.popover);
    case Radius::Window:
        return Px(r.window);
    case Radius::None:
    default:
        return 0.0f;
    }
}

} // namespace

void Skeleton(const SkeletonOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return;
    const internal::NextItemData next = internal::TakeNextItemData();
    const int lines = options.shape == SkeletonShape::Text ? std::max(1, options.lines) : 1;
    float lineHeight = 0.0f;
    ImVec2 size;
    if (options.shape == SkeletonShape::Text)
    {
        lineHeight = options.height > 0.0f ? Px(options.height) : std::round(MeasureText(TextStyle::Body, "").y);
        size = ImVec2(internal::ResolveItemWidth(options.width, next, kFill), lineHeight * static_cast<float>(lines));
    }
    else
    {
        const float height = options.height > 0.0f ? Px(options.height) : ControlHeight(ControlSize::Inherit);
        const float width = options.shape == SkeletonShape::Circle && options.width <= 0.0f
                                ? height
                                : internal::ResolveItemWidth(options.width, next, kFill);
        size = options.shape == SkeletonShape::Circle ? ImVec2(std::min(width, height), std::min(width, height))
                                                      : ImVec2(width, height);
    }
    size = ImVec2(std::max(1.0f, std::round(size.x)), std::max(1.0f, std::round(size.y)));

    const ImVec2 origin = window->DC.CursorPos;
    const ImRect bounds(origin, ImVec2(origin.x + size.x, origin.y + size.y));
    ImGui::ItemSize(size);
    internal::ApplyNextItemData(next);
    if (!ImGui::ItemAdd(bounds, internal::DisplayAnonymousId(window, "##Skeleton"), nullptr, ImGuiItemFlags_NoNav))
        return;

    ImDrawList* drawList = window->DrawList;
    switch (options.shape)
    {
    case SkeletonShape::Circle:
        internal::DrawSkeleton(drawList, bounds.Min, bounds.Max, size.x * 0.5f, true);
        break;
    case SkeletonShape::Rect:
        internal::DrawSkeleton(drawList, bounds.Min, bounds.Max, std::min(SkeletonRadius(options.corners), size.y * 0.5f));
        break;
    case SkeletonShape::Text:
    default:
    {
        const float bar = std::max(1.0f, std::round(lineHeight * kSkeletonBarShare));
        for (int i = 0; i < lines; ++i)
        {
            const float top = std::round(origin.y + lineHeight * static_cast<float>(i) + (lineHeight - bar) * 0.5f);
            const float right = lines > 1 && i == lines - 1 ? origin.x + std::round(size.x * kSkeletonLastLineShare)
                                                            : bounds.Max.x;
            internal::DrawSkeleton(drawList, ImVec2(origin.x, top), ImVec2(right, top + bar),
                                   std::min(Px(T().radii.element), bar * 0.5f));
        }
        break;
    }
    }
}

} // namespace facet
