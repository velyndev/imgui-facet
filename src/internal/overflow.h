#pragma once

// Overflow fitter: which items of a row stay visible in the available width, and which move
// into an overflow list behind a "more" button (toolbars, tab bars, breadcrumbs, chip groups,
// status bars, menu bars).
//
// Pure logic: no Dear ImGui context is needed, so the decisions are unit tested directly.
// Items keep their order; only the set of visible ones changes. When everything fits, every
// item shows and there is no button. Otherwise the width of the button (plus one gap) is
// reserved and items are hidden one by one, lowest priority first; among equal priorities the
// item furthest to the end goes first, so a row of equal items loses its tail. An item the
// caller must keep (the selected tab, the current page of a breadcrumb) gets a higher priority
// than the rest.
//
// Stability: a width that changes slowly (a window being resized) must not make an item blink
// in and out at the threshold. Hiding happens as soon as an item does not fit, but a hidden
// item comes back only when it fits with `hysteresis` pixels to spare. The caller keeps one
// OverflowState per row between frames (in widget state storage) for that.
//
//     facet::internal::OverflowItem items[kCount];          // widths measured this frame
//     bool visible[kCount];
//     OverflowState& state = GetState<OverflowState>(rowId);
//     const OverflowResult fit = FitOverflow(items, availableWidth, gap, moreButtonWidth,
//                                            Px(T().spacing.lg), &state, visible);
//     for (int i = 0; i < kCount; ++i) if (visible[i]) ... draw item i ...
//     if (fit.showButton) ... draw the "more" button listing the items with !visible[i] ...
//
// Groups: entries of kind Separator between items divide a row into groups. When the items on
// one side of a separator are all hidden, the separator goes too, so the row never ends in a
// separator before the button or shows two in a row. A flexible spacer (ToolbarSpacer) has no
// minimum width and stays out of the fitter; the caller places it in the room that is left.
//
// No heap allocation for up to kOverflowInlineItems entries; longer rows allocate a scratch
// array per call.

#include <span>

namespace facet::internal
{

inline constexpr int kOverflowInlineItems = 64;

enum class OverflowKind
{
    Item,       // a button, tab or crumb: shown, or moved into the overflow list
    // A divider between groups (a toolbar separator). It is never ranked, hidden on its own or
    // listed in the overflow; it shows only with a visible item on each side, one per pair of
    // neighbours, never at either end of the row. Its width and gaps count only while it shows.
    Separator,
};

struct OverflowItem
{
    float width = 0.0f;  // pixels; negative counts as 0
    int priority = 0;    // higher stays visible longer; unused for separators
    OverflowKind kind = OverflowKind::Item;
};

// Kept by the caller from one frame to the next. A default value means "nothing hidden yet".
struct OverflowState
{
    int hidden = 0;  // items hidden in the last decision
};

struct OverflowResult
{
    int hiddenCount = 0;      // items in the overflow list (separators never count)
    bool showButton = false;  // true exactly when hiddenCount > 0
    float usedWidth = 0.0f;   // visible items and separators, the gaps between them and the button
};

// Decides which items stay visible in `available` pixels. `gap` is the space between two
// neighbours (and between the last visible item and the button); `buttonWidth` is the width of
// the overflow button. A hidden item returns only when the row fits with `hysteresis` pixels to
// spare (0 disables the hysteresis). `state` may be null for a one-off decision. `visible`
// receives one flag per item and must be at least items.size() long; extra entries are left
// alone. When even the button alone does not fit, every item is hidden and the button shows.
OverflowResult FitOverflow(std::span<const OverflowItem> items, float available, float gap, float buttonWidth,
                           float hysteresis, OverflowState* state, std::span<bool> visible);

// The width a row needs with the `hidden` lowest-ranked items moved into the overflow list,
// including the button when hidden > 0. Exposed for tests and for widgets that size a
// container to its content.
float OverflowRequiredWidth(std::span<const OverflowItem> items, int hidden, float gap, float buttonWidth);

} // namespace facet::internal
