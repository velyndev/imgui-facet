#pragma once

// Display: styled and truncated text, section headers, banners and alerts, empty states and
// data state views, progress, spinners and skeletons, badges and status marks, key caps and
// shortcut hints, character counters, inline code, code and log views, key-value lists, stat
// tiles and data tables.
//
// The declarations follow docs/api-conventions.md; widgets/common.h holds the types shared
// with other families. Every widget here is declared and planned: its body draws a
// placeholder until Phase 2 implements it (docs/widgets/display.md lists the status of each).
//
// Lengths are design units (Px() is applied inside). Strings are UTF-8 and read only during
// the call. Every function is for the thread that runs the Dear ImGui frame (context.h).

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>

#include <imgui.h>

#include "facet/core/icons.h"
#include "facet/core/input.h"
#include "facet/core/theme.h"
#include "facet/widgets/common.h"

namespace facet
{

// --- Text -----------------------------------------------------------------------------------

// Horizontal alignment of text inside the width it is given: styled text and table columns.
enum class ColumnAlign
{
    Start,
    Center,
    End,
};

struct StyledTextOptions
{
    TextStyle style = TextStyle::Body;     // size and weight from T().type
    Tone tone = Tone::Neutral;             // Neutral draws label; the others their status token
    bool secondary = false;                // Neutral only: labelSecondary instead of label
    bool wrap = false;                     // wraps at the item width instead of running past it
    ColumnAlign align = ColumnAlign::Start;  // inside the item width; Start when the width is the text's own
    float width = 0.0f;                    // units; 0 the text's own width, or the available width with wrap
};

// Text in a theme text style and a semantic color. Not interactive and keeps no state, so it
// takes no id. `text` is drawn as it is (no printf formatting, no "##" handling); format into
// a buffer first, or use facet::Text (core/text.h) for formatted text in a style. One item of
// the text's height. A null text draws nothing but still submits an empty item.
void StyledText(const char* text, const StyledTextOptions& options = {});

// Where the ellipsis replaces text that does not fit.
enum class TruncateMode
{
    End,     // "Long file na..."
    Middle,  // "Long f...name.txt", for paths and file names
    Start,   // "...file name.txt"
};

struct TruncatedTextOptions
{
    TruncateMode mode = TruncateMode::End;
    int maxLines = 1;                      // lines before the ellipsis; End mode only for more than 1
    TextStyle style = TextStyle::Body;
    bool secondary = false;                // labelSecondary instead of label
    float width = 0.0f;                    // units; 0 fills the available width
};

// Text cut with an ellipsis to fit its width. While text was cut, hovering the item shows the
// whole text as a tooltip. Not focusable and not activatable; keeps no state. A null text
// draws nothing but still submits an item of one line.
void TruncatedText(const char* text, const TruncatedTextOptions& options = {});

struct SectionHeaderOptions
{
    const char* subtitle = nullptr;        // second line in labelSecondary; null for none
    const char* actionLabel = nullptr;     // trailing link-style action; null for none
    bool divider = false;                  // hairline in separator under the header
    float width = 0.0f;                    // units; 0 fills the available width
};

// A section heading (Title3) with an optional subtitle, a trailing action and a divider. The
// visible label part is the heading; the label is also the id of the action. Returns true on
// the frame the action is activated (click, Enter, Space while it has keyboard focus); without
// actionLabel it is not a Tab stop and always returns false. Keeps no state.
bool SectionHeader(const char* label, const SectionHeaderOptions& options = {});

// --- Alerts and states ----------------------------------------------------------------------

// What the caller does after a Banner call.
enum class BannerAction
{
    None,       // nothing happened this frame
    Primary,    // the primary action was activated
    Secondary,  // the secondary action was activated
    Dismissed,  // the close button or Escape (while the banner has focus) dismissed it
};

struct BannerOptions
{
    Tone tone = Tone::Neutral;             // tint and default icon; Accent for information
    Icon icon;                             // none: the tone's icon
    const char* primaryAction = nullptr;   // button text; null for none
    const char* secondaryAction = nullptr; // plain button text; null for none
    float width = 0.0f;                    // units; 0 fills the available width
};

// A full-width strip that stays until it is dismissed, for conditions that affect the whole
// view (offline, read-only mode, an update waiting). The visible label part is the bold title
// ("##id" for a banner without title) and the label is the id; `message` is the body text,
// wrapped. `open`: null for a banner without close button; otherwise the banner shows while
// *open is true, and dismissing it writes false. Call it every frame also while closed: after
// a dismissal it collapses its height over motion.durationStandard (at once with reduced
// motion) and then takes no space. Each action is a Tab stop; Escape on a focused action
// dismisses. Keeps the collapse animation under its id.
BannerAction Banner(const char* label, bool* open, const char* message, const BannerOptions& options = {});

struct InlineAlertOptions
{
    Tone tone = Tone::Neutral;             // tint and default icon
    Icon icon;                             // none: the tone's icon
    const char* actionLabel = nullptr;     // one action under the message; null for none
    float width = 0.0f;                    // units; 0 fills the available width
};

// A tinted callout box inside the content (a field group's warning, a hint above a form). The
// visible label part is the title in Headline ("##id" for none); `message` is the wrapped body.
// `open` as in Banner (null: no close button). Returns true on the frame the action is
// activated. The close button and the action are Tab stops. Keeps the collapse animation
// under its id.
bool InlineAlert(const char* label, bool* open, const char* message, const InlineAlertOptions& options = {});

struct EmptyStateOptions
{
    const char* message = nullptr;         // wrapped text under the title; null for none
    Icon icon;                             // drawn above the title; none for no icon
    const char* actionLabel = nullptr;     // one button under the message; null for none
    Tone tone = Tone::Neutral;             // Destructive for an error state
    bool compact = false;                  // one line with a small icon, for lists and search results
    float width = 0.0f;                    // units; 0 fills the available width
    float height = 0.0f;                   // units; 0 fits the content; content is centered in it
};

// A centered placeholder for a view without content: an icon, the visible label part as the
// title, a message and one action. Returns true on the frame the action is activated. Only the
// action is a Tab stop. Keeps no state.
bool EmptyState(const char* label, const EmptyStateOptions& options = {});

struct DataStateViewOptions
{
    int skeletonRows = 3;                  // placeholder rows while Loading
    const char* emptyText = nullptr;       // null uses Strings::empty
    const char* errorText = nullptr;       // null uses Strings::loadFailed
    const char* retryLabel = nullptr;      // a retry button in the error state; null for none
    float height = 0.0f;                   // units; 0 keeps the height of the last Ready frame
};

// One wrapper for any data view (a list, a table, a grid) that swaps between skeleton rows
// while Loading, the error state with an optional retry button, the empty state (Ready with a
// count of 0) and the caller's content. Returns true when the content must be submitted
// (Ready and count > 0); then call EndDataStateView. Returns false while it draws a state
// itself; then do not call EndDataStateView. The id keys the crossfade between states and the
// remembered height, which keeps the layout from jumping when the state changes.
[[nodiscard]] bool BeginDataStateView(const char* id, DataState state, int count,
                                      const DataStateViewOptions& options = {});
void EndDataStateView();
// True on the frame the retry button of the last BeginDataStateView call was activated.
bool DataStateViewRetryPressed();

class [[nodiscard]] DataStateViewScope
{
public:
    DataStateViewScope(const char* id, DataState state, int count, const DataStateViewOptions& options = {})
        : open_(BeginDataStateView(id, state, count, options))
    {
    }
    ~DataStateViewScope()
    {
        if (open_)
            EndDataStateView();
    }
    DataStateViewScope(const DataStateViewScope&) = delete;
    DataStateViewScope& operator=(const DataStateViewScope&) = delete;
    explicit operator bool() const { return open_; }

private:
    bool open_;
};

// --- Progress and loading -------------------------------------------------------------------

struct ProgressBarOptions
{
    Tone tone = Tone::Accent;              // fill color; Success or Destructive for a finished state
    bool showPercent = false;              // percentage after the visible label part
    float buffer = 0.0f;                   // 0 to 1: a second, lighter fill ahead of the value; 0 none
    ControlSize size = ControlSize::Inherit;  // track thickness follows the size
    float width = 0.0f;                    // units; 0 fills the available width
};

// A linear progress track. Not interactive. The visible label part is drawn above the bar;
// the id keys the smoothed fill. `fraction` is 0 to 1 (clamped in the drawing) or
// kIndeterminate, which sweeps a segment instead (a slow fade with reduced motion).
void ProgressBar(const char* label, float fraction, const ProgressBarOptions& options = {});

struct ProgressRingOptions
{
    Tone tone = Tone::Accent;
    const char* centerText = nullptr;      // text inside the ring; null shows the percentage when showPercent
    bool showPercent = false;
    float diameter = 0.0f;                 // units; 0 the control height of size
    ControlSize size = ControlSize::Inherit;
};

// A circular progress arc with round caps. Not interactive. The visible label part is drawn to
// the right of the ring ("##" for none) and is its accessible name; the id keys the smoothed
// arc. `fraction` as in ProgressBar.
void ProgressRing(const char* label, float fraction, const ProgressRingOptions& options = {});

struct SpinnerOptions
{
    Tone tone = Tone::Neutral;             // Neutral draws labelSecondary, Accent the accent
    ControlSize size = ControlSize::Inherit;  // the diameter is the text height of the size
};

// A turning busy indicator; with reduced motion it fades in place instead. Not interactive.
// The visible label part is drawn after it ("##" for none) and is its accessible name. Every
// spinner reads one shared clock, so spinners on screen turn in step; nothing is kept per id.
void Spinner(const char* label, const SpinnerOptions& options = {});

enum class SkeletonShape
{
    Text,    // `lines` text lines in the Body height, the last one shorter
    Rect,    // one block of width x height
    Circle,  // a circle of the height (an avatar placeholder)
};

struct SkeletonOptions
{
    SkeletonShape shape = SkeletonShape::Text;
    int lines = 1;                         // Text only
    Radius corners = Radius::Element;      // Rect only
    float width = 0.0f;                    // units; 0 fills the available width (Circle: the height)
    float height = 0.0f;                   // units; 0 one Body line (Text), the control height otherwise
};

// A loading placeholder with the shared shimmer sweep (flat with reduced motion). Not
// interactive and keeps no state, so it takes no id.
void Skeleton(const SkeletonOptions& options = {});

// --- Badges and status ----------------------------------------------------------------------

struct BadgeOptions
{
    Tone tone = Tone::Neutral;
    bool solid = false;                    // filled with the tone color; a subtle tint by default
    Icon icon;                             // leading icon; none by default
};

// A small label in a rounded box (element radius, not a pill), for a real status or category
// of the item next to it. `text` is drawn as it is and is not an id. Not interactive.
void Badge(const char* text, const BadgeOptions& options = {});

struct CountBadgeOptions
{
    Tone tone = Tone::Accent;
    int maxCount = 99;                     // larger counts show as "99+"
    bool showZero = false;                 // a count of 0 draws nothing unless set
};

// A counter pill (unread items, pending changes). Negative counts are drawn as 0. Not
// interactive; keeps no state.
void CountBadge(int count, const CountBadgeOptions& options = {});

struct StatusDotOptions
{
    Tone tone = Tone::Neutral;
    bool pulse = false;                    // a ring that pulses for a live state; still with reduced motion
};

// A small status dot. The visible label part is drawn after it ("##" for a dot alone, then it
// is the hover tooltip and the accessible name). Not interactive.
void StatusDot(const char* label, const StatusDotOptions& options = {});

struct StatusPillOptions
{
    Tone tone = Tone::Neutral;
    Icon icon;                             // leading icon; none by default
    bool liveDot = false;                  // a pulsing dot instead of the icon
    ControlSize size = ControlSize::Inherit;
};

// A toned pill with the visible label part as its text, for the state of a connection, a job
// or a device. A tone change crossfades over motion.durationFast, keyed by the id; use "###"
// when the text changes with the state. Not interactive.
void StatusPill(const char* label, const StatusPillOptions& options = {});

// --- Keys -----------------------------------------------------------------------------------

struct KeyCapOptions
{
    bool live = false;                     // shows the pressed look while the chord is down
    ControlSize size = ControlSize::Inherit;
};

// A chord drawn as key caps, one cap per modifier and one for the key, with the names of
// KeyNameText. ImGuiKey_None draws Strings::unbound in labelSecondary. Not interactive; keeps
// no state.
void KeyCap(ImGuiKeyChord chord, const KeyCapOptions& options = {});

struct ShortcutHintOptions
{
    float width = 0.0f;                    // units; 0 fills the available width
};

// `text` in labelSecondary at the start and the chord as key caps at the end of the width;
// when the width is too small, the caps are dropped before the text is cut. Not interactive.
void ShortcutHint(const char* text, ImGuiKeyChord chord, const ShortcutHintOptions& options = {});
// The current primary chord of a registered shortcut (core/input.h); an invalid or unknown id
// draws the text alone.
void ShortcutHint(const char* text, ShortcutId shortcut, const ShortcutHintOptions& options = {});

struct CharacterCounterOptions
{
    bool ring = false;                     // a small ring instead of "count / max" text
    int warnRemaining = 10;                // the warning tone from this many characters left
};

// The length of a text against its limit: neutral, warning near the limit, destructive over
// it. `max` <= 0 shows the count alone. Not interactive; keeps no state.
void CharacterCounter(int count, int max, const CharacterCounterOptions& options = {});

struct InlineCodeOptions
{
    bool copyOnActivate = false;           // activation copies the text and shows Strings::copied
};

// Inline code or an identifier in a tinted box on the text baseline. The visible label part is
// the code text and the label is its id. Returns true on the frame the text was copied (click,
// Enter or Space with copyOnActivate); without it the item is not a Tab stop.
bool InlineCode(const char* label, const InlineCodeOptions& options = {});

// --- Code and logs --------------------------------------------------------------------------

// Languages the code view tints. Unknown text is Plain.
enum class CodeLanguage
{
    Plain,
    Cpp,
    Json,
    Ini,
};

struct CodeViewOptions
{
    CodeLanguage language = CodeLanguage::Plain;
    bool lineNumbers = true;
    bool wrap = false;                     // wrap long lines instead of scrolling sideways
    bool copyButton = true;                // a copy button in the top-right corner
    std::span<const int> highlightLines = {};  // 1-based lines with the accentSubtle background
    float width = 0.0f;                    // units; 0 fills the available width
    float height = 0.0f;                   // units; 0 fits the text up to 20 lines, then scrolls
};

// A read-only code block with a line gutter and light syntax tint. `code` is read during the
// call. The view is one Tab stop; while it has focus, arrows and Page Up and Down scroll and
// Ctrl+C copies the whole text. Returns true on the frame the text was copied. Keeps the
// scroll position under its id.
bool CodeView(const char* id, std::string_view code, const CodeViewOptions& options = {});

// Severity of a log entry, ordered from least to most severe so a caller can filter with
// `level >= LogLevel::Warning`. It is entry data, not a choice option: the member order is the
// severity order, so the zero member is the lowest level and LogEntry defaults to Info.
enum class LogLevel
{
    Trace,
    Debug,
    Info,
    Warning,
    Error,
};

// One log line. `text` must stay valid during the call only.
struct LogEntry
{
    std::string_view text;
    LogLevel level = LogLevel::Info;
    double time = std::numeric_limits<double>::quiet_NaN();  // seconds (the caller's clock, any sign); NaN shows no timestamp
    const char* source = nullptr;          // short origin tag drawn before the text; null for none
};

// Fills `out` for entry `index` and returns true, or false to skip it. Called during the call
// only, for the visible entries.
using LogGetter = bool (*)(void* user, int index, LogEntry* out);

struct LogViewOptions
{
    bool autoScroll = true;                // follows new entries while scrolled to the end
    bool timestamps = true;                // a time column for entries with a time
    DataState state = DataState::Ready;
    const char* emptyText = nullptr;       // null uses Strings::empty
    const char* errorText = nullptr;       // null uses Strings::loadFailed
    float width = 0.0f;                    // units; 0 fills the available width
    float height = 0.0f;                   // units; 0 fills the available height
};

// A virtualized log with level colors and an optional time column. Every row is one line of
// the Body height and a long line is cut at the view edge (hovering shows it whole), so rows
// have equal heights and only the visible entries are read. When the user scrolls up, following stops and a "jump to latest" button appears;
// it and End resume. The view is one Tab stop; arrows and Page Up and Down scroll. Filtering
// is the caller's: pass a getter over the filtered indices. Keeps the scroll state under its id.
void LogView(const char* id, std::span<const LogEntry> entries, const LogViewOptions& options = {});
void LogView(const char* id, int entryCount, LogGetter getter, void* user, const LogViewOptions& options = {});

// --- Key-value lists and tiles --------------------------------------------------------------

struct KeyValueListOptions
{
    float labelWidth = 0.0f;               // units, the label column with its gap; 0 uses T().layout.labelColumn
    bool copyable = false;                 // values copy on activation; each value is then a Tab stop
};

// A read-only list of labelled values (details of a file, a device, a build). Rows use
// LayoutLabeledRow, so labels line up with SettingsRow and stack when narrow. Content is always
// submitted; EndKeyValueList is always called. Keys in labelSecondary, values in label.
void BeginKeyValueList(const char* id, const KeyValueListOptions& options = {});
void EndKeyValueList();

struct KeyValueOptions
{
    Tone tone = Tone::Neutral;             // value color
    bool wrap = true;                      // a long value wraps instead of being cut with an ellipsis
};

// One row inside BeginKeyValueList. The visible label part is the key and the label is the id
// of the row. Returns true on the frame the value was copied (copyable lists only).
bool KeyValue(const char* label, const char* value, const KeyValueOptions& options = {});

class [[nodiscard]] KeyValueListScope
{
public:
    explicit KeyValueListScope(const char* id, const KeyValueListOptions& options = {})
    {
        BeginKeyValueList(id, options);
    }
    ~KeyValueListScope() { EndKeyValueList(); }
    KeyValueListScope(const KeyValueListScope&) = delete;
    KeyValueListScope& operator=(const KeyValueListScope&) = delete;
};

struct StatTileOptions
{
    const char* format = nullptr;          // printf for the value; null: "%.0f"
    FormatFn formatter = nullptr;          // used instead of format when set
    void* formatterUser = nullptr;
    double delta = std::numeric_limits<double>::quiet_NaN();  // change shown as a chip; NaN hides it
    const char* deltaFormat = nullptr;     // printf for the delta; null: "%+.1f"
    bool invertDelta = false;              // a negative delta is the good direction (latency, errors)
    std::span<const float> trend = {};     // a sparkline behind the value; empty for none
    DataState state = DataState::Ready;    // Loading shows a skeleton, Error the error token
    float width = 0.0f;                    // units; 0 fills the available width
};

// A tile with the visible label part as caption, a large value, an optional delta chip and a
// trend line. A value change counts up or down with the standard spring, keyed by the id
// (jumps with reduced motion). Not interactive.
void StatTile(const char* label, double value, const StatTileOptions& options = {});

// --- Data table -----------------------------------------------------------------------------

struct DataTableColumn
{
    const char* label = nullptr;           // header text; "##" hides it, as for widget labels
    float width = 0.0f;                    // units; 0 stretches
    ColumnAlign align = ColumnAlign::Start;
    bool editable = false;                 // cells of this column can enter edit mode
    ImGuiTableColumnFlags flags = ImGuiTableColumnFlags_None;  // stock flags: DefaultSort, NoHide, NoSort
};

struct DataTableOptions
{
    bool sortable = false;                 // read ImGui::TableGetSortSpecs between Begin and End
    bool multiSort = false;
    bool resizable = true;
    bool reorderable = false;
    bool hideable = false;                 // the header context menu hides and shows columns
    int frozenColumns = 0;                 // leading columns that stay in view when scrolled sideways
    bool singleSelect = false;             // with a selection: one row at a time
    float rowHeight = 0.0f;                // units; 0 for the control height
    DataState state = DataState::Ready;
    const char* emptyText = nullptr;       // null uses Strings::empty
    const char* errorText = nullptr;       // null uses Strings::loadFailed
    float width = 0.0f;                    // units; 0 fills the available width
    float height = 0.0f;                   // units; 0 fills the available height
};

// A Dear ImGui table with Facet headers, a header row that stays in view, virtualized rows and
// optional selection. `columns` (1 to 512) are read during the call; `rowCount` rows exist.
// `selection`: null for rows that are not selectable; otherwise Ctrl, Shift and box select run
// through ImGui::BeginMultiSelect and the storage. The storage keys each row by
// selection->AdapterIndexToStorageId(selection, rowIndex), whose default returns the row index
// itself; when the caller sorts, filters or inserts rows, it sets AdapterIndexToStorageId (and
// UserData) to return a stable id of the item shown in that row, so the selection stays on the
// items and not on the positions. Returns false while the
// table is hidden, not Ready, or empty (the state is drawn instead); call EndDataTable only
// after true. Between Begin and End loop NextVisibleRows, start each row with DataTableRow and
// each cell with ImGui::TableNextColumn. Column widths, order, visibility and sort are kept by
// Dear ImGui under the id (and in its ini file).
[[nodiscard]] bool BeginDataTable(const char* id, std::span<const DataTableColumn> columns, int rowCount,
                                  ImGuiSelectionBasicStorage* selection, const DataTableOptions& options = {});
// Starts row `index`: a full-width row item (selection, hover, focus, activation) behind the
// cells. While a row has keyboard focus, Left and Right move the focused cell. Returns true
// when the row was activated: double click, or Enter when the focused cell is not editable.
bool DataTableRow(int index);
void EndDataTable();

// Inside a row, after ImGui::TableNextColumn: true while this cell is in edit mode. Then submit
// one editor widget (labelled "##edit"), which gets keyboard focus on the first frame, and
// call EndDataTableCellEdit; otherwise draw the cell as usual. A cell of an editable column
// enters edit mode on F2 or Enter while focused, or on a double click. The session ends when
// the editor is deactivated (Enter, Tab, Escape, a click elsewhere); read the outcome with
// IsItemEditCommitted or IsItemEditCanceled right after the editor.
[[nodiscard]] bool BeginDataTableCellEdit();
void EndDataTableCellEdit();

class [[nodiscard]] DataTableScope
{
public:
    DataTableScope(const char* id, std::span<const DataTableColumn> columns, int rowCount,
                   ImGuiSelectionBasicStorage* selection, const DataTableOptions& options = {})
        : open_(BeginDataTable(id, columns, rowCount, selection, options))
    {
    }
    ~DataTableScope()
    {
        if (open_)
            EndDataTable();
    }
    DataTableScope(const DataTableScope&) = delete;
    DataTableScope& operator=(const DataTableScope&) = delete;
    explicit operator bool() const { return open_; }

private:
    bool open_;
};

class [[nodiscard]] DataTableCellEditScope
{
public:
    DataTableCellEditScope() : open_(BeginDataTableCellEdit()) {}
    ~DataTableCellEditScope()
    {
        if (open_)
            EndDataTableCellEdit();
    }
    DataTableCellEditScope(const DataTableCellEditScope&) = delete;
    DataTableCellEditScope& operator=(const DataTableCellEditScope&) = delete;
    explicit operator bool() const { return open_; }

private:
    bool open_;
};

} // namespace facet
