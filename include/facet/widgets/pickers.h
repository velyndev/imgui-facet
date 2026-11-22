#pragma once

// Pickers: color wells, pickers and inputs, swatch palettes, date and time pickers, angle, icon
// and thumbnail pickers, and the file browser.
//
// The declarations follow docs/api-conventions.md; widgets/common.h holds the types shared
// with other families. Every widget of the family is implemented; its entry in
// docs/widgets/pickers.md describes what it does and ends with its status.
//
// Common to every widget here:
//   - Colors are ImVec4 in gamma-encoded sRGB with straight alpha (color.h), everywhere: the
//     edited value, swatches, history and the compare color. Without `alpha` the alpha channel
//     is shown as opaque and never written; without `hdr` channels are clamped to 0 to 1 when
//     written. Pickers that edit hue keep the last hue under their id, so a grey keeps its hue
//     while the saturation is dragged through 0.
//   - The bool result is true on the frame the value was written. Drags, clicks and keys write
//     at once; typed text writes on Enter, Tab or focus loss. Escape during an edit restores the
//     value from activation, writes it and returns true; Escape on a closed field does nothing.
//   - Fields that open a popover (ColorWell, DatePicker, DateRangePicker, TimePicker,
//     IconPicker) open on a click, Enter, Space or Alt+Down, and close on Escape or a click
//     outside; their ForcedItemState::Open look shows the popover.
//   - A null value pointer draws the widget read-only and returns false.
//   - Disabled: ImGui::BeginDisabled. readOnly shows the value, lets it be focused and copied,
//     and never writes.
//   - Lengths are design units (multiplied by Px()).
//   - The visible label part is drawn in front of the control, inside the widget width, by the
//     field-like widgets (ColorWell, ColorInputs, ColorChannelSlider, ColorModeEditor,
//     DatePicker, DateRangePicker, TimePicker, AnglePicker, IconPicker). The panels
//     (ColorPicker, SwatchPalette, SatValSquare, ThumbnailPicker, FileBrowser) do not draw it
//     beside them; it is the tooltip of the whole panel on hover and keyboard focus (rule 6).

#include <cstddef>
#include <span>
#include <string>
#include <vector>

#include <imgui.h>

#include "facet/core/icons.h"
#include "facet/core/image.h"
#include "facet/widgets/common.h"

namespace facet
{

// Recently picked colors, newest first, kept by the caller (saved with its settings if it
// likes). Plain data; the pickers that take a pointer to it add each color they write.
struct ColorHistory
{
    static constexpr int kCapacity = 8;
    ImVec4 colors[kCapacity] = {};
    int count = 0;                      // valid entries at the front, 0 to kCapacity
};

// --- ColorWell --------------------------------------------------------------------------------

struct ColorWellOptions
{
    bool alpha = false;
    bool hdr = false;                   // channels above 1 are allowed
    bool showHex = false;               // the #RRGGBB(AA) text after the swatch
    std::span<const ImVec4> swatches = {};  // read during the call; offered in the popover
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;                 // units; 0 fits the swatch (and the hex text)
    bool readOnly = false;
    bool mixed = false;                 // a striped swatch until the first edit
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// A swatch button (with a checkerboard under translucent colors) that opens a ColorPicker in a
// popover. Ctrl+C and Ctrl+V on the focused well copy and paste the color as hex text; the well
// is a drag and drop source and target of Dear ImGui's color payloads. Returns true when
// *color was written (in the popover, by paste or by a drop). Keeps the popover state under its
// id.
bool ColorWell(const char* label, ImVec4* color, const ColorWellOptions& options = {});

// --- ColorPicker ------------------------------------------------------------------------------

enum class ColorPickerMode
{
    Square,  // a saturation and value square with a hue bar; the default
    Wheel,   // a hue ring around a saturation and value triangle
};

struct ColorPickerOptions
{
    bool alpha = false;
    bool hdr = false;
    ColorPickerMode mode = ColorPickerMode::Square;
    bool inputs = true;                 // ColorInputs under the picker
    const ImVec4* original = nullptr;   // read during the call; a compare swatch that restores it when activated
    std::span<const ImVec4> swatches = {};  // read during the call; a SwatchPalette under the picker
    float width = 0.0f;                 // units; 0 fills up to the default picker width
    bool readOnly = false;
    bool mixed = false;
};

// An inline picker for *color: the square or wheel, a hue bar and an alpha bar, each its own Tab
// stop, moved by drags and arrow keys. Writes while dragging. Returns true when *color was
// written. The visible label part is the panel's tooltip (see the top of this file).
bool ColorPicker(const char* label, ImVec4* color, const ColorPickerOptions& options = {});

// --- ColorInputs ------------------------------------------------------------------------------

enum class ColorModel
{
    Rgb,  // red, green, blue (and alpha); the default
    Hsv,  // hue, saturation, value
    Hsl,  // hue, saturation, lightness
    Hex,  // one #RRGGBB(AA) text field
};

struct ColorInputsOptions
{
    ColorModel model = ColorModel::Rgb;  // shown first; the user's choice is then kept under the id
    bool modeSwitch = true;             // a menu that switches the model
    bool floats = false;                // channels as 0 to 1 instead of 0 to 255 (always with hdr)
    bool alpha = false;
    bool hdr = false;
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;                 // units; 0 fills the available width
    bool readOnly = false;
    bool mixed = false;
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// One drag field per channel of the model (DragNumber behaviour), or one hex field. Pasting
// color text ("#3366ff", "rgb(51, 102, 255)", "0.2, 0.4, 1") into any field parses it. Returns
// true when *color was written.
bool ColorInputs(const char* label, ImVec4* color, const ColorInputsOptions& options = {});

// --- SwatchPalette ----------------------------------------------------------------------------

struct SwatchPaletteOptions
{
    int columns = 0;                    // 0 fits as many swatches as the width allows
    bool alpha = false;                 // compares and writes alpha too
    ControlSize size = ControlSize::Inherit;  // swatch size: the control height of this size
    float width = 0.0f;                 // units; 0 fills the available width
    bool readOnly = false;
    std::span<const char* const> names = {};  // read during the call; name of grid swatch i, its tooltip on hover and keyboard focus; null or missing for none (since 0.1)
};

// A grid of swatches; the one equal to *color shows the accent outline, and a click or Enter on
// a swatch writes its color. `history` (may be null) adds a row of recent colors above the
// grid and receives every color written. One Tab stop; arrow keys move between swatches.
// Returns true when *color was written.
bool SwatchPalette(const char* label, ImVec4* color, std::span<const ImVec4> swatches, ColorHistory* history,
                   const SwatchPaletteOptions& options = {});
// The same with swatches the user edits: a trailing + swatch adds *color (while there is room),
// Delete removes the focused swatch, and dragging reorders. In the caller's storage (rule 5):
// `storage` is the capacity and the first *count entries are the swatches. Returns true when
// *color, the swatches or *count was written. Never allocates.
bool SwatchPalette(const char* label, ImVec4* color, std::span<ImVec4> storage, int* count, ColorHistory* history,
                   const SwatchPaletteOptions& options = {});
// The same in a vector the widget resizes. Allocates only when a swatch is added.
bool SwatchPalette(const char* label, ImVec4* color, std::vector<ImVec4>* swatches, ColorHistory* history,
                   const SwatchPaletteOptions& options = {});

// --- SatValSquare -----------------------------------------------------------------------------

struct SatValSquareOptions
{
    float width = 0.0f;                 // units; 0 fills up to the default picker width; the square keeps its aspect
    bool readOnly = false;
    bool mixed = false;
};

// The saturation and value square of ColorPicker on its own, for the hue of *color. Dragging
// or the arrow keys move the ring and write saturation and value; hue and alpha are kept.
// Returns true when *color was written.
bool SatValSquare(const char* label, ImVec4* color, const SatValSquareOptions& options = {});

// --- ColorChannelSlider -----------------------------------------------------------------------

enum class ColorChannel
{
    Hue,         // the default
    Saturation,  // HSV saturation
    Value,       // HSV value
    Red,
    Green,
    Blue,
    Alpha,
};

struct ColorChannelSliderOptions
{
    ColorChannel channel = ColorChannel::Hue;
    Orientation orientation = Orientation::Horizontal;
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;                 // units, horizontal; 0 fills the available width
    float height = 0.0f;                // units, vertical; 0 for the default fader length
    bool readOnly = false;
    bool mixed = false;
};

// A slider whose track shows the gradient of one channel of *color, with the other channels
// held (a checkerboard under Alpha). Hue wraps at the ends. Writes only that channel, while
// dragging, and with the keys of a Slider. Returns true when *color was written.
bool ColorChannelSlider(const char* label, ImVec4* color, const ColorChannelSliderOptions& options = {});

// --- ColorModeEditor --------------------------------------------------------------------------

enum class ColorAnimation
{
    Static,    // `color` only; the default
    Pulse,     // breathes between `color` and `second`
    Cycle,     // runs through the hue circle from the hue of `color`
    Gradient,  // a sweep from `color` to `second` and back
};

// A color that may change over time, for overlay elements that pulse or cycle. Plain data the
// caller keeps.
struct AnimatedColor
{
    ColorAnimation animation = ColorAnimation::Static;
    ImVec4 color = {};                  // the base color
    ImVec4 second = {};                 // the other color of Pulse and Gradient
    float period = 2.0f;                // seconds per cycle, > 0
};

// The color of `animated` at `seconds` (any running clock, for example ImGui::GetTime()). While
// facet::ReduceMotion() is true (the theme token or the system animation setting) it is `color`.
// No allocation.
ImVec4 SampleAnimatedColor(const AnimatedColor& animated, double seconds);

struct ColorModeEditorOptions
{
    bool alpha = false;
    float width = 0.0f;                 // units; 0 fills the available width
    bool readOnly = false;
    bool mixed = false;
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// Edits an AnimatedColor: a SegmentedControl for the animation, ColorWells for the colors it
// uses, a speed field for the period, and a small live preview. Returns true when any member of
// *animated was written.
bool ColorModeEditor(const char* label, AnimatedColor* animated, const ColorModeEditorOptions& options = {});

// --- DatePicker -------------------------------------------------------------------------------

// A calendar date without time or time zone. A year of 0 means no date.
struct Date
{
    int year = 0;                       // 1 to 9999; 0: no date
    int month = 1;                      // 1 to 12
    int day = 1;                        // 1 to the length of the month

    constexpr bool IsValid() const { return year != 0; }
    friend constexpr bool operator==(const Date&, const Date&) = default;
};

enum class Weekday
{
    Monday,  // the default first day of the week
    Tuesday,
    Wednesday,
    Thursday,
    Friday,
    Saturday,
    Sunday,
};

enum class DatePrecision
{
    Day,    // the default
    Month,  // day is written as 1
    Year,   // month and day are written as 1
};

struct DatePickerOptions
{
    Date min;                           // earliest date that can be picked; no date: unlimited
    Date max;                           // latest date that can be picked; no date: unlimited
    DatePrecision precision = DatePrecision::Day;
    Weekday firstWeekday = Weekday::Monday;
    bool inlineCalendar = false;        // the calendar is drawn in place instead of in a popover
    const char* placeholder = nullptr;  // shown while *date has no date; null uses Strings::select
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;                 // units; 0 fills, or fits the inline calendar (at most the available width)
    bool readOnly = false;
    bool mixed = false;
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// A segmented date field (each part typed or stepped with Up and Down) with a calendar in a
// popover: month and year views, today marked (local time), days outside [min, max] disabled.
// Month names come from Strings::monthJanuary to Strings::monthDecember, the weekday column
// headings from Strings::weekdayMonday to Strings::weekdaySunday, starting at firstWeekday.
// Arrow keys move the day in the calendar, Page Up and Page Down the month, Enter picks.
// Returns true when *date was written. An invalid date (31 February) is shown as typed and
// written only after correction.
bool DatePicker(const char* label, Date* date, const DatePickerOptions& options = {});

// --- DateRangePicker --------------------------------------------------------------------------

// A named range offered beside the calendars. Label first.
struct DateRangePreset
{
    const char* label = nullptr;        // required
    Date start;
    Date end;
};

struct DateRangePickerOptions
{
    Date min;
    Date max;
    Weekday firstWeekday = Weekday::Monday;
    std::span<const DateRangePreset> presets = {};  // read during the call; a list beside the calendars
    const char* placeholder = nullptr;
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;
    bool readOnly = false;
    bool mixed = false;
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// A field showing the range, opening two months side by side. The first pick sets *start, the
// second *end (swapped when earlier); hovering shows the band between them, and the two ends
// can be dragged. Returns true when *start or *end was written (both in the same frame when a
// range is completed or a preset is chosen).
bool DateRangePicker(const char* label, Date* start, Date* end, const DateRangePickerOptions& options = {});

// --- TimePicker -------------------------------------------------------------------------------

// A time of day without date or time zone.
struct TimeOfDay
{
    int hour = 0;                       // 0 to 23
    int minute = 0;                     // 0 to 59
    int second = 0;                     // 0 to 59

    friend constexpr bool operator==(const TimeOfDay&, const TimeOfDay&) = default;
};

struct TimePickerOptions
{
    bool twelveHour = false;            // shows 1 to 12 with a Strings::am and Strings::pm segment
    bool showSeconds = false;
    int minuteStep = 1;                 // the popover columns and Up and Down step minutes by this
    bool inlineColumns = false;         // the columns are drawn in place instead of in a popover
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;
    bool readOnly = false;
    bool mixed = false;
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// A segmented time field with a popover of scrolling hour, minute (and second) columns. Up and
// Down step the focused segment and write at once; typed digits write on Enter, Tab or focus
// loss. Returns true when *time was written.
bool TimePicker(const char* label, TimeOfDay* time, const TimePickerOptions& options = {});

// --- AnglePicker ------------------------------------------------------------------------------

struct AnglePickerOptions
{
    float snapDegrees = 15.0f;          // Shift while dragging snaps to multiples of this
    bool signedRange = false;           // writes -180 to 180 degrees instead of 0 to 360
    bool field = true;                  // a degrees field beside the dial
    float diameter = 0.0f;              // units; 0 derives it from the control height of `size`
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;                 // units; 0 fits label, dial and field, at most the available width
    bool readOnly = false;
    bool mixed = false;
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// A round dial with a needle; *radians is the angle, counter-clockwise from the right (the
// convention of ImGui::SliderAngle values). Dragging points the needle at the pointer; arrow keys
// turn it by one degree (Shift by snapDegrees). Writes while dragging. Returns true when
// *radians was written. Default width: the label, the dial and the degrees field, at most the
// available width. In a narrower width the field shrinks first; the dial keeps its diameter.
bool AnglePicker(const char* label, float* radians, const AnglePickerOptions& options = {});

// --- IconPicker -------------------------------------------------------------------------------

struct IconPickerOptions
{
    bool searchable = true;             // a filter field above the grid, fuzzy matched on names
    bool showRecent = true;             // the icons picked last, kept under the id
    int columns = 0;                    // 0 fits as many as the popover width allows
    const char* placeholder = nullptr;  // shown while *icon is none; null uses Strings::select
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;
    bool readOnly = false;
    bool mixed = false;
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// A field showing *icon and its name that opens a grid of icons in a popover; ChoiceItem::group
// splits the grid into sections. Arrow keys move, Enter or a click picks. Returns true when
// *icon was written. The first form offers every registered icon (IconCount, IconNameAt).
bool IconPicker(const char* label, Icon* icon, const IconPickerOptions& options = {});
bool IconPicker(const char* label, Icon* icon, std::span<const ChoiceItem> icons, const IconPickerOptions& options = {});

// --- ThumbnailPicker --------------------------------------------------------------------------

// One tile of a ThumbnailPicker. Label first, so {"Option 1"} works.
struct ThumbnailItem
{
    const char* label = nullptr;        // caption under the picture; required
    ImageHandle image;                  // shows its loading and failed states; invalid: the icon
    Icon icon;                          // drawn when there is no image
    bool locked = false;                // a lock badge; the tile can be focused but not picked
    bool disabled = false;
    ImGuiID id = 0;                     // stable id for reordered items; 0 uses the index
};

struct ThumbnailPickerOptions
{
    ImVec2 thumbnailSize = {0.0f, 0.0f};  // units; 0 for the default tile size
    int columns = 0;                    // 0 fits as many tiles as the width allows
    DataState state = DataState::Ready;
    const char* emptyText = nullptr;    // null uses Strings::empty
    const char* errorText = nullptr;    // null uses Strings::loadFailed
    float width = 0.0f;                 // units; 0 fills the available width
    bool readOnly = false;
};

// A grid of captioned thumbnails; the picked one shows accentSubtle and the accent outline. The single form is
// one Tab stop with arrow-key movement. The multi form holds checked values, not a selection
// (lists.h): `checked` has one flag per item, each tile is a Tab stop, and a click or Space
// flips its flag, like OptionCardGroup. Returns true when *index or a flag was written. With a count of 0
// in Ready it shows the empty state; Loading shows skeleton tiles; Error shows errorText.
bool ThumbnailPicker(const char* label, int* index, std::span<const ThumbnailItem> items,
                     const ThumbnailPickerOptions& options = {});
bool ThumbnailPicker(const char* label, std::span<bool> checked, std::span<const ThumbnailItem> items,
                     const ThumbnailPickerOptions& options = {});

// --- FileBrowser ------------------------------------------------------------------------------

enum class FileBrowserMode
{
    Open,    // pick an existing file; the default
    Save,    // pick a folder and type a file name, which may not exist yet
    Folder,  // pick a folder
};

// One entry of the FileBrowser type menu. Label first, so {"Images", "*.png;*.jpg"} works.
struct FileType
{
    const char* label = nullptr;        // name shown in the type menu; required
    const char* patterns = nullptr;     // wildcards separated by ";" ("*.png;*.jpg"); null or "" for all files
};

struct FileBrowserOptions
{
    FileBrowserMode mode = FileBrowserMode::Open;
    std::span<const FileType> fileTypes = {};  // read during the call; the first is selected at first; empty: all files
    bool showHidden = false;            // lists hidden and system entries
    float width = 0.0f;                 // units; 0 fills the available width
    float height = 0.0f;                // units; 0 for the default browser height
    bool readOnly = false;              // browsing and selecting work; the confirm button is off and nothing is written
};

// A file browser drawn inside the app, for overlays and full-screen hosts where a system dialog
// cannot be shown. A path bar, a folder list and a file name field. It starts in the folder of
// the current path (or the current directory) and keeps the folder it shows under its id.
// Reading a folder shows the loading state, an unreadable folder the error state with
// Strings::loadFailed, an empty one the empty state. Double click or Enter on a file (or the
// confirm button) writes its full UTF-8 path. Returns true when the path was written. The
// visible label part is the panel's tooltip (see the top of this file); the modal or panel
// around it carries the title. Arrow keys move in the list, Enter opens a folder or confirms a
// file, Backspace goes to the parent folder. Usually shown inside a modal from overlays.h after
// PathFieldBrowsePressed.
bool FileBrowser(const char* label, std::string* path, const FileBrowserOptions& options = {});
// The same with a char buffer for hosts that avoid the heap. A path that does not fit
// `bufferSize` bytes (with the terminating zero) is never written cut short: the confirm button
// stays off for it.
bool FileBrowser(const char* label, char* buffer, std::size_t bufferSize, const FileBrowserOptions& options = {});

} // namespace facet
