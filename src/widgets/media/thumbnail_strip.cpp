// ThumbnailStrip (facet/widgets/media.h): a horizontally scrolling row of square thumbnails with the
// chosen one ringed. One item and one Tab stop: Left and Right move the selection, Home and End
// jump, a click chooses, a drag or the wheel scrolls (a released drag glides on and settles on the
// standard spring). The scroll position is kept under the id.

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
#include "widgets/media/media_look.h"

namespace facet
{
namespace
{

using internal::ForcedItemState;

// The default thumbnail side, in xxxl spacing steps (the header promises four).
constexpr float kStripSpans = 4.0f;
// The selection ring is this many hairlines wide, inside the thumbnail, with one hairline of gap
// to the picture, so it reads as a ring at every scale and needs no extra room.
constexpr float kStripRingHairlines = 2.0f;

struct MediaStripState
{
    internal::MediaSpringValue scroll;  // pixels from the start of the row
    float target = 0.0f;
    float pressX = 0.0f;
    float pressScroll = 0.0f;
    int pressIndex = -1;
    int lastSelected = -2;              // -2: never drawn, reveal without motion
    bool moved = false;                 // the press became a drag
};

// Where the thumbnails come from: a span or a getter.
struct StripSource
{
    std::span<const ImageItem> items;
    int count = 0;
    ImageItemGetter getter = nullptr;
    void* user = nullptr;

    bool Fetch(int index, ImageItem* out) const
    {
        if (index < 0 || index >= count)
            return false;
        if (getter == nullptr)
        {
            *out = items[static_cast<std::size_t>(index)];
            return true;
        }
        *out = ImageItem{};
        return getter(user, index, out);
    }
};

bool ThumbnailStripImpl(const char* label, int* selected, const StripSource& source, const ThumbnailStripOptions& options)
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
    MediaStripState& state = internal::GetState<MediaStripState>(id);
    const Theme& t = T();
    const Colors& c = t.colors;
    const bool ready = options.state == DataState::Ready;
    const int count = ready ? std::max(0, source.count) : 0;
    const bool readOnly = options.readOnly || selected == nullptr || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;

    // Measure: square thumbnails, room for the focus ring around them, the caption line under them.
    const float thumb = std::max(1.0f, PxSnapped(options.thumbnailHeight > 0.0f ? options.thumbnailHeight
                                                                                : t.spacing.xxxl * kStripSpans));
    const float inset = std::ceil(internal::FocusRingOutset());
    const float gap = PxSnapped(t.spacing.sm);
    const float stride = thumb + gap;
    float captionHeight = 0.0f;
    if (options.captions)
    {
        const internal::ControlTextScope text(TextStyle::Footnote);
        captionHeight = PxSnapped(t.spacing.xs) + std::ceil(ImGui::GetTextLineHeight());
    }
    const float width = std::round(internal::ResolveItemWidth(options.width, next, kFill));
    const ImVec2 size(width, inset * 2.0f + thumb + captionHeight);
    const ImVec2 origin(std::round(window->DC.CursorPos.x), std::round(window->DC.CursorPos.y));
    window->DC.CursorPos = origin;
    const ImRect frame(origin, ImVec2(origin.x + size.x, origin.y + size.y));
    const float content = count > 0 ? static_cast<float>(count) * stride - gap + inset * 2.0f : 0.0f;
    const float maxScroll = std::max(0.0f, content - width);

    internal::ItemOptions itemOptions;
    itemOptions.press = internal::PressMode::Click;
    itemOptions.forced = &forced;
    itemOptions.tabStop = count > 0;
    itemOptions.navigable = count > 0;
    itemOptions.handCursor = count > 0;
    internal::ApplyNextItemData(next);
    const internal::ItemState item = internal::ItemBehavior(id, size, itemOptions);

    const int current = selected != nullptr ? *selected : -1;
    const bool live = !item.disabled && item.forced == ForcedItemState::None && count > 0;
    const bool editable = live && !readOnly;
    const float dt = DeltaTime();
    float scroll = std::clamp(state.scroll.position, 0.0f, maxScroll);
    const auto indexAt = [&](float x) {
        const float local = x - frame.Min.x + scroll - inset;
        if (local < 0.0f)
            return -1;
        const int index = static_cast<int>(local / stride);
        return index < count && local - static_cast<float>(index) * stride <= thumb ? index : -1;
    };

    // The mouse: a press that does not move chooses, a press that moves scrolls.
    int written = -1;
    bool holding = false;
    if (live && item.activated && !item.keyboardPressed && ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        state.pressX = g.IO.MousePos.x;
        state.pressScroll = scroll;
        state.pressIndex = indexAt(g.IO.MousePos.x);
        state.moved = false;
    }
    if (live && item.active && g.ActiveIdSource == ImGuiInputSource_Mouse && ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        const float dx = g.IO.MousePos.x - state.pressX;
        if (!state.moved && std::fabs(dx) >= Px(t.drag.startThreshold))
            state.moved = true;
        if (state.moved)
        {
            holding = true;
            state.target = std::clamp(state.pressScroll - dx, 0.0f, maxScroll);
            internal::MediaSpringHold(state.scroll, state.target, dt);
        }
    }
    if (live && item.released)
    {
        if (state.moved)
        {
            // A released drag keeps its speed and loses it over drag.inertiaTime.
            const float glide = ReduceMotion() ? 0.0f : state.scroll.velocity * t.drag.inertiaTime;
            state.target = std::clamp(state.scroll.position + glide, 0.0f, maxScroll);
        }
        else if (editable && state.pressIndex >= 0 && state.pressIndex == indexAt(g.IO.MousePos.x) &&
                 state.pressIndex != current)
        {
            written = state.pressIndex;
        }
        state.moved = false;
        state.pressIndex = -1;
    }

    // The keys: the arrows move the selection (or, read-only, the row), Home and End jump.
    if (live && item.focused)
    {
        const bool right = internal::MediaTakeKey(id, ImGuiKey_RightArrow);
        const bool left = internal::MediaTakeKey(id, ImGuiKey_LeftArrow);
        const bool home = internal::MediaTakeKey(id, ImGuiKey_Home);
        const bool end = internal::MediaTakeKey(id, ImGuiKey_End);
        if (editable)
        {
            int target = current;
            if (home)
                target = 0;
            else if (end)
                target = count - 1;
            else if (right != left)
                target = current < 0 || current >= count ? (right ? 0 : count - 1)
                                                         : std::clamp(current + (right ? 1 : -1), 0, count - 1);
            if (target != current)
                written = target;
        }
        else if (home || end || right != left)
        {
            state.target = home ? 0.0f : (end ? maxScroll : std::clamp(state.target + (right ? stride : -stride), 0.0f, maxScroll));
        }
    }

    // The wheel scrolls the row when there is more of it than fits.
    if (live && maxScroll > 0.0f)
    {
        const ImVec2 wheel = internal::MediaClaimWheel(id, frame);
        const float turn = wheel.x + wheel.y;
        if (turn != 0.0f)
            state.target = std::clamp(state.target - turn * Px(t.scroll.wheelDistance), 0.0f, maxScroll);
    }

    if (written >= 0)
        *selected = written;
    const int shownSelection = written >= 0 ? written : current;
    // A new selection, from here or from the caller, is scrolled into view.
    if (shownSelection != state.lastSelected && shownSelection >= 0 && shownSelection < count)
    {
        const float left = static_cast<float>(shownSelection) * stride;
        const float right = left + thumb + inset * 2.0f;
        float target = std::clamp(state.target, 0.0f, maxScroll);
        if (left < target)
            target = left;
        else if (right > target + width)
            target = right - width;
        state.target = std::clamp(target, 0.0f, maxScroll);
        if (state.lastSelected == -2)
            state.scroll = {state.target, 0.0f, true};
    }
    state.lastSelected = shownSelection;
    state.target = std::clamp(state.target, 0.0f, maxScroll);
    if (!holding)
        internal::MediaSpringStep(state.scroll, state.target, t.motion.standard, dt);
    scroll = std::round(std::clamp(state.scroll.position, 0.0f, maxScroll));

    // Draw.
    const int hovered = item.hovered && !state.moved ? indexAt(g.IO.MousePos.x)
                        : (item.forced & ForcedItemState::Hovered) != ForcedItemState::None ? forced.part
                                                                                             : -1;
    const internal::MediaText alt = internal::MediaVisibleText(label);
    const float radius = std::min(Px(t.radii.control), thumb * 0.5f);
    ImDrawList* drawList = window->DrawList;
    ImageItem hoveredItem;
    ImageItem selectedItem;
    if (item.visible)
    {
        drawList->PushClipRect(frame.Min, frame.Max, true);
        const ImRect inner(frame.Min.x + inset, frame.Min.y + inset, frame.Max.x - inset, frame.Max.y - inset);
        if (options.state == DataState::Loading)
        {
            for (float x = inner.Min.x; x < inner.Max.x; x += stride)
                internal::DrawSkeleton(drawList, ImVec2(x, inner.Min.y), ImVec2(x + thumb, inner.Min.y + thumb), radius);
        }
        else if (options.state == DataState::Error)
        {
            const char* text = options.errorText != nullptr ? options.errorText : GetStrings().loadFailed;
            internal::MediaDrawMessage(drawList, inner, Icon("error"), text, item.disabled);
        }
        else if (count == 0)
        {
            const char* text = options.emptyText != nullptr ? options.emptyText : GetStrings().empty;
            internal::MediaDrawMessage(drawList, inner, Icon("image"), text, item.disabled);
        }
        else
        {
            const int first = std::max(0, static_cast<int>((scroll - inset) / stride));
            const int last = std::min(count - 1, static_cast<int>((scroll + width) / stride));
            const int focusIndex = shownSelection >= 0 && shownSelection < count ? shownSelection : first;
            const float ring = std::max(1.0f, std::round(HairlineThickness() * kStripRingHairlines));
            const internal::ControlTextScope text(TextStyle::Footnote);
            for (int i = first; i <= last; ++i)
            {
                ImageItem entry;
                if (!source.Fetch(i, &entry))
                    continue;
                const float x = frame.Min.x + inset + static_cast<float>(i) * stride - scroll;
                const ImRect tile(x, frame.Min.y + inset, x + thumb, frame.Min.y + inset + thumb);
                const bool chosen = i == shownSelection;
                ImRect picture = tile;
                if (chosen)
                {
                    StrokeRect(drawList, tile.Min, tile.Max, ToU32(item.disabled ? c.labelTertiary : c.accent), radius, ring);
                    picture.Expand(-(ring + HairlineThickness()));
                }
                const internal::MediaText name = internal::MediaVisibleText(entry.label);
                internal::MediaDrawOptions draw;
                draw.fit = ImageFit::Cover;
                draw.radius = std::max(0.0f, radius - (chosen ? ring + HairlineThickness() : 0.0f));
                internal::MediaDrawImageSlot(drawList, internal::MediaImageOf(entry.image), picture, draw, name.text);
                if (i == hovered && !item.disabled)
                {
                    const bool pressing = item.held && state.pressIndex == i && !state.moved;
                    const ImVec4 wash = pressing ? c.fill[1] : WithAlpha(c.fill[2], c.fill[2].w * item.hoverAmount);
                    FillRect(drawList, picture.Min, picture.Max, ToU32(wash), draw.radius);
                }
                if (options.captions && !name.IsEmpty())
                {
                    const internal::FittedText shown = internal::FitText(name.text, nullptr, thumb);
                    const ImVec4 color = item.disabled ? c.labelTertiary : (chosen ? c.label : c.labelSecondary);
                    drawList->AddText(ImVec2(std::round(tile.GetCenter().x - shown.width * 0.5f), tile.Max.y + PxSnapped(t.spacing.xs)),
                                      ToU32(color), shown.text);
                }
                if (i == focusIndex)
                    internal::DrawFocusRing(drawList, tile, radius, item.focusAmount);
                if (i == hovered)
                    hoveredItem = entry;
                if (chosen)
                    selectedItem = entry;
            }
        }
        drawList->PopClipRect();
    }

    // The visible label part is the tooltip; without captions it names the thumbnail under the
    // pointer, or the chosen one while the strip has keyboard focus.
    if (internal::BeginPopupTooltip(id, frame, item.hovered, item.focusVisible))
    {
        if (!alt.IsEmpty())
            Text(TextStyle::Footnote, "%s", alt.text);
        if (!options.captions)
        {
            const ImageItem& named = item.hovered ? hoveredItem : selectedItem;
            const internal::MediaText name = internal::MediaVisibleText(named.label);
            if (!name.IsEmpty())
                Text(TextStyle::Footnote, "%s", name.text);
        }
        internal::EndPopupTooltip();
    }
    return written >= 0;
}

} // namespace

bool ThumbnailStrip(const char* label, int* selected, std::span<const ImageItem> items, const ThumbnailStripOptions& options)
{
    StripSource source;
    source.items = items;
    source.count = static_cast<int>(items.size());
    return ThumbnailStripImpl(label, selected, source, options);
}

bool ThumbnailStrip(const char* label, int* selected, int itemCount, ImageItemGetter getter, void* user,
                    const ThumbnailStripOptions& options)
{
    StripSource source;
    source.count = getter != nullptr ? itemCount : 0;
    source.getter = getter;
    source.user = user;
    return ThumbnailStripImpl(label, selected, source, options);
}

} // namespace facet
