#pragma once

// Types and functions shared by the widget families. See docs/api-conventions.md for the rules
// every widget header follows; this header holds what more than one family needs.
//
// Like every Facet function, these are for the thread that runs the Dear ImGui frame (see
// context.h); the plain types may be built on any thread.

#include <cfloat>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>

#include <imgui.h>

#include "facet/core/icons.h"
#include "facet/core/theme.h"
#include "facet/core/version.h"

// Bit operators and Has() for a flags enum class with a None = 0 member. Use it at namespace
// scope, right after the enum.
#define FACET_FLAGS(E)                                                                             \
    constexpr E operator|(E a, E b)                                                                \
    {                                                                                              \
        return E(std::underlying_type_t<E>(a) | std::underlying_type_t<E>(b));                     \
    }                                                                                              \
    constexpr E operator&(E a, E b)                                                                \
    {                                                                                              \
        return E(std::underlying_type_t<E>(a) & std::underlying_type_t<E>(b));                     \
    }                                                                                              \
    constexpr E operator^(E a, E b)                                                                \
    {                                                                                              \
        return E(std::underlying_type_t<E>(a) ^ std::underlying_type_t<E>(b));                     \
    }                                                                                              \
    constexpr E operator~(E a)                                                                     \
    {                                                                                              \
        return E(~std::underlying_type_t<E>(a));                                                   \
    }                                                                                              \
    constexpr E& operator|=(E& a, E b)                                                             \
    {                                                                                              \
        return a = a | b;                                                                          \
    }                                                                                              \
    constexpr E& operator&=(E& a, E b)                                                             \
    {                                                                                              \
        return a = a & b;                                                                          \
    }                                                                                              \
    constexpr bool Has(E set, E bit)                                                               \
    {                                                                                              \
        return bit != E::None && (set & bit) == bit;                                               \
    }

namespace facet
{

// Width value meaning "the available content width" (the same value Dear ImGui uses).
inline constexpr float kFill = -FLT_MIN;

// Progress value meaning "progress unknown": the indicator sweeps instead of filling
// (ProgressBar, ProgressRing, LoadingOverlay, toasts with progress).
inline constexpr float kIndeterminate = -1.0f;

// Control heights from the controlHeight tokens. Inherit takes the innermost PushControlSize,
// or Regular when none is pushed.
enum class ControlSize
{
    Inherit,
    Small,
    Regular,
    Large,
};

// Semantic tone of status, alerts, badges and destructive actions; colors from the status and
// accent tokens.
enum class Tone
{
    Neutral,
    Accent,
    Success,
    Warning,
    Destructive,
};

enum class Orientation
{
    Horizontal,
    Vertical,
};

// Corner radius by layer token (T().radii): None is square, the others name their token.
enum class Radius
{
    None,
    Element,
    Control,
    Card,
    Popover,
    Window,
};

// Alignment of text inside the width it is given (styled text, table columns, labels).
enum class Align
{
    Start,
    Center,
    End,
};

// A corner of the display (the toast stack, a shell's first position, a watermark).
enum class ScreenCorner
{
    BottomRight,
    BottomLeft,
    TopRight,
    TopLeft,
};

// State of a data view. Ready with a count of 0 shows the empty state.
enum class DataState
{
    Ready,
    Loading,
    Error,
};

// A tri-state check (Checkbox, CheckboxTree, a select-all row). Only the caller sets Mixed.
enum class CheckState
{
    Off,
    On,
    Mixed,
};

// How a value range maps onto a track or a drag distance (sliders, knobs, drag fields).
// Logarithmic needs min > 0; with min <= 0 the widget falls back to Linear.
enum class ValueScale
{
    Linear,
    Logarithmic,
};

// One entry of a single-choice widget (RadioGroup, SegmentedControl, Dropdown, TabBar).
struct ChoiceItem
{
    const char* label = nullptr;   // required; "##" hides the visible part as for widget labels
    Icon icon;                     // none by default
    const char* detail = nullptr;  // secondary text where the widget has room; null for none
    const char* group = nullptr;   // consecutive items with equal text share a group header
    bool disabled = false;
    ImGuiID id = 0;                // stable id for reorderable entries; 0 uses the index
};

// Fills `out` for entry `index` and returns true, or returns false to skip it. Called during
// the widget call only.
using ChoiceGetter = bool (*)(void* user, int index, ChoiceItem* out);

// Writes the text for `value` into buffer (bufferSize bytes, zero terminated) and returns its
// length. Called during the widget call only.
using FormatFn = int (*)(char* buffer, int bufferSize, double value, void* user);

// Rows [first, last) to submit in a list or table.
struct RowRange
{
    int first = 0;
    int last = 0;
};

// Inside BeginVirtualList, BeginItemGrid (ranges of tile indices) or BeginDataTable: the next
// range of rows to submit. Returns false when every range was handed out, and outside those
// containers. Extra ranges keep the keyboard focused row alive.
bool NextVisibleRows(RowRange* rows);

// Size used by widgets whose options say ControlSize::Inherit, until the matching
// PopControlSize. Pushing Inherit keeps the current size. Pushes nest up to
// kControlSizeStackDepth deep; deeper pushes are reported and ignored (their pops still
// balance). A pop without a push is reported and ignored, and facet::EndFrame reports sizes
// still pushed and clears them.
inline constexpr int kControlSizeStackDepth = 16;
void PushControlSize(ControlSize size);
void PopControlSize();

// The size a widget with `size` uses: `size` itself, or for Inherit the innermost
// PushControlSize, or Regular. Never Inherit.
ControlSize ResolveControlSize(ControlSize size);

// Height in pixels at the current scale of a control of this size (controlHeightSmall,
// controlHeight or controlHeightLarge, through Px). Inherit is resolved first.
float ControlHeight(ControlSize size);

// Text style of the label in a control of this size: Small uses Footnote, Regular and Large
// use Body. Inherit is resolved first.
TextStyle ControlTextStyle(ControlSize size);

// How a filter query matches, kept by the caller next to its query (FilterField, and the
// filtered composite views). Plain data.
struct FilterSettings
{
    bool matchCase = false;             // otherwise ASCII case-insensitive
    bool wholeWord = false;             // each query word must match a whole word of the text
};

// Whether `text` matches `query` under `settings`: every space-separated word of the query occurs
// in the text (as a whole word with wholeWord, ASCII case-insensitive unless matchCase). An
// empty query matches everything. No allocation; usable from any thread.
bool PassesFilter(std::string_view query, const FilterSettings& settings, std::string_view text);

// Last item: its edit session ended this frame, it was edited during the session, and the
// session was not canceled.
bool IsItemEditCommitted();
// Last item: Escape ended its edit session this frame and restored the starting value.
bool IsItemEditCanceled();

// Built-in UI strings, English by default. Every member is UTF-8 text a widget shows or uses
// as an accessible name. Members with a number in them are templates, not printf formats:
// "{page}" and "{count}" are replaced by the numbers, and anything else (including '%' and
// unknown "{...}") is shown as written, so no translation can break the formatting. Each
// template names the tokens it may use. New members are appended at the end.
struct Strings
{
    const char* mixed = "Mixed";
    const char* loading = "Loading";
    const char* empty = "Nothing to show";
    const char* noResults = "No results";
    const char* loadFailed = "Could not load";
    const char* select = "Select";
    const char* confirm = "OK";
    const char* cancel = "Cancel";
    const char* close = "Close";
    const char* more = "More";
    const char* unbound = "Not set";
    const char* listening = "Press a key";
    const char* clear = "Clear";
    const char* search = "Search";
    const char* copy = "Copy";
    const char* copied = "Copied";
    const char* show = "Show";
    const char* hide = "Hide";
    const char* remove = "Remove";
    const char* selectAll = "Select all";
    const char* reset = "Reset";
    const char* back = "Back";
    const char* next = "Next";
    const char* previous = "Previous";
    const char* finish = "Finish";
    const char* skip = "Skip";
    const char* browse = "Browse";
    const char* today = "Today";
    const char* capsLock = "Caps Lock is on";
    const char* bindHold = "Hold";      // BindMode::Hold
    const char* bindToggle = "Toggle";  // BindMode::Toggle
    const char* bindAlways = "Always";  // BindMode::Always
    const char* bindOff = "Off";        // BindMode::Off
    const char* minimize = "Minimize";  // window buttons (TitleBar)
    const char* maximize = "Maximize";
    const char* restore = "Restore";
    const char* pin = "Pin";            // MiniWindow pin button
    const char* unpin = "Unpin";
    const char* apply = "Apply";        // UnsavedChangesBar buttons
    const char* revert = "Revert";
    const char* pageOf = "Page {page} of {count}";           // Pagination; {page} counts from 1
    const char* unsavedChange = "{count} unsaved change";    // UnsavedChangesBar with one change
    const char* unsavedChanges = "{count} unsaved changes";  // UnsavedChangesBar otherwise
    const char* themeLight = "Light";   // ThemeModeControl segments
    const char* themeDark = "Dark";
    const char* themeSystem = "System";
    const char* monthJanuary = "January";  // DatePicker and DateRangePicker headers
    const char* monthFebruary = "February";
    const char* monthMarch = "March";
    const char* monthApril = "April";
    const char* monthMay = "May";
    const char* monthJune = "June";
    const char* monthJuly = "July";
    const char* monthAugust = "August";
    const char* monthSeptember = "September";
    const char* monthOctober = "October";
    const char* monthNovember = "November";
    const char* monthDecember = "December";
    const char* weekdayMonday = "Mo";   // calendar column headings, as short as the cells
    const char* weekdayTuesday = "Tu";
    const char* weekdayWednesday = "We";
    const char* weekdayThursday = "Th";
    const char* weekdayFriday = "Fr";
    const char* weekdaySaturday = "Sa";
    const char* weekdaySunday = "Su";
    const char* am = "AM";              // TimePicker with twelveHour
    const char* pm = "PM";
    const char* matchCase = "Match case";   // FilterField toggles
    const char* wholeWord = "Whole word";
    const char* aspectLock = "Keep aspect ratio";  // SizeInput lock button
    const char* swap = "Swap";              // SizeInput swap button
    const char* linkValues = "Link values"; // VectorInput lock button
    const char* increase = "Increase";      // steppers (NumberInput and the fields built on it)
    const char* decrease = "Decrease";
    const char* jumpToLatest = "Jump to latest";  // LogView while it no longer follows new entries
    const char* invalidValue = "Not a valid value";  // a number-like field after a typed text it cannot read (since 0.1)
    const char* presets = "Presets";        // the SizeInput presets menu button (since 0.1)
    const char* selectedCount = "{count} selected";  // MultiDropdown count summary, SearchableChecklist (since 0.1)
    const char* add = "Add";                // TransferList button that moves rows to the target list (since 0.1)
    const char* moreCount = "+{count}";     // the chip for items that do not fit: Flow, AvatarGroup (since 0.1)
    const char* collapse = "Collapse";      // collapsible panels, split views, sidebar sections (since 0.1)
    const char* expand = "Expand";          // (since 0.1)
    const char* recent = "Recent";          // recently picked entries: IconPicker, CommandPalette (since 0.1)
    const char* goToPage = "Go to page";    // the field a Pagination gap opens (since 0.1)
    const char* holdToConfirm = "Hold to confirm";  // ConfirmDialog with holdToConfirm (since 0.1)
    const char* sortAscending = "Sort ascending";   // DataTable header menu (since 0.1)
    const char* sortDescending = "Sort descending"; // (since 0.1)
    const char* columns = "Columns";        // the DataTable menu that shows and hides columns (since 0.1)
    const char* original = "Original";      // ColorPicker compare swatch (since 0.1)
    const char* colorRed = "Red";           // color channels: ColorInputs, ColorChannelSlider (since 0.1)
    const char* colorGreen = "Green";       // (since 0.1)
    const char* colorBlue = "Blue";         // (since 0.1)
    const char* colorAlpha = "Alpha";       // (since 0.1)
    const char* colorHue = "Hue";           // (since 0.1)
    const char* colorSaturation = "Saturation";  // (since 0.1)
    const char* colorValue = "Value";       // (since 0.1)
    const char* colorLightness = "Lightness";    // (since 0.1)
    const char* colorModelRgb = "RGB";      // the ColorInputs model menu (since 0.1)
    const char* colorModelHsv = "HSV";      // (since 0.1)
    const char* colorModelHsl = "HSL";      // (since 0.1)
    const char* colorModelHex = "Hex";      // (since 0.1)
    const char* animationStatic = "Static"; // ColorModeEditor segments, one per ColorAnimation (since 0.1)
    const char* animationPulse = "Pulse";   // (since 0.1)
    const char* animationCycle = "Cycle";   // (since 0.1)
    const char* animationGradient = "Gradient";  // (since 0.1)
    const char* period = "Period";          // the ColorModeEditor field for AnimatedColor::period (since 0.1)
    const char* open = "Open";              // FileBrowser confirm button, one per mode (since 0.1)
    const char* save = "Save";              // (since 0.1)
    const char* chooseFolder = "Choose folder";  // (since 0.1)
    const char* fileName = "File name";     // FileBrowser name field (since 0.1)
    const char* fileType = "File type";     // FileBrowser type menu (since 0.1)
    const char* parentFolder = "Parent folder";  // FileBrowser button and Backspace (since 0.1)
    const char* play = "Play";              // Carousel autoplay button (since 0.1)
    const char* pause = "Pause";            // (since 0.1)
    const char* loopStart = "Loop start";   // TimelineScrubber handles (since 0.1)
    const char* loopEnd = "Loop end";       // (since 0.1)
    const char* playhead = "Playhead";      // (since 0.1)
    const char* seconds = "s";              // unit after a time in seconds: the ColorModeEditor period (since 0.1)
    const char* milliseconds = "ms";        // unit after a time in milliseconds: FrameTimeGraph (since 0.1)
    const char* framesPerSecond = "fps";    // unit after a frame rate: FrameTimeGraph (since 0.1)
    const char* minimum = "min";            // FrameTimeGraph statistics, short (since 0.1)
    const char* average = "avg";            // (since 0.1)
    const char* maximum = "max";            // (since 0.1)
    const char* newPreset = "New preset";   // PresetList menu entries (since 0.1)
    const char* rename = "Rename";  // (since 0.1)
    const char* duplicate = "Duplicate";  // (since 0.1)
    const char* deleteAction = "Delete";  // (since 0.1)
    const char* importAction = "Import";  // (since 0.1)
    const char* exportAction = "Export";  // (since 0.1)
    const char* deletePresetTitle = "Delete preset?";  // PresetList confirmation title and message (since 0.1)
    const char* deletePresetMessage = "\"{name}\" will be deleted. This cannot be undone.";  // (since 0.1)
    const char* modified = "Modified";      // unsaved preset and modified property marks (since 0.1)
    const char* builtIn = "Built-in";       // a preset that cannot be renamed or deleted (since 0.1)
    const char* action = "Action";          // KeybindList column headings (since 0.1)
    const char* primaryBind = "Primary";  // (since 0.1)
    const char* alternateBind = "Alternate";  // (since 0.1)
    const char* conflictWith = "Also bound to {name}";  // KeybindList conflict mark (since 0.1)
    const char* resetToDefault = "Reset to default";   // KeybindList row reset button (since 0.1)
    const char* resultCount = "{count} results";       // SearchFilterBar result count (since 0.1)
    const char* resultCountOne = "{count} result";     // the same for one result (since 0.1)
    const char* locked = "Locked";          // inventory items and layers (since 0.1)
    const char* emptySlot = "Empty slot";   // an inventory slot without an item (since 0.1)
    const char* lock = "Lock";              // LayerList lock toggle (since 0.1)
    const char* unlock = "Unlock";  // (since 0.1)
    const char* themeDefaultAccent = "Theme default";  // AccentPicker first swatch and custom well (since 0.1)
    const char* customColor = "Custom color";  // (since 0.1)
    const char* percent = "{percent} %";    // UiScaleControl value (since 0.1)
    const char* zoomIn = "Zoom in";         // UiScaleControl buttons (since 0.1)
    const char* zoomOut = "Zoom out";  // (since 0.1)
    const char* stepOf = "Step {step} of {count}";     // Onboarding dots; {step} counts from 1 (since 0.1)
    const char* easeLinear = "Linear";      // BezierEditor presets (since 0.1)
    const char* ease = "Ease";  // (since 0.1)
    const char* easeIn = "Ease in";  // (since 0.1)
    const char* easeOut = "Ease out";  // (since 0.1)
    const char* easeInOut = "Ease in and out";  // (since 0.1)
    const char* colorStop = "Color stop";   // GradientEditor stop and its popover (since 0.1)
    const char* position = "Position";      // GradientEditor stop position field (since 0.1)
    const char* anchor = "Anchor";          // CanvasElement anchor control and its nine cells (since 0.1)
    const char* anchorTopLeft = "Top left";  // (since 0.1)
    const char* anchorTop = "Top";  // (since 0.1)
    const char* anchorTopRight = "Top right";  // (since 0.1)
    const char* anchorLeft = "Left";  // (since 0.1)
    const char* anchorCenter = "Center";  // (since 0.1)
    const char* anchorRight = "Right";  // (since 0.1)
    const char* anchorBottomLeft = "Bottom left";  // (since 0.1)
    const char* anchorBottom = "Bottom";  // (since 0.1)
    const char* anchorBottomRight = "Bottom right";  // (since 0.1)
};

// Replaces the table. Every string is copied; a null member keeps the English default.
// Allocates only here.
void SetStrings(const Strings& strings);
// The table in use. The pointers stay valid until the next SetStrings.
const Strings& GetStrings();

} // namespace facet
