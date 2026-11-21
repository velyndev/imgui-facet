# Pickers

Header: `facet/widgets/pickers.h`. The declarations follow [the API conventions](../api-conventions.md);
the comment above each declaration in the header is the full reference. Each entry below ends with
the widget's status. A **planned** widget exists as a function that draws a placeholder of the
widget's default size, returns the neutral result and writes nothing; a **ready** one works as its
header describes. See [the widget index](../widgets.md). The demo gallery shows the color pickers on
the page Color pickers and the others on the page Other pickers.

Colors are `ImVec4` in gamma-encoded sRGB with straight alpha everywhere (see `core/color.h`): the
edited value, swatches, history and the compare color. Without `alpha` the alpha channel is never
written; without `hdr` channels are clamped to 0 to 1. Fields that open a popover open on a click,
Enter, Space or Alt+Down and close on Escape or a click outside. Every widget here returns true on
the frame it wrote its value.

## Color pickers

The color pickers share one model: HSV and HSL over the gamma-encoded sRGB channels, which is also the
space their gradients are drawn in, so the color under the centre of a handle is the color the
handle stands for. A picker keeps the last hue (and saturation) under its id, so a color dragged
through grey or black keeps its hue. A part writes only what it edits: an edit of one channel leaves
the other channels with their exact float values, and a color that is not touched is never written,
so a color survives a round trip through every picker unchanged. Handles are filled with their
color and carry an inner ring in white or black (whichever reads better on that color) with a
hairline of the other one outside it, so they read on any color in both themes. Disabled pickers
show their colors dimmed and the handles in the disabled label color.

### ColorWell

```cpp
bool ColorWell(const char* label, ImVec4* color, const ColorWellOptions& options = {});
```

A field-like swatch button: the label in front, then a field frame with the swatch (a checkerboard
under translucent colors, the opaque color in its left half) and, with `showHex`, the `#RRGGBB` or
`#RRGGBBAA` text. A click, Enter, Space or Alt+Down opens a ColorPicker in a popover below the well
(flipped above when there is no room), with the color from before the opening as its compare color
and `swatches` under it. The popover follows the well while it scrolls. Escape closes it and
restores the color from before it opened; a click outside closes it and keeps the edit; the focus
goes back to the well. Ctrl+C on the focused well copies the color as hex text, Ctrl+V pastes any
color text that ColorInputs reads. The well is a drag and drop source and target of Dear ImGui's
`IMGUI_PAYLOAD_TYPE_COLOR_3F` and `_4F` payloads. Three control sizes; the default width fits the
label, the swatch and the hex text. `readOnly` shows the color, copies it and opens nothing;
`mixed` shows a striped swatch (and Strings::mixed as the hex text) until the first edit; `error`
and `helpText` go under the frame. Status: ready.

### ColorPicker

```cpp
bool ColorPicker(const char* label, ImVec4* color, const ColorPickerOptions& options = {});
```

An inline panel, from the top: the saturation and value square (or, with `ColorPickerMode::Wheel`,
a hue ring around a saturation and value triangle with the pure hue, white and black at its
corners), the hue bar (Square mode), the alpha bar (with `alpha`), the compare strip (with
`original`: the color now beside the original, whose half restores it when clicked or activated
with Enter or Space, Strings::original as its tooltip), ColorInputs (with `inputs`) and a
SwatchPalette of `swatches`. Every part is a Tab stop. The square and the triangle move on both
axes with the arrow keys (Shift a tenth of a step, Ctrl ten steps, Page Up and Down the value by a
tenth, Home and End the saturation to its ends); the bars and the ring take the keys of a Slider,
and the hue wraps at its ends. A press puts a handle under the pointer and a drag follows it
(Shift finer); Escape during a drag or a held key restores the color from the start of that edit.
The default width is `metrics.pickerWidth`, at most the available width. `hdr` lets the inputs edit
channels above 1; the square, the triangle and the bars cover 0 to 1, so an edit there brings an
HDR color into that range. The visible label part is the panel's tooltip. Status: ready.

### ColorInputs

```cpp
bool ColorInputs(const char* label, ImVec4* color, const ColorInputsOptions& options = {});
```

A Dropdown with the models (Strings::colorModelRgb, Hsv, Hsl and Hex; left out with `modeSwitch`
false, and on a line of its own above the fields when they do not fit beside it), then one DragNumber per channel of the
model, each a Tab stop marked with the first letter of its channel name (Strings::colorRed and the
others), or one HexInput (six digits, eight with `alpha`) marked `#`. RGB channels run from 0 to 255,
hue from 0 to 360 (wrapping), saturation, value, lightness and alpha in HSV and HSL from 0 to 100 %;
with `floats` (always with `hdr`) every channel is a fraction from 0 to 1, with `hdr` without an
upper limit for red, green, blue and value. The fields drag, step and type as DragNumber and
HexInput do; a field writes only its channel, and a hex edit changes only the bytes that changed.
Ctrl+V on a focused field reads color text and writes the whole color: `#RGB`, `#RGBA`, `#RRGGBB`,
`#RRGGBBAA`, six or eight hex digits with a letter among them, `rgb()`, `rgba()`, `hsv()` and `hsl()`
with percentages or fractions, and three or four bare numbers (fractions when all are at most 1,
0 to 255 otherwise); a single number is pasted into the field as a number. The model the user
picks is kept under the id. `error` outlines every field and its message goes under the row.
Status: ready.

### SwatchPalette

```cpp
struct ColorHistory { static constexpr int kCapacity = 8; ImVec4 colors[kCapacity]; int count; };
bool SwatchPalette(const char* label, ImVec4* color, std::span<const ImVec4> swatches, ColorHistory* history,
                   const SwatchPaletteOptions& options = {});
bool SwatchPalette(const char* label, ImVec4* color, std::span<ImVec4> storage, int* count, ColorHistory* history,
                   const SwatchPaletteOptions& options = {});
bool SwatchPalette(const char* label, ImVec4* color, std::vector<ImVec4>* swatches, ColorHistory* history,
                   const SwatchPaletteOptions& options = {});
```

A grid of swatches the size of the control height (smaller when a fixed `columns` count does not
fit the width), with the recent colors of `history` in a row above it. The whole palette is one
Tab stop: Left and Right move the focused swatch, Up and Down move to the swatch above or below
(across the recent row and the grid), Home and End go to the first and last swatch, Enter or Space
picks it; a click picks the swatch under the pointer. Each pick goes to the front of the history.
The swatch equal to the color has a ring in the label color and a check mark in white or black,
whichever reads on it, so the choice does not rest on color alone. Without `alpha` the palette
compares and writes red, green and blue only. The editable forms end with a + swatch (Strings::add
as its tooltip) that adds the color while there is room, Delete or Backspace removes the focused
swatch, Alt with Left or Right moves it, and a drag moves a swatch to where the pointer is (a
focus ring colored line marks the place). Without swatches and history it shows Strings::empty.
The label is the palette's tooltip. Status: ready.

### SatValSquare

```cpp
bool SatValSquare(const char* label, ImVec4* color, const SatValSquareOptions& options = {});
```

The saturation and value square of ColorPicker on its own, for the hue of the color: one item and
one Tab stop with the keys and the drag of the picker's square. Hue and alpha are kept. The default
width is `metrics.pickerWidth`, at most the available width; the square keeps its aspect. The label
is its tooltip. Status: ready.

### ColorChannelSlider

```cpp
bool ColorChannelSlider(const char* label, ImVec4* color, const ColorChannelSliderOptions& options = {});
```

A slider whose track is the gradient of one channel (hue, HSV saturation and value, red, green, blue
or alpha) with the other channels held, `picker.colorBarHeight` thick, with a checkerboard under the
alpha gradient. It lays out like a Slider (the label in front, the fader length when vertical, three
control sizes) and takes its keys: the arrows step (Shift finer, Ctrl coarser), Page Up and Down
move a tenth, Home and End go to the ends; the hue wraps. There is no value text and no typed
value. It writes only its channel. Status: ready.

### ColorModeEditor

```cpp
struct AnimatedColor { ColorAnimation animation; ImVec4 color; ImVec4 second; float period; };
ImVec4 SampleAnimatedColor(const AnimatedColor& animated, double seconds);
bool ColorModeEditor(const char* label, AnimatedColor* animated, const ColorModeEditorOptions& options = {});
```

The editor is a SegmentedControl of the animations (Strings::animationStatic and the others), then
a ColorWell for the base color, a second ColorWell for Pulse and Gradient, a DragNumber for the period
in seconds (Strings::period, 0.1 to 60) for every animation but Static, and a live preview swatch;
the second row wraps when the width is too small. Each is a Tab stop. `SampleAnimatedColor` gives the
color at a time: Pulse eases from `color` to `second` and back (a cosine over the period), Gradient
sweeps evenly there and back, both blended in OKLab; Cycle turns the OKLCH hue of `color` once per
period and keeps its lightness and chroma. While facet::ReduceMotion() is true, for Static and for
a period that is not above 0 it returns `color`. The period field shows no unit text yet: a
built-in string for seconds does not exist. Status: ready.
### Usage of the color pickers

A color setting with a recent-colors row shared by two pickers. The history is caller data, so it
can be saved with the other settings.

```cpp
static ImVec4 tint = facet::T().colors.accent;
static facet::ColorHistory recent;
facet::ColorWell("Tint", &tint, {.alpha = true, .showHex = true});
static const ImVec4 kPresets[] = {first, second, third};
facet::SwatchPalette("##presets", &tint, kPresets, &recent);
```

Undo for a picker: the compare swatch shows the color before the current edit, and each finished
drag pushes one undo step.

```cpp
static ImVec4 color = start, before = start;
facet::ColorPicker("Color", &color, {.original = &before});
if (facet::IsItemEditCommitted())
{
    PushUndo(before, color);  // application function
    before = color;
}
```

An overlay element that pulses, evaluated once per frame.

```cpp
static facet::AnimatedColor marker = {.animation = facet::ColorAnimation::Pulse, .color = base, .second = bright};
facet::ColorModeEditor("Marker", &marker);
const ImU32 now = facet::ToU32(facet::SampleAnimatedColor(marker, ImGui::GetTime()));
```

## Date, time and other pickers

### DatePicker

```cpp
struct Date { int year; int month; int day; };
bool DatePicker(const char* label, Date* date, const DatePickerOptions& options = {});
```

A segmented date field with a calendar in a popover or in place. The field shows the date in ISO
order (`2026-03-14`, `2026-03` at Month precision, `2026` at Year precision), the placeholder
(`Strings::select` unless `placeholder` is set) while there is no date, and a calendar icon at its
end. It is a field widget: the label in front, the available width by default; with
`inlineCalendar` the default width fits the label and the calendar, at most the available width,
and a narrower width shrinks the day cells.

Keys on the focused field: Left and Right pick the year, month or day segment; Up and Down step it
and write at once, wrapping inside the segment (day 31 goes to day 1 of the same month) while the
day follows the length of the month; the wheel while focused and a vertical drag on the field step
the same way. Each step is an edit session of the value drag core, so `IsItemEditCommitted` reports
it and Escape during a held step restores the date. Typed digits fill the focused segment and move
on to the next one (`-`, `/` and `.` move on as well); they are shown as typed and written on Enter,
Tab or focus loss once they make a real date, clamped into `[min, max]`. An invalid date such as 31
February stays shown as typed and is written only after it is corrected; Escape drops the typed
parts. Backspace or Delete clears the focused segment, and a field whose segments are all cleared
writes no date. A typed commit is reported by the result and `ImGui::IsItemEdited`, not by
`IsItemEditCommitted`, because typing is not an edit session of Dear ImGui.

A click, Enter, Space or Alt+Down opens the calendar below the field (above when there is no room),
on the blurred popover surface. It shows the month of the date (or of today) with the weekday
headings from `Strings::weekdayMonday` to `weekdaySunday` starting at `firstWeekday`, today with a
hairline outline and the day number in the accent text color, the picked day on the accent fill in
semibold, and days outside `[min, max]` disabled. The keyboard focus moves to the date: the arrows
move by a day and a week, Page Up and Page Down by a month (with Shift by a year), Home and End to
the start and end of the week, across month and year borders and never outside `[min, max]`;
Enter, Space or a click picks, writes and closes, and the focus returns to the field. Escape or a
click outside closes without writing. The previous and next buttons page the months; the title
(`Strings::monthJanuary` to `monthDecember` and the year) opens the month view, and the month
view's title the year view (twelve years), which hand back to the narrower view when an entry is
picked. At Month precision the calendar starts in the month view and a month is the pick; at Year
precision the year view, and a year is the pick. A calendar narrower than three buttons drops the
previous and next buttons; Page Up and Page Down still page.

`readOnly` (or a null `date`) shows the date and focuses, but the calendar does not open and nothing
is written. `mixed` shows `Strings::mixed` until a date is written. Disabled through
`ImGui::BeginDisabled`. `error` and `helpText` show under the field. Captures: `ForcedItemState::Open`
shows the popover; with `inlineCalendar`, a request for part 1 goes to the calendar's cursor day.
State kept under the id: the focused segment, typed parts, and the calendar's view, shown month and
cursor. Today is the local date of the system clock. Status: ready.

### DateRangePicker

```cpp
bool DateRangePicker(const char* label, Date* start, Date* end, const DateRangePickerOptions& options = {});
```

A field showing the range, `2026-03-09`, an arrow and `2026-03-13` (dashes for a missing end, the
placeholder while both are missing), with a calendar icon at its end; a field widget with the label
in front and the available width by default. A click, Enter, Space or Alt+Down opens a popover below
the field with the calendar of DatePicker, two months side by side (one when the display is too
narrow for two) under one pair of previous and next buttons; it starts at the month of *start (or
of today). The first pick writes *start and clears *end; from then on the band between the start and
the day under the pointer (or the keyboard cursor) shows in accentSubtle, and the second pick writes
both ends in order, earlier first, in the same frame, and closes. The ends of a completed range are
on the accent fill; pressing one and dragging moves it (writing while dragging), and an end dragged
past the other takes its place. Keys in the calendar are those of DatePicker; Enter and Space pick.
`presets` adds a column of plain buttons under a `Strings::presets` caption beside the calendars
(above them when there is no room); a preset writes both ends at once, in order and clamped into
`[min, max]`, and closes. Days outside `[min, max]` are disabled; `firstWeekday` sets the week start.

Escape or a click outside closes; a range left after one pick keeps the start and no end.
`readOnly` (or a null pointer) shows the range and focuses but does not open. `mixed` shows
`Strings::mixed` until an edit. `error` and `helpText` show under the field. Captures:
`ForcedItemState::Open` shows the popover. State under the id: the first pick while a range is
being picked, the end being dragged; the calendar keeps its shown months and cursor. Status: ready.

### TimePicker

```cpp
struct TimeOfDay { int hour; int minute; int second; };
bool TimePicker(const char* label, TimeOfDay* time, const TimePickerOptions& options = {});
```

A segmented time field: `14:05`, `14:05:30` with `showSeconds`, and with `twelveHour` the hour 1 to
12 (12 at noon and midnight) followed by `Strings::am` or `Strings::pm`, with a clock icon at its
end. A field widget: the label in front, the available width by default.

Keys on the focused field: Left and Right pick the segment; Up and Down step it and write at once,
the wheel while focused and a vertical drag on the field as well, each step an edit session of the
value drag core (Escape during a held step restores the time). Steps wrap around midnight and carry
into the next larger unit: 23:59 plus a minute is 00:00, 00:00 minus an hour is 23:00; the AM and
PM segment moves the time by twelve hours. Minutes step by `minuteStep` on the slots 0, step, twice
the step and so on in every hour; a minute between slots first moves to the next slot in the
direction of the step. Typed digits fill the segment (a first hour digit that cannot start a two
digit hour fills it alone) and move on; `:`, `.` and a space move on too; A or P sets the half of
the day. Typed parts are written on Enter, Tab or focus loss when they make a valid time; an
invalid one (hour 25) stays shown as typed until corrected, and Escape drops them.

A click, Enter, Space or Alt+Down opens a popover below the field with one scrolling column per
unit (hours, minutes in slots, seconds, AM and PM), each `picker.timeColumnWidth` wide and five
rows tall, the chosen entry on the selection pill. The focus starts on the chosen hour; Tab moves
between the columns. A click or an arrow key in a column writes at once; Enter or a double click
writes and closes, and the focus returns to the field; Escape or a click outside closes. With
`inlineColumns` the columns stay under the field and scroll to the time whenever it changes
elsewhere.

`readOnly` (or a null `time`) shows and focuses but never writes or opens. `mixed` shows
`Strings::mixed` until a time is written. `error` and `helpText` show under the field. Captures:
`ForcedItemState::Open` shows the popover; with `inlineColumns`, a request for part 1 goes to the
first column. State under the id: the focused segment and typed parts; the columns keep their
scroll positions. Status: ready.

### AnglePicker

```cpp
bool AnglePicker(const char* label, float* radians, const AnglePickerOptions& options = {});
```

A round dial with a needle over an angle in radians, counter-clockwise from the right, with a
degrees field beside it (`field`, a NumberInput without steppers that shows the degrees and the
degree sign). The label is drawn in front; the dial is `diameter` units across, or two control
heights of `size`; the default width fits the label, the dial and the field, at most the available
width. In a narrower width the field shrinks first and is left out below one and a half control
heights; in a width narrower than the label and the dial, the dial shrinks.

The pointer sets the needle: a press points it at the pointer, a drag follows it in whole degrees
(Shift snaps to multiples of `snapDegrees`), and Escape during the drag restores the angle from the
press. While the dial has keyboard focus, the arrows of both axes turn it by one degree (Right and
Up counter-clockwise), Shift by `snapDegrees`, Ctrl and Page Up and Down by ten steps, and the wheel
over the focused dial turns it too; each press is an edit session of the value drag core. The angle
wraps: it is written as 0 to 360 degrees, or -180 to 180 with `signedRange`. Typed degrees are
written on Enter, Tab or focus loss and clamped to that range. The dial draws a face with four
quarter ticks, the needle in the accent color (labelSecondary when read-only, labelTertiary when
disabled) and the round focus ring; `error` turns its outline destructive and shows the message.

`readOnly` (or a null pointer) shows the angle and focuses but never writes. `mixed` hides the
needle and empties the field until an edit. Captures: a forced state for part -1 or 0 goes to the
dial, for part 1 to the field. Status: ready.

### IconPicker

```cpp
bool IconPicker(const char* label, Icon* icon, const IconPickerOptions& options = {});
bool IconPicker(const char* label, Icon* icon, std::span<const ChoiceItem> icons, const IconPickerOptions& options = {});
```

A field that shows the icon and its name (the item label, or the registered name in the first form;
the placeholder or `Strings::select` while there is none) with a chevron, like a Dropdown field: the
label in front, the available width by default. A click, Enter, Space, Down or Alt+Down opens a
popover below it (above when there is no room) with a search field (`searchable`) over a grid of
icon tiles one Large control height square. The recently picked icons come first under
`Strings::recent` (`showRecent`, up to eight, kept under the id while the widget is shown), then
every icon, in sections under their `ChoiceItem::group` text; the first form offers every
registered icon (`IconCount`, `IconNameAt`), sorted by name. `columns` sets the columns; 0 fits as
many as the wider of the field and six tiles allow, at most the display width. The grid shows six
lines at most and scrolls; only its visible lines are submitted, and the tiles are rebuilt only when
the filter, the item count, the columns or the recent icons change, so a set of any size costs the
same per frame.

The search field takes the typing from the moment the popover opens and filters by the fuzzy
matcher on the names (in item order when there are sections, best match first otherwise); with no
match the grid shows `Strings::noResults`. Down moves from the search field into the grid. In the
grid the arrows move in two dimensions (Up and Down keep the column and pass over section headers),
Home and End go to the first and last icon, Page Up and Down by the lines in view; disabled items
are shown but skipped. Enter, Space or a click picks, writes, adds the icon to the recent ones and
closes, and the focus returns to the field; Escape (after clearing the search) or a click outside
closes without writing. Without `searchable`, the focus starts on the chosen icon. The chosen tile
has a ring in the accent text color; each tile shows its name as a tooltip on hover and keyboard
focus.

`readOnly` (or a null pointer) shows the icon but does not open. `mixed` shows `Strings::mixed`
until an icon is picked. `error` and `helpText` show under the field. Captures:
`ForcedItemState::Open` shows the popover. Status: ready.

### ThumbnailPicker

```cpp
bool ThumbnailPicker(const char* label, int* index, std::span<const ThumbnailItem> items,
                     const ThumbnailPickerOptions& options = {});
bool ThumbnailPicker(const char* label, std::span<bool> checked, std::span<const ThumbnailItem> items,
                     const ThumbnailPickerOptions& options = {});
```

A grid of captioned tiles in the available width (`width` otherwise): each tile is
`thumbnailSize` units (`metrics.thumbnailSize` square by default, narrowed to the width), with its
caption in Footnote under the picture, cut with an ellipsis and shown whole as a tooltip. A tile
shows its image (with the image's own loading and failed looks), or its icon on a quiet fill when it
has none. `columns` sets the columns; 0 fits as many as the width allows. The visible label part is
not drawn; it is the panel's tooltip on hover and keyboard focus. Only the lines in view are
submitted (through the row ranges of the list core in the surrounding window), so a grid of any
size costs the same per frame.

The single form shows the picked tile with the row selection fill and a ring in the accent text
color. It is one Tab stop: the tile focused last, else the picked one, else the first. The arrows
move in two dimensions (Up and Down keep the column), Home and End go to the first and last tile,
Page Up and Down by the lines in view; Enter, Space or a click writes *index. The multi form keeps
one flag per tile in `checked`: every tile is a Tab stop, Dear ImGui's navigation moves between
them, and a click or Space flips the flag; a checked tile shows the ring and a check badge. A
`locked` tile shows a lock badge and takes the focus but is never picked or flipped; a `disabled`
tile is dimmed, cannot be focused and is passed over by the arrows.

`state` Loading shows skeleton tiles in place of the tiles (one line while there are none), Error
shows `errorText` (or `Strings::loadFailed`) and a Ready grid without tiles `emptyText` (or
`Strings::empty`), in the room of one line; none of them takes input. `readOnly` (or a null index)
lets the tiles focus but writes nothing. Captures: a forced state for part -1 or 0 goes to the
tile Tab lands on. Status: ready.

### FileBrowser

```cpp
struct FileType { const char* label; const char* patterns; };
bool FileBrowser(const char* label, std::string* path, const FileBrowserOptions& options = {});
bool FileBrowser(const char* label, char* buffer, std::size_t bufferSize, const FileBrowserOptions& options = {});
```

A file browser drawn inside the app, for overlays and hosts that cannot show a system dialog. Like
the other panels, it does not draw its label beside it but shows it as its tooltip on hover and
keyboard focus. It fills the available width (`width` otherwise) and stacks a path bar (the parent
folder button, named `Strings::parentFolder`, and the folder's path cut in the middle, whole as a
tooltip), a framed list of the folder's entries `metrics.panelHeight` tall (`height` sets the
height of the whole browser), the file name field (`Strings::fileName`, Save only), and a bottom
line with the file type dropdown (`Strings::fileType`, when `fileTypes` is not empty; at most 32
types) and the confirm button (`Strings::open`, `save` or `chooseFolder` by mode).

It starts in the folder the current path names, else the folder of the file it names, else the
current directory, and keeps the folder it shows under its id. Every file system call runs on one
worker thread, so a slow or unreachable folder never blocks a frame: the list shows the loading
state while a folder is read, the error state with `Strings::loadFailed` for a folder that cannot be
read (no permission, gone, not a folder), and the empty state for an empty one. While a folder is
shown it is checked every two seconds on the worker; a folder that changed is read again (the rows
stay until the new ones arrive, and the chosen entry is kept by name), and a folder that disappeared
turns into the error state, from which the parent folder button still leads out. The list is a list
view of the list core, so a folder of any size costs what its visible rows cost; names that do not
fit end in an ellipsis and show whole as a tooltip.

Folders come first, then files, each by name without case; hidden and system entries and names
starting with a dot are left out unless `showHidden`; files are filtered by the patterns of the
chosen file type (`*` and `?`, separated by `;`, ASCII case folded), and Folder mode lists folders
only. The arrows, Page Up and Down, Home and End move in the list and typing jumps to a name; a
double click or Enter opens a folder and, on a file, confirms (Save takes the file's name first);
Backspace in the list goes to the parent folder. In Save mode choosing a file copies its name into
the name field, and Enter in the field confirms. The confirm button writes the folder joined with
the chosen file (Open), the typed name (Save), or the chosen folder, else the shown folder (Folder),
as one UTF-8 path; it is off while nothing fits the mode, while `readOnly` is set (browsing and
choosing still work), and, in the buffer form, while the path does not fit `bufferSize` bytes with
its terminating zero. Paths are joined with a backslash; the browser does not create, rename or
delete anything. The worker is joined in `facet::Shutdown`; a folder read in progress then finishes
first. Status: ready.

### Usage of the other pickers

A date range limited to the past, with one preset.

```cpp
static facet::Date from, to;
const facet::Date today = Today();  // application function
const facet::DateRangePreset presets[] = {{"Option 1", today, today}};
facet::DateRangePicker("Period", &from, &to, {.max = today, .presets = presets});
```

Picking a file in an overlay, where a system dialog cannot be shown: the browser goes into a modal
and writes the path on confirm. A file type without patterns lists every file.

```cpp
static const facet::FileType kTypes[] = {{"Images", "*.png;*.jpg"}, {"All files"}};
if (facet::BeginModal("Open image", &browsing))
{
    if (facet::FileBrowser("##files", &imagePath, {.fileTypes = kTypes}))
        browsing = false;
    facet::EndModal();
}
```

## Planned, not declared yet

These widgets of the later tier are not declared yet.

- **NumberWheel**: a drum-style scrolling picker with inertia and a center selection band.
- **DirectionPicker**: a shaded sphere that picks a normalized 3D direction.
- **FontPicker**: a dropdown of loaded font faces previewed in their own face.
- **CascadePicker**: a hierarchical picker with side-by-side level columns.
- **Screen eyedropper**: sampling colors outside the app's own frame needs a capture path that a
  Direct3D 11 overlay does not have; it waits for the blur copy path to be usable for sampling.
