// ImageViewer (facet/widgets/media.h): a zoom and pan viewport over one image. The wheel zooms around
// the pointer, a drag pans and a released pan glides on (drag.inertiaTime), a double click toggles
// between fit and 100 percent; while focused, + and - zoom, the arrows pan, 0 fits and 1 shows 100
// percent. The view is the caller's ImageViewerState; the pan is clamped so the image never leaves
// the viewport, and an image smaller than the viewport stays centered.

#include "facet/widgets/media.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/context.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "image/image_internal.h"
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

using internal::ForcedItemState;

// One wheel notch or one + or - press zooms by this factor.
constexpr float kViewerZoomStep = 1.25f;
// The minimap is at most this share of the viewport's width.
constexpr float kViewerMinimapShare = 0.25f;
// The visible region on the minimap is outlined this many hairlines wide.
constexpr float kViewerMinimapRingHairlines = 2.0f;
// The pan speed is smoothed at this rate per second (about two frames at 60 Hz), so one uneven
// frame does not decide the glide.
constexpr float kViewerVelocitySmoothing = 30.0f;
// A glide stops below this speed, in pixels per second.
constexpr float kViewerRestSpeed = 1.0f;

struct MediaViewerState
{
    ImVec2 lastMouse = ImVec2(0.0f, 0.0f);
    ImVec2 velocity = ImVec2(0.0f, 0.0f);  // pixels per second while panning and gliding
    bool panning = false;
};

// The center clamped so the image covers the viewport where it is larger than it, and stays in the
// middle where it is smaller. `scale` is pixels per image pixel.
ImVec2 ClampViewerCenter(ImVec2 center, ImVec2 image, ImVec2 viewport, float scale)
{
    const ImVec2 half(viewport.x * 0.5f / scale, viewport.y * 0.5f / scale);
    const auto axis = [](float c, float length, float h) {
        if (!std::isfinite(c))
            return length * 0.5f;
        return length <= h * 2.0f ? length * 0.5f : std::clamp(c, h, length - h);
    };
    return ImVec2(axis(center.x, image.x, half.x), axis(center.y, image.y, half.y));
}

} // namespace

bool ImageViewer(const char* label, ImageHandle image, ImageViewerState* view, const ImageViewerOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    ImGuiContext& g = *GImGui;
    const internal::NextItemData next = internal::TakeNextItemData();
    const internal::ForcedItemRequest forced = internal::TakeForcedItemState();
    const ImGuiID id = window->GetID(label);
    MediaViewerState& state = internal::GetState<MediaViewerState>(id);
    const Theme& t = T();
    const Colors& c = t.colors;
    const internal::MediaImage picture = internal::MediaImageOf(image);

    // Measure: the available width and height by default.
    const float width = std::round(internal::ResolveItemWidth(options.width, next, kFill));
    float height = 0.0f;
    if (options.height > 0.0f)
        height = Px(options.height);
    else
        height = ImGui::GetContentRegionAvail().y + (options.height < 0.0f && options.height != kFill ? Px(options.height) : 0.0f);
    height = std::round(std::max(height, ControlHeight(ControlSize::Large)));
    const ImVec2 origin(std::round(window->DC.CursorPos.x), std::round(window->DC.CursorPos.y));
    window->DC.CursorPos = origin;
    const ImVec2 size(width, height);
    const ImRect rect(origin, ImVec2(origin.x + width, origin.y + height));

    internal::ItemOptions itemOptions;
    itemOptions.press = internal::PressMode::Click;
    itemOptions.forced = &forced;
    internal::ApplyNextItemData(next);
    const internal::ItemState item = internal::ItemBehavior(id, size, itemOptions);

    // The view: fit (zoom 0) or the caller's zoom, within the limits.
    const bool ready = picture.status == ImageStatus::Ready;
    const ImVec2 imageSize = ready ? picture.size : ImVec2(1.0f, 1.0f);
    const float unit = Px(1.0f);
    const float fitScale = std::min(width / imageSize.x, height / imageSize.y);
    const float minScale = std::min(fitScale, unit);
    const float maxScale = std::max(minScale, unit * std::max(1.0f, std::isfinite(options.maxZoom) ? options.maxZoom : 1.0f));
    ImageViewerState shown = view != nullptr ? *view : ImageViewerState{};
    const bool fitted = !(shown.zoom > 0.0f) || !std::isfinite(shown.zoom);
    float scale = fitted ? fitScale : std::clamp(shown.zoom * unit, minScale, maxScale);
    ImVec2 center = fitted ? ImVec2(imageSize.x * 0.5f, imageSize.y * 0.5f) : shown.center;
    center = ClampViewerCenter(center, imageSize, size, scale);

    const bool live = ready && view != nullptr && !item.disabled && item.forced == ForcedItemState::None;
    bool edited = false;
    bool toFit = false;
    const float dt = DeltaTime();
    const ImVec2 viewportCenter = rect.GetCenter();
    // Zooms to `next` pixels per image pixel, keeping the image point under `anchor` where it is.
    const auto zoomAround = [&](float nextScale, ImVec2 anchor) {
        nextScale = std::clamp(nextScale, minScale, maxScale);
        if (nextScale == scale)
            return;
        const ImVec2 offset(anchor.x - viewportCenter.x, anchor.y - viewportCenter.y);
        const ImVec2 point(center.x + offset.x / scale, center.y + offset.y / scale);
        scale = nextScale;
        center = ImVec2(point.x - offset.x / scale, point.y - offset.y / scale);
        edited = true;
    };
    const auto pan = [&](ImVec2 pixels) {
        if (pixels.x == 0.0f && pixels.y == 0.0f)
            return;
        center = ImVec2(center.x + pixels.x / scale, center.y + pixels.y / scale);
        edited = true;
    };
    if (live)
    {
        // The wheel zooms around the pointer; a sideways wheel pans.
        const ImVec2 wheel = internal::MediaClaimWheel(id, rect);
        if (wheel.y != 0.0f && !g.IO.KeyShift)
            zoomAround(scale * std::pow(kViewerZoomStep, wheel.y), g.IO.MousePos);
        else if (wheel.x != 0.0f || wheel.y != 0.0f)
            pan(ImVec2(-(wheel.x + wheel.y) * Px(t.scroll.wheelDistance), 0.0f));

        // A double click toggles between fit and 100 percent around the pointer.
        if (item.hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            if (std::fabs(scale - fitScale) <= fitScale * 0.01f)
                zoomAround(unit, g.IO.MousePos);
            else
                toFit = true;
            state.panning = false;
            state.velocity = ImVec2(0.0f, 0.0f);
        }
        // A drag pans, and the image glides on after the release.
        else if (item.activated && !item.keyboardPressed && ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            state.panning = true;
            state.lastMouse = g.IO.MousePos;
            state.velocity = ImVec2(0.0f, 0.0f);
        }
        if (state.panning && item.active && ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            const ImVec2 delta(g.IO.MousePos.x - state.lastMouse.x, g.IO.MousePos.y - state.lastMouse.y);
            state.lastMouse = g.IO.MousePos;
            pan(ImVec2(-delta.x, -delta.y));
            if (dt > 0.0f)
            {
                const float keep = std::exp(-dt * kViewerVelocitySmoothing);
                state.velocity = ImVec2(state.velocity.x * keep + delta.x / dt * (1.0f - keep),
                                        state.velocity.y * keep + delta.y / dt * (1.0f - keep));
            }
        }
        else if (state.panning)
        {
            state.panning = false;
            if (ReduceMotion())
                state.velocity = ImVec2(0.0f, 0.0f);
        }
        else if (state.velocity.x != 0.0f || state.velocity.y != 0.0f)
        {
            pan(ImVec2(-state.velocity.x * dt, -state.velocity.y * dt));
            const float decay = std::exp(-dt / std::max(1e-3f, t.drag.inertiaTime * t.motion.scale));
            state.velocity = ImVec2(state.velocity.x * decay, state.velocity.y * decay);
            if (std::hypot(state.velocity.x, state.velocity.y) < kViewerRestSpeed)
                state.velocity = ImVec2(0.0f, 0.0f);
        }

        // The keys while focused.
        if (item.focused)
        {
            const bool zoomIn = internal::MediaTakeKey(id, ImGuiKey_Equal) || internal::MediaTakeKey(id, ImGuiKey_KeypadAdd);
            const bool zoomOut = internal::MediaTakeKey(id, ImGuiKey_Minus) || internal::MediaTakeKey(id, ImGuiKey_KeypadSubtract);
            const bool fit = internal::MediaTakeKey(id, ImGuiKey_0) || internal::MediaTakeKey(id, ImGuiKey_Keypad0);
            const bool actual = internal::MediaTakeKey(id, ImGuiKey_1) || internal::MediaTakeKey(id, ImGuiKey_Keypad1);
            const float stepPx = Px(t.spacing.xxxl);
            const float dx = (internal::MediaTakeKey(id, ImGuiKey_RightArrow) ? 1.0f : 0.0f) -
                             (internal::MediaTakeKey(id, ImGuiKey_LeftArrow) ? 1.0f : 0.0f);
            const float dy = (internal::MediaTakeKey(id, ImGuiKey_DownArrow) ? 1.0f : 0.0f) -
                             (internal::MediaTakeKey(id, ImGuiKey_UpArrow) ? 1.0f : 0.0f);
            if (zoomIn != zoomOut)
                zoomAround(scale * (zoomIn ? kViewerZoomStep : 1.0f / kViewerZoomStep), viewportCenter);
            if (fit)
                toFit = true;
            else if (actual)
                zoomAround(unit, viewportCenter);
            pan(ImVec2(dx * stepPx, dy * stepPx));
        }
    }
    else
    {
        state.panning = false;
        state.velocity = ImVec2(0.0f, 0.0f);
    }

    bool written = false;
    if (toFit && view != nullptr)
    {
        scale = fitScale;
        center = ImVec2(imageSize.x * 0.5f, imageSize.y * 0.5f);
        written = view->zoom != 0.0f || view->center.x != 0.0f || view->center.y != 0.0f;
        *view = ImageViewerState{};
    }
    else if (edited && view != nullptr)
    {
        center = ClampViewerCenter(center, imageSize, size, scale);
        const ImageViewerState updated{scale / unit, center};
        written = updated.zoom != view->zoom || updated.center.x != view->center.x || updated.center.y != view->center.y;
        *view = updated;
    }

    // Draw.
    ImDrawList* drawList = window->DrawList;
    const float radius = std::min(Px(t.radii.card), 0.5f * std::min(width, height));
    const internal::MediaText alt = internal::MediaVisibleText(label);
    if (item.visible)
    {
        FillRect(drawList, rect.Min, rect.Max, ToU32(item.disabled ? c.fill[3] : c.fill[2]), radius);
        if (picture.status == ImageStatus::Loading)
        {
            internal::DrawSkeleton(drawList, rect.Min, rect.Max, radius);
        }
        else if (!ready)
        {
            internal::DrawImageFailed(drawList, rect.Min, rect.Max, radius, alt.text);
        }
        else
        {
            // Whole pixels at 100 percent and below, so the picture stays sharp.
            ImVec2 min(viewportCenter.x - center.x * scale, viewportCenter.y - center.y * scale);
            min = ImVec2(std::round(min.x), std::round(min.y));
            const ImVec2 max(min.x + std::round(imageSize.x * scale), min.y + std::round(imageSize.y * scale));
            drawList->PushClipRect(rect.Min, rect.Max, true);
            if (options.checkerboard)
            {
                const ImVec2 cMin(std::max(min.x, rect.Min.x), std::max(min.y, rect.Min.y));
                const ImVec2 cMax(std::min(max.x, rect.Max.x), std::min(max.y, rect.Max.y));
                if (cMax.x > cMin.x && cMax.y > cMin.y)
                    FillCheckerboard(drawList, cMin, cMax, ToU32(c.checker[0]), ToU32(c.checker[1]),
                                     PxSnapped(t.metrics.checkerCell));
            }
            internal::MediaDrawOptions draw;
            draw.fit = ImageFit::Fill;
            internal::MediaDrawTexture(drawList, picture, ImRect(min, max), draw);
            drawList->PopClipRect();

            // The minimap shows where the viewport is while the image is larger than it.
            const bool larger = max.x - min.x > width + 0.5f || max.y - min.y > height + 0.5f;
            if (options.minimap && larger)
            {
                const float inset = PxSnapped(t.spacing.sm);
                const float mapWidth = std::min(Px(t.metrics.thumbnailSize), width * kViewerMinimapShare);
                const float mapScale = std::min(mapWidth / imageSize.x, (height * kViewerMinimapShare * 2.0f) / imageSize.y);
                const ImVec2 mapSize(std::round(imageSize.x * mapScale), std::round(imageSize.y * mapScale));
                const ImVec2 mapMin(rect.Max.x - inset - mapSize.x, rect.Max.y - inset - mapSize.y);
                const ImVec2 mapMax(mapMin.x + mapSize.x, mapMin.y + mapSize.y);
                if (mapSize.x >= 2.0f && mapSize.y >= 2.0f && mapMin.x > rect.Min.x && mapMin.y > rect.Min.y)
                {
                    const float frame = HairlineThickness();
                    FillRect(drawList, ImVec2(mapMin.x - frame, mapMin.y - frame), ImVec2(mapMax.x + frame, mapMax.y + frame),
                             ToU32(c.bgElevated));
                    internal::MediaDrawTexture(drawList, picture, ImRect(mapMin, mapMax), draw);
                    StrokeRect(drawList, ImVec2(mapMin.x - frame, mapMin.y - frame), ImVec2(mapMax.x + frame, mapMax.y + frame),
                               ToU32(c.separator), 0.0f, frame);
                    const ImVec2 half(width * 0.5f / scale, height * 0.5f / scale);
                    ImVec2 a(mapMin.x + (center.x - half.x) * mapScale, mapMin.y + (center.y - half.y) * mapScale);
                    ImVec2 b(mapMin.x + (center.x + half.x) * mapScale, mapMin.y + (center.y + half.y) * mapScale);
                    a = ImVec2(std::round(std::max(a.x, mapMin.x)), std::round(std::max(a.y, mapMin.y)));
                    b = ImVec2(std::round(std::min(b.x, mapMax.x)), std::round(std::min(b.y, mapMax.y)));
                    const float ring = std::max(1.0f, std::round(frame * kViewerMinimapRingHairlines));
                    StrokeRect(drawList, a, b, ToU32(internal::FocusRingColor()), 0.0f, ring);
                }
            }
        }
        StrokeRect(drawList, rect.Min, rect.Max, ToU32(c.separator), radius, HairlineThickness());
        internal::DrawItemFocusRing(item, radius);
    }

    // The alt text is the tooltip of the viewport.
    if (!alt.IsEmpty() && internal::BeginPopupTooltip(id, rect, item.hovered && !item.active, item.focusVisible))
    {
        Text(TextStyle::Footnote, "%s", alt.text);
        internal::EndPopupTooltip();
    }
    return written;
}

} // namespace facet
