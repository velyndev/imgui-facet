#pragma once

// Parts shared by the navigation widgets (facet/widgets/navigation.h): the count and detail
// badge of tabs and sidebar items, the record of where each part of a widget was drawn (for
// selftests and tests, which click the part they mean instead of a pixel offset), and the roving
// focus of bars whose entries run their item behaviour one after the other, some of them in calls
// of their own (the sidebar, the menu bar, breadcrumbs, pagination, the step indicator). Every name
// carries the family, because every family links into the same library.
//
// Part records, right after a part ran its item behaviour:
//
//     NavigationRecordPart(barId, i, tab.rect);                       // tab i
//     NavigationRecordPart(barId, NavigationClosePart(i), closeRect); // its close button
//     ...
//     const ImRect rect = NavigationPartRect(barId, 2);               // latest frame
//
// Roving focus across calls (one Tab stop for the whole bar, arrows inside it):
//
//     NavigationRoving& roving = GetState<NavigationRoving>(barId);
//     roving.BeginFrame();                                // in the Begin of the bar, once per frame
//     itemOptions.tabStop = roving.IsTabStop(entryId);    // for each entry, before its item
//     const ItemState entry = ItemBehavior(entryId, size, itemOptions);
//     roving.Add(entryId, !entry.disabled, selected);
//     if (entry.focused) roving.Step(entryId, false);     // Left and Right (true: Up and Down)

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/widgets/common.h"
#include "internal/item.h"

namespace facet::internal
{

// ---- Part records ---------------------------------------------------------------------------

// Part numbers of the navigation widgets. Entries (tabs, sidebar items, crumbs, page buttons,
// steps) are their index; the other parts have a number of their own.
inline constexpr int kNavigationPartMore = 100000;        // a "more" button
inline constexpr int kNavigationPartPrevious = 100001;    // Pagination: the previous button
inline constexpr int kNavigationPartNext = 100002;        // Pagination: the next button
inline constexpr int kNavigationPartBack = 100003;        // WizardFooter: Back
inline constexpr int kNavigationPartAdvance = 100004;     // WizardFooter: Next or Finish
inline constexpr int kNavigationPartCancel = 100005;      // WizardFooter: Cancel
inline constexpr int kNavigationPartClose = 100006;       // ShortcutSheet: the close button
inline constexpr int kNavigationPartFilter = 100007;      // ShortcutSheet: the filter field; Pagination: the gap's field
inline constexpr int kNavigationPartEmpty = 100008;       // ShortcutSheet: the Strings::empty state (rows are their index)
inline constexpr int kNavigationPartNoResults = 100009;   // ShortcutSheet: the Strings::noResults state
inline constexpr int kNavigationPartCloseBase = 200000;   // TabBar: the close button of tab i is base + i
inline constexpr int kNavigationPartGapBase = 300000;     // Pagination: gap 0 (before the current page) and 1 (after)
inline constexpr int kNavigationPartMenuRowBase = 400000; // a "more" menu: the row of entry i is base + i

inline constexpr int NavigationClosePart(int index)
{
    return kNavigationPartCloseBase + index;
}
inline constexpr int NavigationMenuRowPart(int index)
{
    return kNavigationPartMenuRowBase + index;
}

// Records where `part` of the widget `widgetId` was drawn this frame (screen pixels).
void NavigationRecordPart(ImGuiID widgetId, int part, const ImRect& rect);
// The rectangle `part` of `widgetId` was drawn at in the latest frame that drew it; empty when it
// was not drawn in this frame or the one before (it moved into a menu, the widget is gone).
ImRect NavigationPartRect(ImGuiID widgetId, int part);

// ---- Badges ---------------------------------------------------------------------------------

// The badge after a tab label or a sidebar item: Caption text in a rounded fill (radii.element),
// one Caption line plus spacing.xs tall. Neutral is the grey wash with label text; the other tones
// are the tone's solid fill with its text color, so a count that asks for attention stands out.
struct NavigationBadgeMetrics
{
    float width = 0.0f;   // pixels
    float height = 0.0f;  // pixels
};

// The size of a badge showing [text, end) (end null: zero terminated).
NavigationBadgeMetrics MeasureNavigationBadge(const char* text, const char* end = nullptr);
// Draws it with its left edge at `x`, centered on `centerY`.
void DrawNavigationBadge(ImDrawList* drawList, float x, float centerY, const char* text, const char* end, Tone tone,
                         bool disabled);

// Writes `count` as decimal text ("12") into `buffer` (zero terminated) and returns its length.
int FormatNavigationCount(int count, char* buffer, int bufferSize);

// ---- Roving focus across calls ------------------------------------------------------------------

// The entries of a bar as the last frame submitted them, so the entry that is the bar's one Tab
// stop can be told at each entry's call, and an arrow key can move the focus to an entry that is
// submitted later in the frame or by another call.
inline constexpr int kNavigationRovingMax = 128;

struct NavigationRoving
{
    ImGuiID ids[kNavigationRovingMax] = {};
    bool enabled[kNavigationRovingMax] = {};
    int count = 0;
    ImGuiID stop = 0;           // the Tab stop decided for this frame
    ImGuiID selectedNext = 0;   // the selected entry of this frame so far
    ImGuiID last[kNavigationRovingMax] = {};
    bool lastEnabled[kNavigationRovingMax] = {};
    int lastCount = 0;
    ImGuiID lastSelected = 0;
    int frame = -1;
    int stepFrame = -1;         // the frame a key last moved the focus

    // Call once per frame before the entries: the last frame's entries become the reference and the
    // Tab stop is decided from them (the focused entry, else the selected one, else the first
    // enabled one).
    void BeginFrame();
    // Whether `id` is the Tab stop this frame. An entry the bar has not seen before is a Tab stop
    // when no entry was known yet.
    bool IsTabStop(ImGuiID id) const;
    // Records an entry in submission order.
    void Add(ImGuiID id, bool isEnabled, bool selected);
    // For the focused entry `id`: takes the keys of `vertical` (Up and Down, else Left and Right)
    // plus Home and End, and moves the focus to the neighbour in the last frame's order. Returns
    // the id the focus moved to, or 0. One step per frame: the entry that takes the focus may come
    // later in the same frame and sees the same key press. Ctrl, Alt and Win leave the keys alone.
    ImGuiID Step(ImGuiID id, bool vertical);
};

} // namespace facet::internal
