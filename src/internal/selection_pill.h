#pragma once

// Selection pill: Facet's identity motif. One highlight sits behind the selected entry of a
// group (tabs, the sidebar, a segmented control, a list) and slides with a spring to the newly
// selected entry when the selection changes, stretching a little along the way: the edge in the
// direction of travel leads, the other follows, and both settle on the new entry. With reduced
// motion it moves at once.
//
// Usage, every frame around the entries of the group:
//
//     SelectionPill pill;
//     pill.Begin(groupId, ImGui::GetWindowDrawList(), origin);
//     for (each entry)
//     {
//         const ItemState item = ItemBehavior(entryId, size);
//         if (selected) pill.Target(item.rect);
//         PushPillLabelStyle(TextStyle::Body, selected);   // semibold when selected
//         ... draw the label: it lands above the pill ...
//         PopTextStyle();
//     }
//     pill.End(ToU32(T().colors.accentSubtle), Px(T().radii.control));
//
// Begin splits the draw list in two channels and makes the upper one current, so the entries
// draw above the pill, which End draws into the lower one before merging. The splitters are
// kept between frames (no allocation once warm) for up to kMaxPillDepth pills nested inside one
// another.
//
// Coordinates: the pill animates in coordinates relative to `origin`, a point that moves with
// the group's content (for a scrolling list: the window position minus the scroll), so
// scrolling the group moves the pill with its entry instead of animating it, and a layout change
// that moves the selected entry (a resize) is followed by the spring. A group without a target
// in a frame draws no pill; when a target appears again, the pill starts on it (nothing slides
// in from a stale place). State is kept per group id in the widget state storage.

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/theme.h"

namespace facet::internal
{

inline constexpr int kMaxPillDepth = 4;

class SelectionPill
{
public:
    SelectionPill() = default;
    ~SelectionPill();
    SelectionPill(const SelectionPill&) = delete;
    SelectionPill& operator=(const SelectionPill&) = delete;

    // Starts the group. `origin` in screen pixels, see the top of this file.
    void Begin(ImGuiID groupId, ImDrawList* drawList, ImVec2 origin);
    // The rectangle (screen pixels) of the selected entry this frame. The last call wins.
    void Target(const ImRect& rect);
    // Draws the pill behind the entries in `color` with corner `radius` (pixels), and merges
    // the draw list. Returns the rectangle drawn (empty when there was no target).
    ImRect End(ImU32 color, float radius);

    // The rectangle the pill was drawn at in the last End, screen pixels.
    const ImRect& Drawn() const { return drawn_; }
    // True while the pill is still moving toward its target.
    bool Moving() const { return moving_; }

private:
    ImGuiID id_ = 0;
    ImDrawList* drawList_ = nullptr;
    ImDrawListSplitter* splitter_ = nullptr;
    ImVec2 origin_;
    ImRect target_;
    bool hasTarget_ = false;
    bool open_ = false;
    bool moving_ = false;
    ImRect drawn_;
};

// The spring of the edge that leads in the direction of travel: the standard spring with this
// share of its response, so it arrives first and the pill stretches on the way.
inline constexpr float kPillLeadingResponse = 0.7f;

// Pushes the font for the label of an entry in a pill group: the font and size of `style`, in
// semibold (or bolder, when the style already is) when `selected`. The pill is a soft tint whose
// contrast to the surface and to a hovered entry is low by design; the heavier label is the cue
// that does not depend on color. Every group with a pill draws its labels this way. Pair with
// facet::PopTextStyle.
void PushPillLabelStyle(TextStyle style, bool selected);

} // namespace facet::internal
