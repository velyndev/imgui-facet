// ThumbnailPicker: a grid of captioned pictures. The single form picks one (one Tab stop, the
// arrows move in two dimensions); the multi form flips one flag per tile (each tile a Tab stop).
//
// The tiles are the tile grid of icon_picker_parts.h in the page's own window: only the lines in
// view are submitted, so a grid of any size costs the same per frame. A tile shows its image (with
// the image's loading and failed looks), or its icon on a quiet fill, and its caption cut with an
// ellipsis under it. The chosen tiles carry a ring in the accent text color; a locked tile a lock
// badge, and it focuses but is never picked; a disabled tile is dimmed and skipped. Loading shows
// skeleton tiles, Error the error text, an empty Ready list the empty text.

#include "facet/widgets/pickers.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/icons.h"
#include "facet/core/image.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/list_view.h"
#include "internal/loading.h"
#include "internal/popup_host.h"
#include "internal/state_storage.h"
#include "widgets/pickers/icon_picker_parts.h"

namespace facet
{
namespace
{

using internal::ForcedItemState;

// The icon of a tile without an image takes this share of the picture's shorter side.
constexpr float kThumbnailIconShare = 0.35f;
// The lock badge is this share of the picture's shorter side, at most a Large control's icon.
constexpr float kThumbnailBadgeShare = 0.22f;

struct ThumbnailPickerState
{
    internal::PickerGridRows rows;
    int builtCount = -1;
    int builtColumns = -1;
};

struct ThumbnailContext
{
    std::span<const ThumbnailItem> items;
    ImVec2 picture;      // pixels of the picture part of a tile
    float captionGap = 0.0f;
    bool multi = false;
};

void ThumbnailInfo(void* user, int index, internal::PickerGridTileInfo* out)
{
    const ThumbnailItem& item = static_cast<const ThumbnailContext*>(user)->items[static_cast<std::size_t>(index)];
    out->id = item.id;
    out->disabled = item.disabled;
    out->locked = item.locked;
}

void ThumbnailDraw(void* user, const internal::PickerGridTile& tile)
{
    using namespace internal;
    const ThumbnailContext& c = *static_cast<const ThumbnailContext*>(user);
    const ThumbnailItem& item = c.items[static_cast<std::size_t>(tile.index)];
    if (!tile.item.visible)
        return;
    const Theme& t = T();
    const Colors& colors = t.colors;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const float inset = PxSnapped(t.metrics.segmentInset);
    const ImRect picture(tile.rect.Min.x + inset, tile.rect.Min.y + inset, tile.rect.Max.x - inset,
                         tile.rect.Min.y + c.picture.y - inset);
    const float radius = Px(t.radii.element);
    const float shorter = std::min(picture.GetWidth(), picture.GetHeight());
    if (item.image.IsValid())
    {
        ImageOptions image;
        image.fit = ImageFit::Cover;
        image.radius = radius;
        if (tile.disabled)
            image.tint = ScaleAlpha(IM_COL32_WHITE, kDisabledPictureOpacity);
        DrawImage(drawList, item.image, picture.Min, picture.Max, image);
    }
    else
    {
        FillRect(drawList, picture.Min, picture.Max, ToU32(colors.fill[3]), radius);
        if (item.icon.IsValid())
        {
            const float side = std::max(1.0f, std::round(shorter * kThumbnailIconShare));
            DrawIcon(drawList, item.icon, ImVec2(std::round(picture.GetCenter().x - side * 0.5f), std::round(picture.GetCenter().y - side * 0.5f)),
                     side, ToU32(ControlSecondaryColor(tile.disabled)));
        }
    }
    if (tile.chosen)
        StrokeRect(drawList, picture.Min, picture.Max, ToU32(colors.accentText), radius, HairlineThickness() * 2.0f);
    if (item.locked)
    {
        // The lock badge: the lock glyph on an elevated disc at the top-right corner.
        const float badge = std::max(1.0f, std::round(std::min(shorter * kThumbnailBadgeShare, GetControlMetrics(ControlSize::Large).height * 0.5f)));
        const ImVec2 center(picture.Max.x - badge * 0.5f - inset * 2.0f, picture.Min.y + badge * 0.5f + inset * 2.0f);
        drawList->AddCircleFilled(center, badge * 0.5f + inset, ToU32(colors.bgElevated));
        DrawIcon(drawList, Icon("lock"), ImVec2(std::round(center.x - badge * 0.35f), std::round(center.y - badge * 0.35f)),
                 std::round(badge * 0.7f), ToU32(ControlSecondaryColor(tile.disabled)));
    }
    if (c.multi && tile.chosen)
    {
        // The multi form marks a checked tile with a check badge at the top-left corner as well.
        const float badge = std::max(1.0f, std::round(std::min(shorter * kThumbnailBadgeShare, GetControlMetrics(ControlSize::Large).height * 0.5f)));
        const ImVec2 center(picture.Min.x + badge * 0.5f + inset * 2.0f, picture.Min.y + badge * 0.5f + inset * 2.0f);
        drawList->AddCircleFilled(center, badge * 0.5f + inset, ToU32(colors.accent));
        DrawIcon(drawList, Icon("check"), ImVec2(std::round(center.x - badge * 0.35f), std::round(center.y - badge * 0.35f)),
                 std::round(badge * 0.7f), ToU32(colors.onAccent));
    }
    // The caption under the picture, cut with an ellipsis; the whole caption as a tooltip.
    bool cut = false;
    {
        ControlTextScope footnote(TextStyle::Footnote);
        const char* label = item.label != nullptr ? item.label : "";
        const char* end = ImGui::FindRenderedTextEnd(label);
        const FittedText shown = FitText(label, end, tile.rect.GetWidth() - inset * 2.0f);
        cut = shown.truncated;
        drawList->AddText(ImVec2(std::round(tile.rect.GetCenter().x - shown.width * 0.5f), picture.Max.y + c.captionGap),
                          ToU32(ControlLabelColor(tile.disabled)), shown.text);
    }
    if (cut && item.label != nullptr)
        CutTextTooltip(tile.item, item.label, ImGui::FindRenderedTextEnd(item.label), true);
}

bool ThumbnailImpl(const char* label, int* index, std::span<bool> checked, bool multi,
                   std::span<const ThumbnailItem> items, const ThumbnailPickerOptions& options)
{
    using namespace internal;
    ImGuiWindow* window = ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const NextItemData next = TakeNextItemData();
    const ForcedItemRequest forced = TakeForcedItemState();
    const Theme& t = T();
    const ImGuiID id = window->GetID(label);
    ThumbnailPickerState& state = GetState<ThumbnailPickerState>(id);
    const bool readOnly = options.readOnly || (!multi && index == nullptr) || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    const int count = static_cast<int>(items.size());

    // Layout: tiles of the thumbnail size (narrowed to the width) with the caption under them.
    const float width = ResolveItemWidth(options.width, next, kFill);
    const float gap = PxSnapped(t.spacing.sm);
    float tileWidth = std::round(options.thumbnailSize.x > 0.0f ? Px(options.thumbnailSize.x) : Px(t.metrics.thumbnailSize));
    float pictureHeight = std::round(options.thumbnailSize.y > 0.0f ? Px(options.thumbnailSize.y) : tileWidth);
    if (tileWidth > width)
    {
        pictureHeight = std::max(1.0f, std::round(pictureHeight * width / tileWidth));
        tileWidth = width;
    }
    tileWidth = std::max(1.0f, tileWidth);
    int columns = options.columns > 0 ? options.columns : static_cast<int>((width + gap) / (tileWidth + gap));
    columns = std::max(1, columns);
    if (options.columns > 0)
        tileWidth = std::max(1.0f, std::floor((width - gap * (columns - 1)) / columns));
    const float captionGap = PxSnapped(t.spacing.xs);
    const float caption = std::ceil(MeasureText(TextStyle::Footnote, "Ag").y);
    const ImVec2 tile(tileWidth, pictureHeight + captionGap + caption);
    const ImVec2 origin(std::round(window->DC.CursorPos.x), std::round(window->DC.CursorPos.y));

    ImGui::BeginGroup();
    bool written = false;
    bool hovered = false;
    bool focusVisible = false;
    if (options.state != DataState::Ready || count == 0)
    {
        // Loading: skeleton tiles where the tiles will be (one line when the count is unknown);
        // Error and empty: the state text in the room of one line.
        if (options.state == DataState::Loading)
        {
            const int shown = count > 0 ? count : columns;
            const int lines = (shown + columns - 1) / columns;
            for (int i = 0; i < shown; ++i)
            {
                const ImVec2 at(origin.x + static_cast<float>(i % columns) * (tile.x + gap),
                                origin.y + static_cast<float>(i / columns) * (tile.y + gap));
                DrawSkeleton(window->DrawList, at, ImVec2(at.x + tile.x, at.y + pictureHeight), Px(t.radii.element));
                DrawSkeleton(window->DrawList, ImVec2(at.x + tile.x * 0.2f, at.y + pictureHeight + captionGap),
                             ImVec2(at.x + tile.x * 0.8f, at.y + tile.y), Px(t.radii.element));
            }
            ImGui::ItemSize(ImVec2(width, lines * (tile.y + gap) - gap));
        }
        else
        {
            const ImRect room(origin, ImVec2(origin.x + width, origin.y + tile.y));
            ImGui::ItemSize(room.GetSize());
            DrawListState(room, options.state == DataState::Error ? DataState::Error : DataState::Ready,
                          options.emptyText != nullptr ? options.emptyText : GetStrings().empty, options.errorText);
        }
        ImGui::ItemAdd(ImRect(origin, ImVec2(origin.x + width, ImGui::GetCursorScreenPos().y)), id);
        hovered = ImGui::IsItemHovered();
    }
    else
    {
        if (state.builtCount != count || state.builtColumns != columns)
        {
            PickerGridBuildRows(&state.rows, count, columns, nullptr, nullptr);
            state.builtCount = count;
            state.builtColumns = columns;
        }
        ThumbnailContext context{items, ImVec2(tile.x, pictureHeight), captionGap, multi};
        PickerGridDesc grid;
        grid.rows = std::span(state.rows.rows.Data, static_cast<std::size_t>(state.rows.rows.Size));
        grid.tileCount = count;
        grid.columns = columns;
        grid.tile = tile;
        grid.gap = gap;
        grid.width = width;
        grid.chosen = !multi && index != nullptr ? *index : -1;
        grid.multi = multi;
        grid.checked = checked;
        grid.info = &ThumbnailInfo;
        grid.draw = &ThumbnailDraw;
        grid.user = &context;
        grid.readOnly = readOnly;
        grid.live = forced.state == ForcedItemState::None;
        grid.forced = &forced;
        ImGui::PushOverrideID(id);
        const PickerGridResult result = PickerGrid(ImHashStr("##tiles", 0, id), grid);
        ImGui::PopID();
        focusVisible = result.focused >= 0 && KeyboardModality();
        if (result.activated >= 0 && !readOnly)
        {
            if (multi)
            {
                if (result.activated < static_cast<int>(checked.size()))
                {
                    bool& flag = checked[static_cast<std::size_t>(result.activated)];
                    flag = !flag;
                    written = true;
                }
            }
            else if (index != nullptr && *index != result.activated)
            {
                *index = result.activated;
                written = true;
            }
        }
    }
    ImGui::EndGroup();
    const ImRect panel(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    hovered = hovered || (ImGui::IsMouseHoveringRect(panel.Min, panel.Max) && ImGui::IsWindowHovered());
    FinishWidgetGroup(id);
    // The visible label part is the panel's tooltip (pickers.h), on hover and keyboard focus.
    const char* end = ImGui::FindRenderedTextEnd(label);
    if (end != label && BeginPopupTooltip(id, panel, hovered, focusVisible))
    {
        facet::Text(TextStyle::Footnote, "%.*s", static_cast<int>(end - label), label);
        EndPopupTooltip();
    }
    if (written)
        GImGui->LastItemData.StatusFlags |= ImGuiItemStatusFlags_Edited;
    return written;
}

} // namespace

bool ThumbnailPicker(const char* label, int* index, std::span<const ThumbnailItem> items, const ThumbnailPickerOptions& options)
{
    return ThumbnailImpl(label, index, {}, false, items, options);
}

bool ThumbnailPicker(const char* label, std::span<bool> checked, std::span<const ThumbnailItem> items,
                     const ThumbnailPickerOptions& options)
{
    return ThumbnailImpl(label, nullptr, checked, true, items, options);
}

} // namespace facet
