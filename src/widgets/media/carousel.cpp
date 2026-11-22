// Carousel (facet/widgets/media.h): one slide at a time with previous and next buttons over the
// slide, page dots under it and an optional autoplay with a pause button. The slides are one Tab
// stop (Left and Right page, Home and End jump); the buttons are Tab stops of their own and the dots
// take clicks. The slide follows a horizontal drag and snaps by velocity on the standard spring.

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
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/loading.h"
#include "internal/popup_host.h"
#include "internal/state_storage.h"
#include "internal/text_util.h"
#include "widgets/media/media_look.h"

namespace facet
{
namespace
{

using internal::ForcedItemState;

// The default slide height as a share of the width (the header promises 9/16).
constexpr float kCarouselAspect = 9.0f / 16.0f;
// A drag past the first or last slide moves the slide this share of the pointer travel, so the
// end of the row is felt without the slide running away.
constexpr float kCarouselEdgeResistance = 0.35f;
// A forced Dragging state (captures) shows the slide this share of a slide on its way to the next.
constexpr float kCarouselForcedDragShare = 0.35f;
// Part indices for forced states.
constexpr int kCarouselPartSlides = 0;
constexpr int kCarouselPartPrevious = 1;
constexpr int kCarouselPartNext = 2;
constexpr int kCarouselPartPause = 3;

struct MediaCarouselState
{
    internal::MediaSpringValue position;  // pixels along the row of slides
    float slide = 0.0f;                   // the slide the spring settles on, unwrapped while looping
    float pressX = 0.0f;
    float pressPosition = 0.0f;
    float timer = 0.0f;                   // seconds of autoplay on the current slide
    float wheel = 0.0f;                   // sideways wheel notches not yet turned into a page
    int lastIndex = -2;                   // -2: never drawn
    bool moved = false;
    bool paused = false;
};

int WrapIndex(int index, int count)
{
    return count > 0 ? ((index % count) + count) % count : 0;
}

// A round button over the slide (previous, next) or in the dots row (pause): the elevated surface
// with a hairline, so it reads over any picture, and the hover and press washes.
void DrawCarouselButton(ImDrawList* drawList, const internal::ItemState& item, Icon icon, bool overImage)
{
    const Colors& c = T().colors;
    const ImRect& r = item.rect;
    const float radius = r.GetWidth() * 0.5f;
    if (overImage)
    {
        FillRoundRect(drawList, r.Min, r.Max, ToU32(c.bgElevated), radius);
        StrokeRoundRect(drawList, r.Min, r.Max, ToU32(c.separator), radius, HairlineThickness());
    }
    const ImVec4 clear = WithAlpha(c.fill[2], 0.0f);
    const ImVec4 wash = item.disabled ? clear : internal::StateFill(clear, c.fill[2], c.fill[1], item);
    if (wash.w > 0.0f)
        FillRoundRect(drawList, r.Min, r.Max, ToU32(wash), radius);
    const float side = std::floor(r.GetWidth() * 0.5f);
    DrawIcon(drawList, icon, ImVec2(std::round(r.GetCenter().x - side * 0.5f), std::round(r.GetCenter().y - side * 0.5f)),
             side, ToU32(internal::ControlLabelColor(item.disabled)));
    internal::DrawFocusRing(drawList, r, radius, item.focusAmount, 0, internal::RingShape::Round);
}

} // namespace

bool Carousel(const char* label, int* index, std::span<const ImageItem> slides, const CarouselOptions& options)
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
    MediaCarouselState& state = internal::GetState<MediaCarouselState>(id);
    const Theme& t = T();
    const Colors& c = t.colors;
    const Strings& s = GetStrings();
    const bool ready = options.state == DataState::Ready;
    const int count = ready ? static_cast<int>(slides.size()) : 0;
    const bool readOnly = index == nullptr || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    const float dt = DeltaTime();

    // Measure: the slide, then the row of dots under it.
    const float width = std::round(internal::ResolveItemWidth(options.width, next, kFill));
    const float slideHeight = std::max(1.0f, std::round(options.height > 0.0f ? Px(options.height) : width * kCarouselAspect));
    const float rowGap = PxSnapped(t.spacing.xs);
    const float rowHeight = ControlHeight(ControlSize::Small);
    const ImVec2 origin(std::round(window->DC.CursorPos.x), std::round(window->DC.CursorPos.y));
    window->DC.CursorPos = origin;
    const ImRect slideRect(origin, ImVec2(origin.x + width, origin.y + slideHeight));
    const ImRect row(origin.x, slideRect.Max.y + rowGap, origin.x + width, slideRect.Max.y + rowGap + rowHeight);
    const ImVec2 size(width, row.Max.y - origin.y);
    const float gap = PxSnapped(t.spacing.sm);
    const float stride = width + gap;
    const bool autoplay = options.autoplay > 0.0f && !ReduceMotion() && count > 1;

    // The slide shown: the caller's index, clamped for drawing (rule 14).
    const int raw = index != nullptr ? *index : 0;
    const int current = count > 0 ? std::clamp(raw, 0, count - 1) : 0;
    if (count > 0 && current != WrapIndex(static_cast<int>(std::lround(state.slide)), count))
        state.slide = static_cast<float>(current);
    if (state.lastIndex == -2)
        state.position = {state.slide * stride, 0.0f, true};

    ImGui::BeginGroup();
    internal::ApplyNextItemData(next);
    // The slides: one item over the slide that takes presses and the keys; the buttons over it
    // take the pointer first.
    internal::ItemOptions slideOptions;
    slideOptions.press = internal::PressMode::Click;
    slideOptions.forced = &forced;
    slideOptions.part = kCarouselPartSlides;
    slideOptions.allowOverlap = true;
    slideOptions.tabStop = count > 0;
    slideOptions.navigable = count > 0;
    slideOptions.handCursor = count > 1;
    const internal::ItemState body = internal::ItemBehavior(id, slideRect.GetSize(), slideOptions);
    const bool live = !body.disabled && count > 0;

    // Previous and next over the slide's sides, the pause button at the end of the row.
    ImGui::PushOverrideID(id);
    const ImGuiID previousId = ImGui::GetID("##previous");
    const ImGuiID nextId = ImGui::GetID("##next");
    const ImGuiID pauseId = ImGui::GetID("##pause");
    const ImGuiID dotsId = ImGui::GetID("##dots");
    ImGui::PopID();
    const float button = ControlHeight(ControlSize::Regular);
    const float edge = PxSnapped(t.spacing.sm);
    const float buttonTop = std::round(slideRect.GetCenter().y - button * 0.5f);
    const bool showButtons = count > 1 && slideRect.GetWidth() >= button * 2.0f + edge * 4.0f &&
                             slideRect.GetHeight() >= button + edge * 2.0f;
    internal::ItemState previous;
    internal::ItemState following;
    internal::ItemState pause;
    if (showButtons)
    {
        internal::ItemOptions o;
        o.forced = &forced;
        o.part = kCarouselPartPrevious;
        o.disabled = readOnly || (!options.loop && current <= 0);
        previous = internal::PartBehavior(previousId, ImRect(slideRect.Min.x + edge, buttonTop, slideRect.Min.x + edge + button, buttonTop + button), o);
        o.part = kCarouselPartNext;
        o.disabled = readOnly || (!options.loop && current >= count - 1);
        following = internal::PartBehavior(nextId, ImRect(slideRect.Max.x - edge - button, buttonTop, slideRect.Max.x - edge, buttonTop + button), o);
    }
    if (autoplay)
    {
        internal::ItemOptions o;
        o.forced = &forced;
        o.part = kCarouselPartPause;
        pause = internal::PartBehavior(pauseId, ImRect(row.Max.x - rowHeight, row.Min.y, row.Max.x, row.Max.y), o);
        if (pause.pressed && !pause.disabled)
            state.paused = !state.paused;
    }

    // The dots: one per slide while they fit, else the page text.
    const float dot = PxSnapped(t.metrics.pageDotSize);
    const float dotStride = std::max(dot + PxSnapped(t.spacing.sm), Px(t.metrics.minHitTarget) * 0.5f);
    const float dotsRoom = width - (autoplay ? (rowHeight + gap) * 2.0f : 0.0f);
    const bool dotsFit = count > 1 && static_cast<float>(count) * dotStride <= dotsRoom;
    const float dotsWidth = static_cast<float>(count) * dotStride;
    const float dotsLeft = std::round(row.GetCenter().x - dotsWidth * 0.5f);
    int dotPressed = -1;
    if (dotsFit && live && !readOnly)
    {
        internal::ItemOptions o;
        o.forced = &forced;
        o.navigable = false;
        o.tabStop = false;
        for (int i = 0; i < count; ++i)
        {
            const float cx = dotsLeft + (static_cast<float>(i) + 0.5f) * dotStride;
            const ImRect hit(std::round(cx - dotStride * 0.5f), row.Min.y, std::round(cx + dotStride * 0.5f), row.Max.y);
            o.part = -2;  // forced states never land on a dot
            const internal::ItemState part = internal::PartBehavior(ImGui::GetIDWithSeed(i, dotsId), hit, o);
            if (part.pressed)
                dotPressed = i;
        }
    }

    // Input.
    int target = -1;
    float slideTarget = state.slide;
    const auto page = [&](int step) {
        if (count <= 0)
            return;
        if (options.loop)
            slideTarget = std::round(slideTarget) + static_cast<float>(step);
        else
            slideTarget = static_cast<float>(std::clamp(current + step, 0, count - 1));
        target = WrapIndex(static_cast<int>(std::lround(slideTarget)), count);
    };
    bool holding = false;
    const bool editable = live && !readOnly && body.forced == ForcedItemState::None;
    if (editable)
    {
        if (previous.pressed)
            page(-1);
        if (following.pressed)
            page(1);
        if (dotPressed >= 0)
        {
            slideTarget = std::round(slideTarget) + static_cast<float>(dotPressed - current);
            target = dotPressed;
        }
        if (body.focused)
        {
            const bool right = internal::MediaTakeKey(id, ImGuiKey_RightArrow);
            const bool left = internal::MediaTakeKey(id, ImGuiKey_LeftArrow);
            const bool home = internal::MediaTakeKey(id, ImGuiKey_Home);
            const bool end = internal::MediaTakeKey(id, ImGuiKey_End);
            if (home || end)
            {
                const int to = home ? 0 : count - 1;
                slideTarget = std::round(slideTarget) + static_cast<float>(to - current);
                target = to;
            }
            else if (right != left)
                page(right ? 1 : -1);
        }
        // A sideways wheel or Shift+wheel pages, one slide per notch; the plain wheel still scrolls
        // the window.
        if (count > 1)
        {
            const ImVec2 wheel = internal::MediaClaimWheel(id, slideRect, false);
            state.wheel += wheel.x;
            if (std::fabs(state.wheel) >= 1.0f)
            {
                page(state.wheel > 0.0f ? -1 : 1);
                state.wheel = 0.0f;
            }
        }
        // A horizontal drag moves the slide; the release pages by distance or by a flick.
        if (body.activated && !body.keyboardPressed && ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            state.pressX = g.IO.MousePos.x;
            state.pressPosition = state.position.position;
            state.moved = false;
        }
        if (body.active && g.ActiveIdSource == ImGuiInputSource_Mouse && ImGui::IsMouseDown(ImGuiMouseButton_Left) && count > 1)
        {
            const float dx = g.IO.MousePos.x - state.pressX;
            if (!state.moved && std::fabs(dx) >= Px(t.drag.startThreshold))
                state.moved = true;
            if (state.moved)
            {
                holding = true;
                float at = state.pressPosition - dx;
                if (!options.loop)
                {
                    const float low = 0.0f;
                    const float high = static_cast<float>(count - 1) * stride;
                    if (at < low)
                        at = low + (at - low) * kCarouselEdgeResistance;
                    else if (at > high)
                        at = high + (at - high) * kCarouselEdgeResistance;
                }
                internal::MediaSpringHold(state.position, at, dt);
            }
        }
        if (body.released && state.moved)
        {
            state.moved = false;
            const float from = std::round(state.slide);
            const float travel = (state.position.position - from * stride) / stride;
            int step = static_cast<int>(std::lround(travel));
            if (std::fabs(state.position.velocity) >= Px(t.drag.flickSpeed))
                step = state.position.velocity > 0.0f ? std::max(step, 1) : std::min(step, -1);
            step = std::clamp(step, -1, 1);
            slideTarget = from;
            page(step);
            if (target < 0)
                target = current;
        }
    }

    // Autoplay: one slide every `autoplay` seconds while nobody is looking at a part of it.
    // Visible keyboard focus counts, not the focus a window hands its first item when it appears.
    const bool attended = body.hovered || body.focusVisible || body.active || previous.hovered || following.hovered ||
                          pause.hovered || previous.focusVisible || following.focusVisible || pause.focusVisible;
    if (autoplay && !readOnly && !state.paused && !attended && !holding && !body.disabled && target < 0)
    {
        state.timer += dt;
        if (state.timer >= options.autoplay)
        {
            if (!options.loop && current >= count - 1)
            {
                slideTarget = std::round(slideTarget) - static_cast<float>(current);
                target = 0;
            }
            else
            {
                page(1);
            }
        }
    }
    if (target >= 0 || attended)
        state.timer = 0.0f;

    bool written = false;
    if (target >= 0 && index != nullptr)
    {
        written = target != raw;
        if (written)
            *index = target;
    }
    state.slide = slideTarget;
    state.lastIndex = target >= 0 ? target : current;
    if (!holding)
        internal::MediaSpringStep(state.position, state.slide * stride, t.motion.standard, dt);
    float position = state.position.position;
    if ((body.forced & ForcedItemState::Dragging) != ForcedItemState::None && count > 1)
        position += stride * kCarouselForcedDragShare;

    // Draw.
    ImDrawList* drawList = window->DrawList;
    const float radius = std::min(Px(t.radii.card), 0.5f * std::min(width, slideHeight));
    const internal::MediaText alt = internal::MediaVisibleText(label);
    if (ImGui::IsRectVisible(slideRect.Min, row.Max))
    {
        drawList->PushClipRect(slideRect.Min, slideRect.Max, true);
        if (options.state == DataState::Loading)
        {
            internal::DrawSkeleton(drawList, slideRect.Min, slideRect.Max, radius);
        }
        else if (count == 0)
        {
            FillRect(drawList, slideRect.Min, slideRect.Max, ToU32(c.fill[3]), radius);
            const bool error = options.state == DataState::Error;
            const char* text = error ? (options.errorText != nullptr ? options.errorText : s.loadFailed)
                                     : (options.emptyText != nullptr ? options.emptyText : s.empty);
            internal::MediaDrawMessage(drawList, slideRect, Icon(error ? "error" : "image"), text, body.disabled);
        }
        else
        {
            const float at = position / stride;
            const int first = static_cast<int>(std::floor(at));
            for (int j = first; j <= first + 1; ++j)
            {
                const int slide = options.loop ? WrapIndex(j, count) : j;
                if (slide < 0 || slide >= count)
                    continue;
                const float x = std::round(slideRect.Min.x + static_cast<float>(j) * stride - position);
                const ImRect box(x, slideRect.Min.y, x + width, slideRect.Max.y);
                if (box.Max.x <= slideRect.Min.x || box.Min.x >= slideRect.Max.x)
                    continue;
                const ImageItem& entry = slides[static_cast<std::size_t>(slide)];
                const internal::MediaText name = internal::MediaVisibleText(entry.label);
                const internal::MediaImage picture = internal::MediaImageOf(entry.image);
                // Contain leaves bands beside the picture: the slide surface fills them.
                if (picture.status == ImageStatus::Ready && options.fit == ImageFit::Contain)
                    FillRect(drawList, box.Min, box.Max, ToU32(c.fill[3]), radius);
                internal::MediaDrawOptions draw;
                draw.fit = options.fit;
                draw.radius = radius;
                internal::MediaDrawImageSlot(drawList, picture, box, draw, name.text);
                if (options.captions)
                    internal::MediaDrawCaption(drawList, box, internal::MediaCorner::BottomLeft, name.text, body.disabled);
            }
        }
        drawList->PopClipRect();
        if (showButtons)
        {
            DrawCarouselButton(drawList, previous, Icon("chevron-left"), true);
            DrawCarouselButton(drawList, following, Icon("chevron-right"), true);
        }
        internal::DrawFocusRing(drawList, slideRect, radius, body.focusAmount);

        // The dots, or the page text where they do not fit.
        if (dotsFit)
        {
            for (int i = 0; i < count; ++i)
            {
                const float cx = dotsLeft + (static_cast<float>(i) + 0.5f) * dotStride;
                const bool on = i == (target >= 0 ? target : current);
                // The current page in the accent mark (3:1 on every surface), the others in the quiet
                // controlBorder; disabled dots take separator, never lighter than enabled ones.
                const ImVec4 color = body.disabled ? c.separator : (on ? c.focusRing : c.controlBorder);
                drawList->AddCircleFilled(ImVec2(std::round(cx), std::round(row.GetCenter().y)), dot * 0.5f, ToU32(color));
            }
        }
        else if (count > 1)
        {
            char text[64];
            const internal::TextToken tokens[] = {{"page", (target >= 0 ? target : current) + 1}, {"count", count}};
            internal::FormatTokens(text, sizeof(text), s.pageOf, tokens);
            const internal::ControlTextScope scope(TextStyle::Footnote);
            const internal::FittedText shown = internal::FitText(text, nullptr, std::max(0.0f, dotsRoom));
            drawList->AddText(ImVec2(std::round(row.GetCenter().x - shown.width * 0.5f), CenteredTextY(row.Min.y, rowHeight)),
                              ToU32(internal::ControlSecondaryColor(body.disabled)), shown.text);
        }
        if (autoplay)
            DrawCarouselButton(drawList, pause, Icon(state.paused ? "play" : "pause"), false);
    }

    // The group: as wide as the carousel, the slide and the row under it.
    ImGui::SetCursorScreenPos(origin);
    ImGui::Dummy(size);
    ImGui::EndGroup();
    internal::FinishWidgetGroup(id);

    // Tooltips: the label for the slides, the button names for the buttons.
    if (!alt.IsEmpty() && internal::BeginPopupTooltip(id, slideRect, body.hovered && !previous.hovered && !following.hovered,
                                                      body.focusVisible))
    {
        Text(TextStyle::Footnote, "%s", alt.text);
        internal::EndPopupTooltip();
    }
    if (showButtons && internal::BeginPopupTooltip(previousId, previous.rect, previous.hovered, previous.focusVisible))
    {
        Text(TextStyle::Footnote, "%s", s.previous);
        internal::EndPopupTooltip();
    }
    if (showButtons && internal::BeginPopupTooltip(nextId, following.rect, following.hovered, following.focusVisible))
    {
        Text(TextStyle::Footnote, "%s", s.next);
        internal::EndPopupTooltip();
    }
    if (autoplay && internal::BeginPopupTooltip(pauseId, pause.rect, pause.hovered, pause.focusVisible))
    {
        Text(TextStyle::Footnote, "%s", state.paused ? s.play : s.pause);
        internal::EndPopupTooltip();
    }
    return written;
}

} // namespace facet
