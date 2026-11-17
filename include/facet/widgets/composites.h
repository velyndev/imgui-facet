#pragma once

// Composites: editors for curves, easing and gradients, property grids, console views, search
// and filter bars, key bind lists, preset lists, inventory grids, item tiles and slots, item
// detail cards, media cards, user chips, profile switchers, preview canvases, layer lists,
// onboarding, theme controls, settings search and the public drag and drop calls.
//
// Composites are built from the widgets of the other families and from the drag and drop
// core. Their item data always belongs to the caller: it comes in as spans or getters and is
// read during the call. Where a widget edits per-item state, the items come as a non-const
// span and the widget writes only the members documented as written. Where a user action
// would rearrange the caller's data (moving an inventory item, reordering layers), the widget
// reports the action as a result and the caller applies it. Entries get ids under the widget
// id: the item's `id` member when it is non-zero, the index otherwise, so a caller that
// reorders its items sets `id` and the per-entry state (hover, animations) follows the item.
//
// The declarations follow docs/api-conventions.md; widgets/common.h holds the types shared
// with other families, and this header includes the family headers whose types it takes.
// Every widget here is declared and planned: its body draws a placeholder until Phase 2
// implements it (docs/widgets/composites.md lists the status of each). Lengths are design
// units. Every function is for the thread that runs the Dear ImGui frame.

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>

#include <imgui.h>

#include "facet/core/icons.h"
#include "facet/core/image.h"
#include "facet/core/input.h"
#include "facet/core/theme.h"
#include "facet/widgets/common.h"
#include "facet/widgets/display.h"
#include "facet/widgets/media.h"

namespace facet
{

// --- Editors --------------------------------------------------------------------------------

enum class CurveInterpolation
{
    Smooth,  // monotone cubic: no overshoot between points
    Linear,
    Step,    // holds each value until the next point
};

struct CurveEditorOptions
{
    CurveInterpolation interpolation = CurveInterpolation::Smooth;
    bool lockEnds = true;                  // the first and last point stay at min.x and max.x
    bool grid = true;
    float marker = std::numeric_limits<float>::quiet_NaN();  // x of a live marker on the curve (the current input); NaN none
    float width = 0.0f;                    // units; 0 fills the available width
    float height = 0.0f;                   // units; 0 for the editor height default
    bool readOnly = false;
};

// A response curve through control points between `min` and `max` (x and y ranges). `points`
// is the caller's storage: its size is the capacity and *count points are in use, kept sorted
// by x. A double click on empty space adds a point while there is room; Delete or a double
// click on a point removes it (never below two). Each point is a Tab stop with a focus ring;
// arrows move it (Shift fine, Ctrl coarse) and it never passes its neighbours. Writes while
// dragging. Returns true when points or *count were written. min > max is swapped per axis.
bool CurveEditor(const char* label, std::span<ImVec2> points, int* count, ImVec2 min, ImVec2 max,
                 const CurveEditorOptions& options = {});

// A cubic easing curve from (0, 0) to (1, 1) given by its two control points.
struct CubicBezier
{
    ImVec2 p1 = {0.0f, 0.0f};
    ImVec2 p2 = {1.0f, 1.0f};
};

struct BezierEditorOptions
{
    bool presets = true;                   // a menu of common easings
    bool allowOvershoot = false;           // control point y may leave 0 to 1
    bool numericFields = true;             // the four numbers as fields under the graph
    bool preview = true;                   // a dot runs the curve once after each edit; not with reduced motion
    float width = 0.0f;                    // units; 0 fills the available width
    float height = 0.0f;                   // units; 0 for the editor height default
    bool readOnly = false;
};

// An easing editor with the two control points as handles, each a Tab stop moved with arrows.
// x stays in 0 to 1. Writes while dragging. Returns true when *curve was written.
bool BezierEditor(const char* label, CubicBezier* curve, const BezierEditorOptions& options = {});

// One color stop of a gradient; color is sRGB with straight alpha.
struct GradientStop
{
    float position = 0.0f;                 // 0 to 1
    ImVec4 color = {0.0f, 0.0f, 0.0f, 1.0f};
};

struct GradientEditorOptions
{
    bool alpha = false;                    // stops have an alpha channel (checkerboard behind the bar)
    float width = 0.0f;                    // units; 0 fills the available width
    float height = 0.0f;                   // units; 0 for twice the control height
    bool readOnly = false;
};

// A gradient bar with draggable stops. `stops` is the caller's storage as in CurveEditor,
// kept sorted by position (a stop dragged past another changes places with it). A click on
// the bar adds a stop with the color at that point; dragging a stop off the bar or Delete
// removes it (never below one). Activating a stop opens a popover with a color picker. Each
// stop is a Tab stop; arrows move it. Returns true when stops or *count were written.
bool GradientEditor(const char* label, std::span<GradientStop> stops, int* count,
                    const GradientEditorOptions& options = {});

// --- Property grid --------------------------------------------------------------------------

struct PropertyGridOptions
{
    const char* filter = nullptr;          // rows whose label or keywords do not match (fuzzy) are skipped
    float labelWidth = 0.0f;               // units, the label column with its gap; 0 uses T().layout.labelColumn;
                                           // the user can drag the column edge (kept under the id)
};

// An inspector of labelled rows grouped in collapsible categories. Laid out with
// LayoutLabeledRow, not as a Dear ImGui table, so it works inside any container. Content is
// always submitted; EndPropertyGrid is always called.
void BeginPropertyGrid(const char* id, const PropertyGridOptions& options = {});
void EndPropertyGrid();

struct PropertyCategoryOptions
{
    bool defaultOpen = true;               // state before the user first toggles it
};

// A collapsible category header inside BeginPropertyGrid. Returns true while it is open
// (always while a filter is set and a row inside matches); then call EndPropertyCategory. The
// header is a Tab stop; Enter, Space, Left and Right toggle. Open state kept under the label.
[[nodiscard]] bool BeginPropertyCategory(const char* label, const PropertyCategoryOptions& options = {});
void EndPropertyCategory();

struct PropertyRowOptions
{
    const char* description = nullptr;     // tooltip of the label; null for none
    const char* keywords = nullptr;        // extra words the filter matches; null for none
    bool modified = false;                 // marks a value that differs from its default
};

// One row: the visible label part in the label column, the caller's control in the value
// column. Returns false when the grid filter hides the row; otherwise the label is pushed on
// the id stack (the control can use "##value") and EndPropertyRow must be called.
[[nodiscard]] bool BeginPropertyRow(const char* label, const PropertyRowOptions& options = {});
void EndPropertyRow();

class [[nodiscard]] PropertyGridScope
{
public:
    explicit PropertyGridScope(const char* id, const PropertyGridOptions& options = {})
    {
        BeginPropertyGrid(id, options);
    }
    ~PropertyGridScope() { EndPropertyGrid(); }
    PropertyGridScope(const PropertyGridScope&) = delete;
    PropertyGridScope& operator=(const PropertyGridScope&) = delete;
};

class [[nodiscard]] PropertyCategoryScope
{
public:
    explicit PropertyCategoryScope(const char* label, const PropertyCategoryOptions& options = {})
        : open_(BeginPropertyCategory(label, options))
    {
    }
    ~PropertyCategoryScope()
    {
        if (open_)
            EndPropertyCategory();
    }
    PropertyCategoryScope(const PropertyCategoryScope&) = delete;
    PropertyCategoryScope& operator=(const PropertyCategoryScope&) = delete;
    explicit operator bool() const { return open_; }

private:
    bool open_;
};

class [[nodiscard]] PropertyRowScope
{
public:
    explicit PropertyRowScope(const char* label, const PropertyRowOptions& options = {})
        : open_(BeginPropertyRow(label, options))
    {
    }
    ~PropertyRowScope()
    {
        if (open_)
            EndPropertyRow();
    }
    PropertyRowScope(const PropertyRowScope&) = delete;
    PropertyRowScope& operator=(const PropertyRowScope&) = delete;
    explicit operator bool() const { return open_; }

private:
    bool open_;
};

// --- Console --------------------------------------------------------------------------------

struct ConsoleViewOptions
{
    std::span<const char* const> history = {};   // earlier commands, oldest first; Up and Down recall them
    std::span<const char* const> commands = {};  // names for Tab completion (fuzzy matched)
    const char* placeholder = nullptr;     // prompt text while empty; null for none
    bool timestamps = false;               // as in LogViewOptions
    DataState state = DataState::Ready;    // of the log, as in LogViewOptions; the command field stays usable
    const char* emptyText = nullptr;       // shown for 0 entries; null uses Strings::empty
    const char* errorText = nullptr;       // null uses Strings::loadFailed
    float width = 0.0f;                    // units; 0 fills the available width
    float height = 0.0f;                   // units; 0 fills the available height
};

// A LogView over the caller's entries with a command field under it. Typing writes the command
// text per keystroke; Enter with text submits: the function returns true on that frame and the
// text stays, so the caller reads it, runs it, adds it to the history and clears it. Tab
// completes from `commands`; Up and Down walk the history. Keeps the scroll state and the
// history position under the id.
bool ConsoleView(const char* id, std::string* command, std::span<const LogEntry> entries,
                 const ConsoleViewOptions& options = {});
bool ConsoleView(const char* id, std::string* command, int entryCount, LogGetter getter, void* user,
                 const ConsoleViewOptions& options = {});
// Command text in a caller buffer of bufferSize bytes (zero terminated).
bool ConsoleView(const char* id, char* buffer, std::size_t bufferSize, std::span<const LogEntry> entries,
                 const ConsoleViewOptions& options = {});
bool ConsoleView(const char* id, char* buffer, std::size_t bufferSize, int entryCount, LogGetter getter, void* user,
                 const ConsoleViewOptions& options = {});

// --- Search and filters ---------------------------------------------------------------------

// One filter chip of a SearchFilterBar.
struct FilterChip
{
    const char* label = nullptr;
    Icon icon;                             // none by default
    bool active = false;                   // written when the chip is toggled
    ImGuiID id = 0;                        // stable id of the entry's state when entries change order; 0 uses the index
};

struct SearchFilterBarOptions
{
    const char* placeholder = nullptr;     // null uses Strings::search
    int resultCount = -1;                  // shown after the field; < 0 hides it
    float width = 0.0f;                    // units; 0 fills the available width
};

// A search field with toggleable filter chips beside it (or under it when narrow); chips that
// do not fit move into a "more" popover. The query is written per keystroke; Escape in the
// field clears it. Each chip is a Tab stop; Enter and Space toggle. Returns true when the query
// or any chip's active member was written.
bool SearchFilterBar(const char* label, std::string* query, std::span<FilterChip> filters,
                     const SearchFilterBarOptions& options = {});
bool SearchFilterBar(const char* label, char* buffer, std::size_t bufferSize, std::span<FilterChip> filters,
                     const SearchFilterBarOptions& options = {});

struct SettingsSearchMatchOptions
{
    const char* description = nullptr;     // also searched
    const char* keywords = nullptr;        // extra words, space separated
    const char* section = nullptr;         // the section the row belongs to, for SettingsSearchHitCount
};

// Settings search over rows the caller submits: between BeginSettingsSearch and
// EndSettingsSearch, ask SettingsSearchMatch before each settings row and submit the row only
// when it returns true. `query` is the search text (usually from a search field); an empty
// query matches every row. Content is always submitted; EndSettingsSearch is always called.
void BeginSettingsSearch(const char* id, const char* query);
void EndSettingsSearch();
// True when the row should be shown: outside a settings search, for an empty query, or when the
// query matches the label, description or keywords (fuzzy). Counts the hit for its section.
bool SettingsSearchMatch(const char* label, const SettingsSearchMatchOptions& options = {});
// Hits of the settings search `id` in its last completed frame, for one section (null: all),
// so a navigation drawn before the page can show counts.
int SettingsSearchHitCount(const char* id, const char* section);

class [[nodiscard]] SettingsSearchScope
{
public:
    SettingsSearchScope(const char* id, const char* query) { BeginSettingsSearch(id, query); }
    ~SettingsSearchScope() { EndSettingsSearch(); }
    SettingsSearchScope(const SettingsSearchScope&) = delete;
    SettingsSearchScope& operator=(const SettingsSearchScope&) = delete;
};

// --- Key binds and presets ------------------------------------------------------------------

struct KeybindListOptions
{
    const char* filter = nullptr;          // only rows whose label or chord text match (fuzzy)
    bool alternate = true;                 // a second capture slot per row
    bool groups = true;                    // group headers from the group text
    DataState state = DataState::Ready;    // Loading shows skeleton rows, Error the error state
    const char* emptyText = nullptr;       // shown when no row is listed; null uses Strings::empty
                                           // (Strings::noResults while a filter is set)
    const char* errorText = nullptr;       // null uses Strings::loadFailed
    float width = 0.0f;                    // units; 0 fills the available width
    float height = 0.0f;                   // units; 0 fits the rows
};

// The shortcut registry (core/input.h) as an editable table: label, primary and alternate
// chord as KeybindCapture fields, a conflict mark when FindChordConflict finds one, and a
// reset button on rows that differ from their default. Writes with SetShortcutChords. Returns
// true when a binding was written. Rows are Tab stops through their capture fields.
bool KeybindList(const char* id, const KeybindListOptions& options = {});

// One row of a caller-owned key bind list.
struct KeybindListItem
{
    const char* label = nullptr;
    const char* group = nullptr;           // null: no group
    Keybind bind;                          // written when the user rebinds or resets the row
    Keybind defaultBind;                   // the reset target
    ImGuiID id = 0;                        // stable id of the entry's state when entries change order; 0 uses the index
};

// The same table over the caller's binds instead of the registry; conflicts are searched among
// `items`. Returns true when any bind member was written.
bool KeybindList(const char* id, std::span<KeybindListItem> items, const KeybindListOptions& options = {});

// One preset of a PresetList.
struct PresetItem
{
    const char* label = nullptr;
    bool modified = false;                 // unsaved changes marker
    bool builtIn = false;                  // cannot be renamed or deleted
    ImGuiID id = 0;                        // stable id of the entry's state when entries change order; 0 uses the index
};

enum class PresetAction
{
    None,
    Selected,   // *active was written
    Create,     // a new preset named `name` was asked for
    Save,       // save the current settings into preset `index`
    Rename,     // preset `index` gets `name`
    Duplicate,  // copy preset `index`
    Delete,     // delete preset `index` (the user already confirmed)
    Import,     // the caller opens its import flow
    Export,     // export preset `index`
};

struct PresetListResult
{
    PresetAction action = PresetAction::None;
    int index = -1;                        // the preset the action is for, or -1
    const char* name = nullptr;            // Create and Rename: the entered name, valid until the end of the frame
};

struct PresetListOptions
{
    bool importExport = false;             // import and export entries in the menu
    DataState state = DataState::Ready;    // Loading shows skeleton rows, Error the error state; the
                                           // menu keeps create and import
    const char* emptyText = nullptr;       // shown for 0 presets; null uses Strings::empty
    const char* errorText = nullptr;       // null uses Strings::loadFailed
    float width = 0.0f;                    // units; 0 fills the available width
    float height = 0.0f;                   // units; 0 fits the rows
};

// A preset manager: the list with the active preset marked, unsaved markers, and actions in a
// menu (create, save, rename inline, duplicate, delete after confirmation, import, export).
// One Tab stop for the list (arrows move, Enter selects, F2 renames, Delete asks to delete)
// plus the menu button. The caller applies every action to its own data. *active -1 means none.
PresetListResult PresetList(const char* id, int* active, std::span<const PresetItem> presets,
                            const PresetListOptions& options = {});

// --- Inventory ------------------------------------------------------------------------------

// One item in a slot. A null label means an empty slot.
struct InventoryItem
{
    const char* label = nullptr;           // name: tooltip and accessible name
    ImageHandle image;                     // invalid: the icon is drawn instead
    Icon icon;
    int count = 0;                         // > 1 shows the count
    std::uint32_t stackKey = 0;            // items with the same non-zero key merge; 0 never merges
    int maxStack = 0;                      // largest count after a merge; 0 unlimited
    ImU32 tint = 0;                        // category color (data) as a bar at the bottom; 0 none
    bool locked = false;                   // shown with a lock; cannot be dragged or dropped on
    float cooldown = 0.0f;                 // 0 to 1: remaining part of a cooldown, drawn as a sweep
    ImGuiID id = 0;                        // stable id of the item's state (cooldown, count animation) when it
                                           // moves between slots; 0 uses the slot index
};

// The drag and drop payload of inventory widgets, so custom widgets can accept their items.
struct InventoryPayload
{
    ImGuiID source = 0;                    // id of the grid, tile or slot the drag started in
    int slot = -1;                         // slot index in the source; -1 for a tile or slot widget
    int count = 0;                         // items being moved (a split moves part of a stack)
};

// Payload type of inventory drags unless an options struct names another.
inline constexpr const char* kInventoryPayloadType = "facet.inventory";

enum class InventoryAction
{
    None,
    Activated,  // slot `to` was activated (double click, Enter)
    Moved,      // move `count` items from `from` in `source` to the empty slot `to`
    Swapped,    // swap `from` in `source` with `to`
    Merged,     // add `count` items from `from` to the stack in `to`
    Split,      // move `count` items (part of the stack) from `from` to the empty slot `to`
};

// What the caller applies to its data after an inventory widget call. `to` is a slot index of
// the widget that returned it (-1 for a tile or slot widget).
struct InventoryResult
{
    InventoryAction action = InventoryAction::None;
    ImGuiID source = 0;
    int from = -1;
    int to = -1;
    int count = 0;
};

struct InventoryGridOptions
{
    int columns = 0;                       // 0 fits as many slots as the width allows
    float slotSize = 0.0f;                 // units; 0 for twice the xxxl spacing
    const char* payloadType = nullptr;     // null uses kInventoryPayloadType
    DataState state = DataState::Ready;    // Loading shows skeleton slots, Error the error state
    const char* emptyText = nullptr;       // shown for 0 slots; null uses Strings::empty
    const char* errorText = nullptr;       // null uses Strings::loadFailed
    float width = 0.0f;                    // units; 0 fills the available width
};

// A fixed grid of slots. Drag an item onto an empty slot to move it, onto another item to swap
// or merge; Ctrl+drag moves half a stack. One Tab stop: arrows move the selection, Enter
// activates, Ctrl+arrows move the selected item. *selected -1 means none and is written when
// the selection moves. The caller applies the returned action to its slots.
InventoryResult InventoryGrid(const char* id, int* selected, std::span<const InventoryItem> slots,
                              const InventoryGridOptions& options = {});

struct ItemTileOptions
{
    bool selected = false;                 // the accent ring
    bool dragSource = true;                // the tile can be dragged (InventoryPayload)
    const char* payloadType = nullptr;     // null uses kInventoryPayloadType
    float slotSize = 0.0f;                 // units; 0 for twice the xxxl spacing
};

// One item tile outside a grid: image, count, category bar, lock and cooldown. `id` is never
// shown; the item label is the tooltip and accessible name. Returns true on the frame it was
// activated (click, Enter, Space).
bool ItemTile(const char* id, const InventoryItem& item, const ItemTileOptions& options = {});

struct ItemSlotOptions
{
    Icon placeholderIcon;                  // drawn faintly while the slot is empty; none for no icon
    const char* payloadType = nullptr;     // null uses kInventoryPayloadType
    float slotSize = 0.0f;                 // units; 0 for twice the xxxl spacing
};

// A single slot (an equipment slot) that accepts inventory drops and can be dragged from. The
// visible label part names the slot (tooltip while empty). The slot is highlighted while an
// item hovers it. Returns the drop or drag action for the caller to apply.
InventoryResult ItemSlot(const char* label, const InventoryItem& item, const ItemSlotOptions& options = {});

// One stat line of an ItemDetailCard.
struct ItemStat
{
    const char* label = nullptr;
    double value = 0.0;
    double compare = std::numeric_limits<double>::quiet_NaN();  // a value to compare with; NaN shows none
    bool lowerIsBetter = false;            // which direction of the difference is good
};

struct ItemDetailCardOptions
{
    const char* description = nullptr;     // wrapped body text
    std::span<const ItemStat> stats = {};
    const char* statFormat = nullptr;      // printf for stat values; null: "%g"
    std::span<const char* const> tags = {};     // shown as badges
    std::span<const char* const> actions = {};  // buttons at the bottom; the first is primary
    float width = 0.0f;                    // units; 0 for the card width default
};

// A detail card for one item: image, the visible label part as title, description, stats
// with differences, tags and actions. Also usable inside BeginRichTooltip as a hover card.
// Returns the index of the action activated this frame, or -1.
int ItemDetailCard(const char* label, ImageHandle image, const ItemDetailCardOptions& options = {});

// --- Cards and people -----------------------------------------------------------------------

struct MediaCardOptions
{
    const char* detail = nullptr;          // second line in labelSecondary
    std::span<const char* const> badges = {};
    bool selected = false;                 // the accent ring
    ImageFit fit = ImageFit::Cover;
    float width = 0.0f;                    // units; 0 for the card width default; narrow cards lay out horizontally
};

// A card with a cover image, the visible label part as title, a detail line and badges. One
// Tab stop; returns true on the frame it was activated. For a menu, call BeginContextMenu
// right after it.
bool MediaCard(const char* label, ImageHandle cover, const MediaCardOptions& options = {});

struct UserChipOptions
{
    const char* detail = nullptr;          // second line; null for none
    bool showStatus = false;
    Tone status = Tone::Neutral;
    bool compact = false;                  // the avatar alone (the name stays the tooltip)
    ControlSize size = ControlSize::Inherit;
};

// An avatar with a name and a detail line in a pill, for the signed-in account. One Tab stop;
// returns true on the frame it was activated (the caller opens its account popover).
bool UserChip(const char* label, ImageHandle avatar, const UserChipOptions& options = {});

enum class ProfileSwitcherAction
{
    None,
    Switched,  // *current was written
    Add,       // the add entry was activated
    Manage,    // the manage entry was activated
};

struct ProfileSwitcherOptions
{
    const char* addLabel = nullptr;        // an add entry with this text; null for none
    const char* manageLabel = nullptr;     // a manage entry with this text; null for none
    DataState state = DataState::Ready;    // Loading shows skeleton rows, Error the error state; the
                                           // add and manage entries stay usable
    const char* emptyText = nullptr;       // shown for 0 profiles; null uses Strings::empty
    const char* errorText = nullptr;       // null uses Strings::loadFailed
    float width = 0.0f;                    // units; 0 fills the available width
};

// A list of profiles with avatars and a check on the current one. One Tab stop: arrows move,
// Enter and Space switch. *current -1 means none.
ProfileSwitcherAction ProfileSwitcher(const char* id, int* current, std::span<const AvatarItem> profiles,
                                      const ProfileSwitcherOptions& options = {});

// --- Preview canvas and layers --------------------------------------------------------------

struct PreviewCanvasOptions
{
    float gridSpacing = 0.0f;              // reference pixels between grid lines; 0 no grid
    float safeArea = 0.0f;                 // 0 to 0.5: inset of the safe area frame per side; 0 none
    bool centerGuides = true;              // lines through the center while an element is dragged
    float width = 0.0f;                    // units; 0 fills the available width
    float height = 0.0f;                   // units; 0 from the reference aspect ratio
};

// A letterboxed preview of a screen of `referenceSize` reference pixels (the target
// resolution), scaled to fit. Elements inside are placed in reference pixels. Content is
// always submitted; EndPreviewCanvas is always called. A size with a component <= 0 draws an
// empty canvas.
void BeginPreviewCanvas(const char* id, ImVec2 referenceSize, const PreviewCanvasOptions& options = {});
void EndPreviewCanvas();

class [[nodiscard]] PreviewCanvasScope
{
public:
    PreviewCanvasScope(const char* id, ImVec2 referenceSize, const PreviewCanvasOptions& options = {})
    {
        BeginPreviewCanvas(id, referenceSize, options);
    }
    ~PreviewCanvasScope() { EndPreviewCanvas(); }
    PreviewCanvasScope(const PreviewCanvasScope&) = delete;
    PreviewCanvasScope& operator=(const PreviewCanvasScope&) = delete;
};

// The point of an element (and of the canvas) its offset is measured between.
enum class CanvasAnchor
{
    TopLeft,
    Top,
    TopRight,
    Left,
    Center,
    Right,
    BottomLeft,
    Bottom,
    BottomRight,
};

// Placement of a canvas element, owned by the caller (it is what the app saves).
struct CanvasElementState
{
    CanvasAnchor anchor = CanvasAnchor::TopLeft;
    ImVec2 offset = {0.0f, 0.0f};          // reference pixels from the canvas anchor point to the element's
    ImVec2 extent = {0.0f, 0.0f};          // element width and height in reference pixels
};

struct CanvasElementOptions
{
    bool resizable = false;                // edge and corner handles change extent
    bool snap = true;                      // snaps to the canvas center, edges, safe area and other elements
    bool anchorPicker = true;              // a 3 x 3 anchor control while selected; keeps the position on change
};

// A draggable element inside BeginPreviewCanvas, drawn as a labelled box (the visible label
// part); the caller may draw its own preview into the item rect afterwards. One Tab stop;
// arrows move it by one reference pixel, Shift by ten. Writes while dragging. Returns true when
// *element was written.
bool CanvasElement(const char* label, CanvasElementState* element, const CanvasElementOptions& options = {});

// One layer row. `visible` and `locked` are written by the row's toggles.
struct LayerItem
{
    const char* label = nullptr;
    Icon icon;                             // none by default
    int depth = 0;                         // indentation level for grouped layers
    bool visible = true;                   // written
    bool locked = false;                   // written
    ImGuiID id = 0;                        // stable id of the row's state when layers are reordered; 0 uses the index
};

enum class LayerAction
{
    None,
    Selected,    // *selected was written
    Visibility,  // layers[index].visible was written (and others, for solo)
    Lock,        // layers[index].locked was written
    Moved,       // move layer `index` to position `to`; the caller reorders its data
};

struct LayerListResult
{
    LayerAction action = LayerAction::None;
    int index = -1;
    int to = -1;
};

struct LayerListOptions
{
    bool solo = true;                      // Alt+click on a visibility toggle shows only that layer
    DataState state = DataState::Ready;    // Loading shows skeleton rows, Error the error state
    const char* emptyText = nullptr;       // shown for 0 layers; null uses Strings::empty
    const char* errorText = nullptr;       // null uses Strings::loadFailed
    float width = 0.0f;                    // units; 0 fills the available width
    float height = 0.0f;                   // units; 0 fits the rows
};

// Ordered layer rows, topmost first, with visibility and lock toggles; dragging across the
// toggles paints them. Rows reorder by drag (with a drop line) or Alt+Up and Alt+Down. One Tab
// stop for the rows (arrows move the selection) plus each focused row's toggles.
LayerListResult LayerList(const char* id, int* selected, std::span<LayerItem> layers,
                          const LayerListOptions& options = {});

// --- Onboarding -----------------------------------------------------------------------------

struct OnboardingOptions
{
    bool skippable = true;                 // a skip button (Strings::skip) that closes the panel
    float width = 0.0f;                    // units; 0 for the dialog width default
};

// A centered panel over a scrim with paged steps, progress dots and back, next and finish
// buttons (Strings back, next, finish). The caller writes *open = true to show it and draws
// the content of step *step between Begin and End; the buttons write *step, and finish, skip
// and Escape write *open = false. The visible label part is the panel title. Returns false
// while closed; call EndOnboarding only after true. stepCount < 1 counts as 1.
[[nodiscard]] bool BeginOnboarding(const char* label, bool* open, int* step, int stepCount,
                                   const OnboardingOptions& options = {});
void EndOnboarding();

class [[nodiscard]] OnboardingScope
{
public:
    OnboardingScope(const char* label, bool* open, int* step, int stepCount, const OnboardingOptions& options = {})
        : open_(BeginOnboarding(label, open, step, stepCount, options))
    {
    }
    ~OnboardingScope()
    {
        if (open_)
            EndOnboarding();
    }
    OnboardingScope(const OnboardingScope&) = delete;
    OnboardingScope& operator=(const OnboardingScope&) = delete;
    explicit operator bool() const { return open_; }

private:
    bool open_;
};

// --- Theme controls -------------------------------------------------------------------------

struct ThemeModeControlOptions
{
    bool apply = true;                     // calls SetThemeMode when *mode is written
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;                    // units; 0 fits the segments
};

// A segmented control for Light, Dark and System. Returns true when *mode was written.
bool ThemeModeControl(const char* label, ThemeMode* mode, const ThemeModeControlOptions& options = {});

struct AccentPickerOptions
{
    std::span<const AccentPreset> presets = {};  // empty uses AccentPresets(); the first is "theme default"
    bool customColor = true;               // a color well for any color after the presets
    bool apply = true;                     // calls SetAccent (ClearAccent for the first preset) when written
    ControlSize size = ControlSize::Inherit;
};

// A row of accent swatches. One Tab stop: arrows move, Enter and Space choose. Returns true
// when *accent was written.
bool AccentPicker(const char* label, ImVec4* accent, const AccentPickerOptions& options = {});

struct UiScaleControlOptions
{
    float step = 0.25f;                    // scale steps between min and max
    bool keyboardZoom = false;             // Ctrl+=, Ctrl+-, Ctrl+0 and Ctrl+wheel change the scale anywhere
    bool apply = true;                     // calls SetUiScale when written
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;                    // units; 0 fills the available width
};

// A stepped control for the user UI scale between `min` and `max` (1 is 100 percent), shown as
// percent. Returns true when *scale was written. min > max is swapped.
bool UiScaleControl(const char* label, float* scale, float min, float max, const UiScaleControlOptions& options = {});

// --- Drag and drop --------------------------------------------------------------------------

struct DragSourceOptions
{
    const char* previewText = nullptr;     // text of the drag preview; null for none
    Icon icon;                             // icon of the drag preview; none for none
    ImageHandle image;                     // image of the drag preview; invalid for none
};

// Makes the last item a drag source for a payload of `type` (at most 32 bytes, Dear ImGui's
// limit; names starting with '_' are reserved) whose `size` bytes at `data` are copied while
// the drag starts. Returns true while the item is being dragged; then optional extra preview
// content may be submitted, and EndDragSource must be called. The preview is drawn as a
// floating layer in the theme, and lists and scroll areas scroll while it nears their edge.
[[nodiscard]] bool BeginDragSource(const char* type, const void* data, std::size_t size,
                                   const DragSourceOptions& options = {});
void EndDragSource();

// Makes the last item a drop target. Returns true while a payload is dragged over it; then call
// AcceptDrop for each accepted type, and EndDropTarget.
[[nodiscard]] bool BeginDropTarget();

// How a drop target shows where the payload lands.
enum class DropIndicator
{
    Box,         // an accent outline around the target
    LineBefore,  // an accent line before the target (above, or left in a row)
    LineAfter,   // an accent line after the target
    None,
};

struct AcceptDropOptions
{
    DropIndicator indicator = DropIndicator::Box;
    bool peek = false;                     // also returns the payload while it hovers, before the drop
};

// The payload of `type` when it was dropped on the target this frame (or, with peek, while it
// hovers; check ImGuiPayload::IsDelivery), otherwise null. Valid until the end of the frame.
// Call it between a BeginDropTarget that returned true and EndDropTarget; anywhere else it
// returns null.
const ImGuiPayload* AcceptDrop(const char* type, const AcceptDropOptions& options = {});
void EndDropTarget();

class [[nodiscard]] DragSourceScope
{
public:
    DragSourceScope(const char* type, const void* data, std::size_t size, const DragSourceOptions& options = {})
        : open_(BeginDragSource(type, data, size, options))
    {
    }
    ~DragSourceScope()
    {
        if (open_)
            EndDragSource();
    }
    DragSourceScope(const DragSourceScope&) = delete;
    DragSourceScope& operator=(const DragSourceScope&) = delete;
    explicit operator bool() const { return open_; }

private:
    bool open_;
};

class [[nodiscard]] DropTargetScope
{
public:
    DropTargetScope() : open_(BeginDropTarget()) {}
    ~DropTargetScope()
    {
        if (open_)
            EndDropTarget();
    }
    DropTargetScope(const DropTargetScope&) = delete;
    DropTargetScope& operator=(const DropTargetScope&) = delete;
    explicit operator bool() const { return open_; }

private:
    bool open_;
};

} // namespace facet
