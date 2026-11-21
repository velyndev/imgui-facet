// ImageView (facet/widgets/media.h): an image as one item, sized in design units, with its loading
// and failed states, an optional activation and an optional retry button in the failed state. It
// follows the order of the model widget (src/widgets/buttons/button.cpp): entry, measure, items,
// draw from tokens, then the tooltip and the result.

#include "facet/widgets/media.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/anim.h"
#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "facet/widgets/buttons.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/loading.h"
#include "internal/popup_host.h"
#include "internal/state_storage.h"
#include "widgets/media/media_look.h"

namespace facet
{
namespace
{

// Animation sub key of the fade-in under the widget id.
constexpr int kImageViewFadeSub = 1;

// The size the box last took from its image, so a box whose image starts loading again, fails or
// is released keeps its place on the page instead of jumping to the placeholder square.
struct MediaImageViewState
{
    ImVec2 natural = ImVec2(0.0f, 0.0f);
};

// The retry button of the last call and the frame it ran in.
struct MediaImageViewRetry
{
    int frame = -1;
    bool pressed = false;
};

MediaImageViewRetry& ImageViewLastRetry()
{
    static MediaImageViewRetry retry;
    return retry;
}

// The box size in pixels: set components resolve as for every widget (rule 6), a component left
// at 0 follows the image's aspect ratio, and both at 0 take the image's own size.
ImVec2 ImageViewBoxSize(ImGuiWindow* window, const ImageViewOptions& options, const internal::NextItemData& next,
                        ImVec2 natural)
{
    const bool widthSet = options.width != 0.0f || next.hasWidth || window->DC.ItemWidthStack.Size > 0;
    float width = widthSet ? internal::ResolveItemWidth(options.width, next, kFill) : 0.0f;
    float height = 0.0f;
    if (options.height > 0.0f)
        height = Px(options.height);
    else if (options.height < 0.0f)
        height = ImGui::GetContentRegionAvail().y + (options.height == kFill ? 0.0f : Px(options.height));
    if (width <= 0.0f && height <= 0.0f)
    {
        width = natural.x;
        height = natural.y;
    }
    else if (width <= 0.0f)
        width = height * natural.x / natural.y;
    else if (height <= 0.0f)
        height = width * natural.y / natural.x;
    return ImVec2(std::max(1.0f, std::round(width)), std::max(1.0f, std::round(height)));
}

bool ImageViewImpl(const char* label, const internal::MediaImage& picture, const ImageViewOptions& options)
{
    ImageViewLastRetry() = {};
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const internal::NextItemData next = internal::TakeNextItemData();
    const internal::ForcedItemRequest forced = internal::TakeForcedItemState();
    const ImGuiID id = window->GetID(label);
    const Theme& t = T();
    const Colors& c = t.colors;

    // Measure: the image's own size, else the size it had last, else the placeholder square.
    MediaImageViewState& state = internal::GetState<MediaImageViewState>(id);
    if (picture.status == ImageStatus::Ready)
        state.natural = picture.size;
    ImVec2 natural = state.natural;
    if (natural.x <= 0.0f || natural.y <= 0.0f)
        natural = ImVec2(Px(t.metrics.controlHeightLarge), Px(t.metrics.controlHeightLarge));
    const ImVec2 size = ImageViewBoxSize(window, options, next, natural);
    // Whole pixels, so an image shown at its own size is sampled one texel per pixel.
    const ImVec2 origin(std::round(window->DC.CursorPos.x), std::round(window->DC.CursorPos.y));
    window->DC.CursorPos = origin;
    const ImRect box(origin, ImVec2(origin.x + size.x, origin.y + size.y));
    const float radius = std::min(internal::MediaRadiusPx(options.corners), 0.5f * std::min(size.x, size.y));
    const internal::MediaText alt = internal::MediaVisibleText(label);

    const bool failed = picture.status != ImageStatus::Ready && picture.status != ImageStatus::Loading;
    const bool retry = failed && options.showStates && options.retryLabel != nullptr && options.retryLabel[0] != '\0';
    if (retry)
        ImGui::BeginGroup();

    // The image item: a Tab stop when activatable, otherwise an item that only answers hover.
    internal::ItemState item;
    internal::ApplyNextItemData(next);
    if (options.activatable)
    {
        internal::ItemOptions itemOptions;
        itemOptions.forced = &forced;
        itemOptions.allowOverlap = retry;  // the retry button on it takes the pointer first
        item = internal::ItemBehavior(id, size, itemOptions);
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

    const float fade = Tween(id, kImageViewFadeSub, picture.status == ImageStatus::Ready ? 1.0f : 0.0f,
                             t.motion.durationFast);
    const float buttonHeight = ControlHeight(ControlSize::Small);
    const float gap = PxSnapped(t.spacing.xs);
    // With a retry button the marker takes the space above it, when the box has room for both.
    const bool markerAbove = retry && box.GetHeight() >= buttonHeight * 2.0f + gap * 3.0f;
    ImDrawList* drawList = window->DrawList;
    if (item.visible)
    {
        internal::MediaDrawOptions draw;
        draw.fit = options.fit;
        draw.radius = radius;
        draw.tint = options.tint;
        if (picture.status == ImageStatus::Ready)
        {
            // A fading image rests on the skeleton it replaces.
            if (fade < 1.0f && options.showStates)
            {
                DrawGroup group(drawList, 1.0f - fade);
                internal::DrawSkeleton(drawList, box.Min, box.Max, radius);
            }
            draw.opacity = fade;
            internal::MediaDrawTexture(drawList, picture, box, draw);
        }
        else if (options.showStates && retry)
        {
            // One surface for the whole box, the marker and the button on it.
            FillRect(drawList, box.Min, box.Max, ToU32(c.fill[3]), radius);
            StrokeRect(drawList, box.Min, box.Max, ToU32(c.separator), radius, HairlineThickness());
            if (markerAbove)
            {
                const ImRect marker(box.Min, ImVec2(box.Max.x, box.Max.y - buttonHeight - gap * 2.0f));
                internal::MediaDrawImageSlot(drawList, picture, marker, draw, alt.text);
            }
        }
        else if (options.showStates)
        {
            internal::MediaDrawImageSlot(drawList, picture, box, draw, alt.text);
        }
        if (options.activatable)
        {
            // Hover and press feedback as a wash over the picture; the disabled wash says it is off.
            const ImVec4 clear = WithAlpha(c.fill[2], 0.0f);
            const ImVec4 wash = internal::StateFill(clear, c.fill[2], c.fill[1], item);
            if (wash.w > 0.0f)
                FillRect(drawList, box.Min, box.Max, ToU32(wash), radius);
            internal::DrawItemFocusRing(item, radius);
        }
    }

    if (retry)
    {
        // The retry button sits at the bottom of the box (or in its middle when the marker has no
        // room), centered and never wider than the box.
        const char* retryLabel = options.retryLabel;
        float contentWidth = 0.0f;
        {
            const internal::ControlMetrics small = internal::GetControlMetrics(ControlSize::Small);
            const internal::ControlTextScope text(small);
            contentWidth = std::ceil(small.paddingX * 2.0f +
                                     ImGui::CalcTextSize(retryLabel, ImGui::FindRenderedTextEnd(retryLabel)).x);
        }
        const float buttonWidth = std::min(std::max(1.0f, box.GetWidth() - gap * 2.0f), contentWidth);
        const float top = markerAbove ? box.Max.y - buttonHeight - gap : std::round(box.GetCenter().y - buttonHeight * 0.5f);
        ImGui::PushOverrideID(id);
        ImGui::SetCursorScreenPos(ImVec2(std::round(box.GetCenter().x - buttonWidth * 0.5f), top));
        ImGui::PushClipRect(box.Min, box.Max, true);
        const bool pressed = Button(retryLabel, {.size = ControlSize::Small, .width = buttonWidth / Px(1.0f)});
        ImGui::PopClipRect();
        ImGui::PopID();
        // The group covers exactly the box.
        ImGui::SetCursorScreenPos(box.Min);
        ImGui::Dummy(size);
        ImGui::EndGroup();
        internal::FinishWidgetGroup(id);
        ImageViewLastRetry() = {ImGui::GetFrameCount(), pressed};
    }

    // The alt text is the tooltip on hover and on keyboard focus.
    if (!alt.IsEmpty() && internal::BeginPopupTooltip(id, box, item.hovered, item.focusVisible))
    {
        Text(TextStyle::Footnote, "%s", alt.text);
        internal::EndPopupTooltip();
    }
    return options.activatable && item.pressed && !item.disabled;
}

} // namespace

bool ImageView(const char* label, ImageHandle image, const ImageViewOptions& options)
{
    return ImageViewImpl(label, internal::MediaImageOf(image), options);
}

bool ImageView(const char* label, ID3D11ShaderResourceView* texture, const ImageViewOptions& options)
{
    return ImageViewImpl(label, internal::MediaImageOf(texture), options);
}

bool ImageViewRetryPressed()
{
    const MediaImageViewRetry& retry = ImageViewLastRetry();
    return retry.pressed && ImGui::GetCurrentContext() != nullptr && retry.frame == ImGui::GetFrameCount();
}

} // namespace facet
