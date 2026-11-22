// ProfileSwitcher (facet/widgets/composites.h): the current profile in a field-like button that
// opens a popover (overlays.h) with the list of profiles, and the add and manage entries.
//
// The list inside is a list view of the list core (src/internal/list_view.h): one Tab stop, the
// arrows move the focus, Enter, Space or a click switches to the focused profile, typing jumps to a
// name. The current profile is marked by a check and a semibold name, not by color alone. Switching
// (or an add or manage entry) closes the popover, and the popover puts the keyboard focus back on
// the button. Escape closes it without switching. The avatars use the avatar look of the media
// family, so they match every Avatar.

#include "facet/widgets/composites.h"

#include <algorithm>
#include <climits>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/icons.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "facet/widgets/buttons.h"
#include "facet/widgets/layout.h"
#include "facet/widgets/overlays.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/list_view.h"
#include "internal/loading.h"
#include "internal/selection_pill.h"
#include "internal/state_storage.h"
#include "widgets/media/media_look.h"

namespace facet
{
namespace
{

// Frames after opening in which the current profile's row is asked to take the focus.
constexpr int kProfileFocusFrames = 3;

struct ProfileSwitcherState
{
    bool open = false;
    int focusFrames = 0;
};

void ProfileSwitcherEntry(void* user, int index, internal::ListEntryInfo* out)
{
    const auto* profiles = static_cast<const std::span<const AvatarItem>*>(user);
    const AvatarItem& item = (*profiles)[static_cast<std::size_t>(index)];
    out->label = item.label;
    out->id = item.id;
}

// An avatar, the name and the description inside `rect`; returns whether a text was cut.
bool ProfileSwitcherDrawPerson(ImDrawList* drawList, const ImRect& rect, const AvatarItem& item, bool current,
                               bool disabled, float trailing)
{
    const Theme& t = T();
    const float inset = PxSnapped(t.spacing.xs);
    const float diameter = std::max(2.0f, std::round((rect.GetHeight() - inset * 2.0f) * 0.5f) * 2.0f);
    const ImVec2 at(std::round(rect.Min.x), std::round(rect.GetCenter().y - diameter * 0.5f));
    internal::MediaAvatarLook look;
    look.disabled = disabled;
    internal::MediaDrawAvatar(drawList, ImRect(at, ImVec2(at.x + diameter, at.y + diameter)),
                              internal::MediaVisibleText(item.label).text, item.image, look);
    const float x = at.x + diameter + Px(t.spacing.sm);
    const float room = std::max(0.0f, rect.Max.x - trailing - x);
    const char* label = item.label != nullptr ? item.label : "";
    const char* end = ImGui::FindRenderedTextEnd(label);
    const bool detail = item.description != nullptr && item.description[0] != '\0';
    float nameLine = 0.0f;
    float detailLine = 0.0f;
    {
        PushTextStyle(TextStyle::Body);
        nameLine = std::ceil(ImGui::GetTextLineHeight());
        PopTextStyle();
    }
    if (detail)
    {
        PushTextStyle(TextStyle::Footnote);
        detailLine = std::ceil(ImGui::GetTextLineHeight());
        PopTextStyle();
    }
    float y = std::round(rect.GetCenter().y - (nameLine + detailLine) * 0.5f);
    bool cut = false;
    internal::PushPillLabelStyle(TextStyle::Body, current);
    {
        const internal::FittedText shown = internal::FitText(label, end, room);
        cut = shown.truncated;
        drawList->AddText(ImVec2(x, y), ToU32(internal::ControlLabelColor(disabled)), shown.text);
    }
    PopTextStyle();
    if (detail)
    {
        y += nameLine;
        PushTextStyle(TextStyle::Footnote);
        const internal::FittedText shown = internal::FitText(item.description, nullptr, room);
        cut = cut || shown.truncated;
        drawList->AddText(ImVec2(x, y), ToU32(internal::ControlSecondaryColor(disabled)), shown.text);
        PopTextStyle();
    }
    return cut;
}

// Skeleton rows while the profiles load: a round avatar and a name bar each.
void ProfileSwitcherSkeleton(float width, float rowHeight, int rows)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    const Theme& t = T();
    const float inset = PxSnapped(t.spacing.xs);
    const float pad = Px(t.spacing.sm);
    for (int i = 0; i < rows; ++i)
    {
        const ImVec2 at = window->DC.CursorPos;
        ImGui::Dummy(ImVec2(width, rowHeight));
        const float diameter = std::max(2.0f, rowHeight - inset * 2.0f);
        const ImVec2 avatar(at.x + pad, at.y + inset);
        internal::DrawSkeleton(window->DrawList, avatar, ImVec2(avatar.x + diameter, avatar.y + diameter), diameter * 0.5f,
                               true);
        const float lineHeight = MeasureText(TextStyle::Body, "").y;
        const ImVec2 bar(avatar.x + diameter + pad, std::round(at.y + (rowHeight - lineHeight) * 0.5f));
        const float barEnd = std::max(bar.x + 1.0f, at.x + width - pad);
        internal::DrawSkeleton(window->DrawList, bar, ImVec2(barEnd, bar.y + lineHeight), Px(t.radii.element));
    }
}

} // namespace

ProfileSwitcherAction ProfileSwitcher(const char* id, int* current, std::span<const AvatarItem> profiles,
                                      const ProfileSwitcherOptions& options)
{
    using namespace internal;
    ImGuiWindow* window = ControlWindow();
    if (window == nullptr)
        return ProfileSwitcherAction::None;
    if (id == nullptr)
        id = "";
    const NextItemData next = TakeNextItemData();
    const Theme& t = T();
    const Colors& c = t.colors;
    const Strings& strings = GetStrings();
    const ImGuiID switcherId = window->GetID(id);
    const int count = static_cast<int>(std::min<std::size_t>(profiles.size(), static_cast<std::size_t>(INT_MAX)));
    const bool readOnly = options.readOnly || current == nullptr || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    const int chosen = current != nullptr && *current >= 0 && *current < count && options.state == DataState::Ready ? *current : -1;
    ProfileSwitcherState& state = GetState<ProfileSwitcherState>(switcherId);

    // The button: the current profile, or Strings::select without one.
    const ControlMetrics m = GetControlMetrics(ControlSize::Large);
    const bool detail = chosen >= 0 && profiles[static_cast<std::size_t>(chosen)].description != nullptr;
    const float inset = PxSnapped(t.spacing.xs);
    const float lines = MeasureText(TextStyle::Body, "").y + (detail ? MeasureText(TextStyle::Footnote, "").y : 0.0f);
    const float height = std::max(m.height, std::ceil(lines + inset * 2.0f));
    const float width = ResolveItemWidth(options.width, next, kFill);
    ApplyNextItemData(next);
    const ItemState button = ItemBehavior(switcherId, ImVec2(width, height));
    if (button.pressed && !button.disabled)
    {
        state.open = !state.open;
        if (state.open)
            state.focusFrames = kProfileFocusFrames;
    }
    bool cut = false;
    if (button.visible)
    {
        ImDrawList* drawList = window->DrawList;
        DrawFieldFrame(drawList, button.rect,
                       {.hoverAmount = button.hoverAmount, .active = state.open, .disabled = button.disabled, .readOnly = readOnly});
        const ImRect inner(button.rect.Min.x + m.fieldPadding, button.rect.Min.y, button.rect.Max.x - m.fieldPadding,
                           button.rect.Max.y);
        const float chevron = std::floor(MeasureText(TextStyle::Body, "").y);
        DrawControlIcon(drawList, Icon("chevron-down"), inner.Max.x - chevron, inner.GetCenter().y, chevron,
                        ToU32(ControlSecondaryColor(button.disabled)));
        if (chosen >= 0)
        {
            cut = ProfileSwitcherDrawPerson(drawList, inner, profiles[static_cast<std::size_t>(chosen)], false, button.disabled,
                                            chevron + m.iconGap);
        }
        else
        {
            PushTextStyle(TextStyle::Body);
            drawList->AddText(ImVec2(inner.Min.x, CenteredTextY(inner.Min.y, inner.GetHeight())),
                              ToU32(ControlSecondaryColor(button.disabled)), strings.select);
            PopTextStyle();
        }
        DrawItemFocusRing(button, m.radius);
    }
    if (chosen >= 0)
    {
        const AvatarItem& item = profiles[static_cast<std::size_t>(chosen)];
        const char* label = item.label != nullptr ? item.label : "";
        CutTextTooltip(button, label, ImGui::FindRenderedTextEnd(label), cut);
    }

    // The popover with the list, anchored to the button.
    ProfileSwitcherAction action = ProfileSwitcherAction::None;
    bool closed = false;  // closed by a choice in the list: the focus goes back to the button
    if (Has(button.forced, ForcedItemState::Open))
        ForceNextItemState(ForcedItemState::Open);
    ImGui::PushID(id);
    if (BeginPopover("##profiles", &state.open, {.placement = PopoverPlacement::Below, .arrow = false, .width = width / Scale()}))
    {
        const float rowHeight = ListRowHeight(ControlSize::Large);
        const float listWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x);
        if (options.state == DataState::Loading)
        {
            ProfileSwitcherSkeleton(listWidth, rowHeight, std::clamp(count, 1, t.list.popupRows));
        }
        else
        {
            const int visible = std::clamp(count, 1, t.list.popupRows);
            ListViewDesc desc;
            desc.rowCount = count;
            desc.size = ControlSize::Large;
            desc.areaSize = ImVec2(listWidth, rowHeight * static_cast<float>(visible) + FocusRingOutset() * 2.0f);
            desc.readOnly = readOnly;
            desc.entry = &ProfileSwitcherEntry;
            desc.user = &profiles;
            desc.state = options.state;
            desc.emptyText = options.emptyText;
            desc.errorText = options.errorText;
            desc.revealRow = state.focusFrames == kProfileFocusFrames ? chosen : -1;
            const ImGuiID listId = ImGui::GetID("##list");
            if (BeginListView(listId, desc))
            {
                RowRange rows;
                while (NextListRows(&rows))
                {
                    for (int i = rows.first; i < rows.last; ++i)
                    {
                        const ListRowState row = ListViewRow(i);
                        const AvatarItem& item = profiles[static_cast<std::size_t>(i)];
                        const bool isCurrent = i == chosen;
                        if (isCurrent && state.focusFrames > 0 && !row.focused && !row.disabled)
                        {
                            ImGui::SetFocusID(row.item.id, ImGui::GetCurrentWindow());
                            ImGui::SetNavCursorVisible(KeyboardModality());
                        }
                        ImDrawList* drawList = ImGui::GetWindowDrawList();
                        const float check = std::floor(MeasureText(TextStyle::Body, "").y);
                        if (isCurrent)
                            DrawControlIcon(drawList, Icon("check"), row.content.Max.x - check, row.content.GetCenter().y, check,
                                            ToU32(row.disabled ? c.labelTertiary : c.accentText));
                        const bool rowCut = ProfileSwitcherDrawPerson(drawList, row.content, item, isCurrent, row.disabled,
                                                                      check + Px(t.spacing.sm));
                        const char* label = item.label != nullptr ? item.label : "";
                        CutTextTooltip(row.item, label, ImGui::FindRenderedTextEnd(label), rowCut);
                        if (!readOnly && !row.disabled && (row.item.pressed || row.activated))
                        {
                            if (i != *current)
                            {
                                *current = i;
                                action = ProfileSwitcherAction::Switched;
                            }
                            state.open = false;
                            closed = true;
                        }
                    }
                }
                (void)EndListView();
            }
        }
        if (options.addLabel != nullptr || options.manageLabel != nullptr)
        {
            Separator();
            ImGui::BeginDisabled(readOnly);
            if (options.addLabel != nullptr &&
                Button(options.addLabel, {.variant = ButtonVariant::Plain, .icon = Icon("plus"), .width = kFill}))
            {
                action = ProfileSwitcherAction::Add;
                state.open = false;
                closed = true;
            }
            if (options.manageLabel != nullptr &&
                Button(options.manageLabel, {.variant = ButtonVariant::Plain, .icon = Icon("settings"), .width = kFill}))
            {
                action = ProfileSwitcherAction::Manage;
                state.open = false;
                closed = true;
            }
            ImGui::EndDisabled();
        }
        EndPopover();
    }
    ImGui::PopID();
    if (closed && !button.disabled)
    {
        // Also after a click in the list, which moved the focus into the closing popover.
        ImGui::SetFocusID(switcherId, window);
    }
    if (state.focusFrames > 0)
        --state.focusFrames;
    if (action == ProfileSwitcherAction::Switched)
        MarkWidgetEdited(switcherId);
    return action;
}

} // namespace facet
