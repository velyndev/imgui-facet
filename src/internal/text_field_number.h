#pragma once

// Number editing on top of the text field core (text_field.h): the core behind NumberInput,
// DragNumber, UnitInput, VectorInput, SizeInput, DurationInput and the number fields of
// SliderWithInput and RangeInput.
//
//     double value = *userValue;
//     const NumberFieldResult edit = NumberFieldBehavior(label, &value, {.format = options.format,
//                                                                       .min = options.min,
//                                                                       .max = options.max,
//                                                                       .integer = true,
//                                                                       .steppers = options.step > 0.0});
//     if (edit.changed)
//         *userValue = static_cast<int>(value);
//
// Two layers:
//
// 1. Pure text functions (no Dear ImGui context needed, unit tested directly): ParseNumber reads
//    a plain number, FormatNumber writes one with a printf format or a FormatFn. Both are locale
//    independent: the decimal separator is a dot whatever setlocale says, and a typed comma is
//    read as the decimal separator when it is the only separator in the text ("2,5"), but never
//    next to a dot ("1,234.5" is rejected rather than guessed). Plain numbers only: an optional
//    sign, digits with one separator, and for non-integer fields an exponent ("1e-3"); no
//    expressions, no "inf" or "nan".
//
// 2. NumberFieldBehavior: a text field whose text is the number. While it does not have the
//    keyboard it shows the value through the format (decorations such as "%.1f ms" included) or
//    the FormatFn; when it takes the keyboard the text is the bare number (the format's
//    conversion alone, or the shortest exact form for a FormatFn) with everything selected.
//    Typed text is parsed and written on Enter, Tab or focus loss (ImGuiItemFlags_
//    LiveEditOnInputScalar off, the default), or per keystroke when the caller pushed that flag on
//    (only text that parses is written then). Written values are clamped to [min, max] and
//    rounded for integer fields. Text that does not parse (empty, "-", "1e") writes nothing and
//    the field shows the value again when the edit ends.
//    Stepping writes at once, through the value drag core (value_drag.h): Up and Down while the
//    field has the keyboard (Shift a tenth of a step for non-integer fields, Ctrl or Page Up and
//    Down stepFast), the mouse wheel over the field while it has the keyboard (in drag mode while
//    it is focused or Alt is held, as the value drag core does), and
//    the optional - and + steppers at the end (held: repeating; named by Strings::decrease and
//    Strings::increase; not Tab stops). A step while typing starts from the typed number when it
//    parses. Escape restores the value from the start of the edit, steps included, and reports a
//    canceled edit.
//    Drag mode (DragNumber): while not typing, the field is a drag item: dragging it sideways, or
//    dragging the visible label, changes the value by `speed` per pixel of travel since the press
//    (Shift a tenth, Ctrl ten times, as the value drag core does), after the pointer moved
//    T().drag.startThreshold; Left and Right step while it is focused. A click without a drag,
//    Enter or Space types a value instead; Tab reaches the field once.
//
// Widgets with their own grammar (DurationInput, UnitInput) pass a NumberParseFn and a FormatFn;
// DragNumber's logarithmic scale is NumberFieldParams::scale. HexInput is not built here: its
// std::uint32_t and std::uint64_t values do not fit the double this core edits (integer
// conversions are rounded into the int range, and a double holds 53 bits exactly), so it edits
// its digits with overtype on TextFieldBehavior (text_field.h) and parses them itself.

#include <limits>

#include <imgui.h>

#include "facet/widgets/common.h"
#include "internal/text_field.h"

namespace facet::internal
{

// ---- Text ----------------------------------------------------------------------------------

enum class NumberParse
{
    Ok,       // *out holds the number
    Empty,    // nothing but blanks
    Invalid,  // anything else: a lone sign, two separators, letters, an exponent without digits
};

// Reads a plain number from `text` (spaces and tabs around it are ignored); see the top of this
// file for the accepted forms. Fractions and exponents are read for every field (an integer field
// rounds the result, and its typing filter offers no "e"). A number too large for a double is
// Invalid. *out is written only for Ok.
NumberParse ParseNumber(const char* text, double* out);

// Writes `value` into `buffer` (`bufferSize` bytes, zero terminated) and returns the length.
// With a FormatFn it is called; otherwise `format` (printf in Dear ImGui's dialect, null for
// "%d" when `integer`, "%.3g" otherwise) is applied, an integer conversion (%d, %i, %u, %x) to
// the value rounded into the int range. `bare` writes the editable form: only the format's
// conversion without the text around it, and for a FormatFn the shortest text that reads back
// as exactly `value`. The decimal separator is always a dot.
int FormatNumber(char* buffer, int bufferSize, double value, const char* format, FormatFn formatter,
                 void* formatterUser, bool integer, bool bare);

// Reads a typed text in a field's own grammar ("1m30" or "01:30:00" for DurationInput, "12 ms"
// with a unit suffix for UnitInput). Returns Ok and writes *out, or Empty or Invalid as
// ParseNumber does. It must also read the plain numbers the field writes as its editable form
// (call ParseNumber for those).
using NumberParseFn = NumberParse (*)(void* user, const char* text, double* out);

// ---- Behaviour -----------------------------------------------------------------------------

struct NumberFieldParams
{
    const char* format = nullptr;       // printf; null: "%d" (integer) or "%.3g"
    FormatFn formatter = nullptr;       // used instead of format when set
    void* formatterUser = nullptr;
    double min = -std::numeric_limits<double>::infinity();
    double max = std::numeric_limits<double>::infinity();
    bool integer = false;               // whole numbers: typed values are rounded, steps are whole
    double step = 1.0;                  // value units per Up, Down, wheel notch and stepper press; 0: no stepping
    double stepFast = 10.0;             // with Ctrl, and per Page Up and Down
    bool steppers = false;              // - and + buttons at the end of the field (needs step > 0)
    bool drag = false;                  // drag mode (see the top of this file)
    double speed = 0.0;                 // drag mode: value units per unit of travel; 0 derives it: a finite
                                        // range over the frame width, else one step per spacing.xs
    bool wrap = false;                  // drag mode and steps: a finite range wraps around instead of stopping
    const char* unit = nullptr;         // suffix text inside the frame such as "ms"; null for none
    const char* placeholder = nullptr;  // shown while the field is empty (mixed, or the text deleted while typing)
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;                 // units; rule 6, the visible label included
    bool readOnly = false;
    bool mixed = false;                 // empty with Strings::mixed until the first edit writes
    const char* error = nullptr;
    const char* helpText = nullptr;
    // Members below were added for the frozen number widgets.
    // Drag mode over a finite range: Logarithmic maps the pointer travel across the frame width
    // onto the range through the value drag core's mapping (DragNumberOptions::scale), so equal
    // travel multiplies the value by equal factors; logZero as ValueMapping::logZero. Keys, the
    // wheel and the steppers still step by `step`. Linear, a range the mapping cannot take
    // logarithmically (see NormalizeMapping) or a `speed` above 0 keeps the delta drag.
    ValueScale scale = ValueScale::Linear;
    double logZero = 0.0;
    // Reads typed text instead of ParseNumber; with a parser every printable character can be
    // typed (the number filter is off), so its grammar can use letters and separators.
    NumberParseFn parser = nullptr;
    void* parserUser = nullptr;
    // A trailing slot of the widget's own at the end of the frame (a unit menu, a lock button),
    // as TextFieldParams::trailingWidth, trailingSlot and trailingUser. Used only without
    // steppers, which take the slot themselves.
    float trailingWidth = 0.0f;
    TextFieldSlotFn trailingSlot = nullptr;
    void* trailingUser = nullptr;
};

struct NumberFieldResult
{
    bool changed = false;       // *value was written this frame
    bool stepped = false;       // a key, wheel or stepper step wrote it
    bool dragging = false;      // a drag of the field or the label is under way
    bool typing = false;        // the text has the keyboard (in drag mode: typing a value)
    bool parseFailed = false;   // a typed text was committed that is not a number; nothing was written
    TextFieldResult field;      // the text field underneath
};

// A number field over *value (null: drawn read-only, writes nothing). See the top of this file.
// After the call the field is the last item; IsItemEditCommitted reports one finished edit
// (typed value, step key press with its repeats, stepper press, drag), IsItemEditCanceled the
// Escape that restored it.
NumberFieldResult NumberFieldBehavior(const char* label, double* value, const NumberFieldParams& params = {});

} // namespace facet::internal
