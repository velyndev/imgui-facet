// Number editing on the text field core (text_field_number.h): plain-number parsing and
// formatting, and the number field with steps, steppers and drag mode.

#include "internal/text_field_number.h"

#include <algorithm>
#include <charconv>
#include <climits>
#include <clocale>
#include <cmath>
#include <cstdio>
#include <cstring>

#include <imgui_internal.h>

#include "facet/core/icons.h"
#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "internal/state_storage.h"
#include "internal/text_field_detail.h"
#include "internal/value_drag.h"

namespace facet::internal
{

namespace
{

// Longest number text a field edits; longer typed text is rejected by the buffer size.
constexpr int kNumberTextCapacity = 64;

bool IsBlank(char c)
{
    return c == ' ' || c == '\t';
}

bool IsDigit(char c)
{
    return c >= '0' && c <= '9';
}

// Copies [begin, end) of a printf format into `out`, turning "%%" into "%".
int AppendFormatLiteral(char* out, int capacity, int length, const char* begin, const char* end)
{
    for (const char* p = begin; p < end && length < capacity - 1; ++p)
    {
        if (p[0] == '%' && p + 1 < end && p[1] == '%')
            ++p;
        out[length++] = *p;
    }
    out[length] = 0;
    return length;
}

// The conversion character of a format spec from ImParseFormatFindStart to FindEnd.
char FormatConversion(const char* specBegin, const char* specEnd)
{
    return specEnd > specBegin ? specEnd[-1] : 0;
}

bool IsIntegerConversion(char conversion)
{
    return conversion == 'd' || conversion == 'i' || conversion == 'u' || conversion == 'x' || conversion == 'X' ||
           conversion == 'o' || conversion == 'c';
}

bool IsFloatConversion(char conversion)
{
    return conversion == 'f' || conversion == 'F' || conversion == 'e' || conversion == 'E' || conversion == 'g' ||
           conversion == 'G' || conversion == 'a' || conversion == 'A';
}

// Writes one printf conversion of `value` and puts a dot where the C library put the decimal
// separator of its locale.
int FormatConversionText(char* out, int capacity, const char* spec, double value, bool integerConversion)
{
    int length = 0;
    if (integerConversion)
    {
        const double rounded = std::round(value);
        const int whole = rounded >= static_cast<double>(INT_MAX) ? INT_MAX
                          : rounded <= static_cast<double>(INT_MIN) ? INT_MIN
                                                                    : static_cast<int>(rounded);
        length = std::snprintf(out, static_cast<std::size_t>(capacity), spec, whole);
    }
    else
    {
        length = std::snprintf(out, static_cast<std::size_t>(capacity), spec, value);
    }
    if (length < 0)
    {
        out[0] = 0;
        return 0;
    }
    length = std::min(length, capacity - 1);
    const lconv* locale = std::localeconv();
    const char separator = locale != nullptr && locale->decimal_point != nullptr ? locale->decimal_point[0] : '.';
    if (separator != '.' && separator != 0)
        for (int i = 0; i < length; ++i)
            if (out[i] == separator)
                out[i] = '.';
    return length;
}

} // namespace

// ---- Text ----------------------------------------------------------------------------------

NumberParse ParseNumber(const char* text, double* out)
{
    if (text == nullptr)
        return NumberParse::Empty;
    const char* begin = text;
    const char* end = text + std::strlen(text);
    while (begin < end && IsBlank(*begin))
        ++begin;
    while (end > begin && IsBlank(end[-1]))
        --end;
    if (begin == end)
        return NumberParse::Empty;
    if (end - begin >= kNumberTextCapacity)
        return NumberParse::Invalid;

    // [sign] (digits [sep [digits]] | sep digits) [(e|E) [sign] digits], one separator at most.
    char normalized[kNumberTextCapacity];
    int length = 0;
    const char* p = begin;
    if (*p == '+' || *p == '-')
    {
        if (*p == '-')
            normalized[length++] = '-';
        ++p;
    }
    int mantissaDigits = 0;
    int separators = 0;
    while (p < end && (IsDigit(*p) || *p == '.' || *p == ','))
    {
        if (*p == '.' || *p == ',')
        {
            if (++separators > 1)
                return NumberParse::Invalid;  // "1,234.5", "1.2.3": not guessed
            normalized[length++] = '.';
        }
        else
        {
            ++mantissaDigits;
            normalized[length++] = *p;
        }
        ++p;
    }
    if (mantissaDigits == 0)
        return NumberParse::Invalid;  // "-", ".", "+e5"
    if (p < end && (*p == 'e' || *p == 'E'))
    {
        normalized[length++] = 'e';
        ++p;
        if (p < end && (*p == '+' || *p == '-'))
            normalized[length++] = *p++;
        int exponentDigits = 0;
        while (p < end && IsDigit(*p))
        {
            normalized[length++] = *p++;
            ++exponentDigits;
        }
        if (exponentDigits == 0)
            return NumberParse::Invalid;  // "1e", "1e-"
    }
    if (p != end)
        return NumberParse::Invalid;
    normalized[length] = 0;

    double value = 0.0;
    const std::from_chars_result parsed = std::from_chars(normalized, normalized + length, value, std::chars_format::general);
    if (parsed.ec != std::errc() || parsed.ptr != normalized + length || !std::isfinite(value))
        return NumberParse::Invalid;
    if (value == 0.0)
        value = 0.0;  // no negative zero
    if (out != nullptr)
        *out = value;
    return NumberParse::Ok;
}

int FormatNumber(char* buffer, int bufferSize, double value, const char* format, FormatFn formatter,
                 void* formatterUser, bool integer, bool bare)
{
    if (buffer == nullptr || bufferSize <= 0)
        return 0;
    buffer[0] = 0;
    if (value == 0.0)
        value = 0.0;  // "-0" is never shown
    if (integer && std::isfinite(value))
        value = std::round(value);

    if (formatter != nullptr && !bare)
    {
        const int length = formatter(buffer, bufferSize, value, formatterUser);
        buffer[bufferSize - 1] = 0;
        return std::clamp(length, 0, static_cast<int>(std::strlen(buffer)));
    }
    if (formatter != nullptr)
    {
        // The editable form for a custom formatter: the shortest text that reads back exactly.
        char* last = buffer + bufferSize - 1;
        std::to_chars_result written{};
        if (integer && std::fabs(value) < 9.0e15)
            written = std::to_chars(buffer, last, static_cast<long long>(value));
        else
            written = std::to_chars(buffer, last, value);
        if (written.ec != std::errc())
        {
            buffer[0] = 0;
            return 0;
        }
        *written.ptr = 0;
        return static_cast<int>(written.ptr - buffer);
    }

    const char* fallback = integer ? "%d" : "%.3g";
    const char* fmt = (format != nullptr && format[0] != 0) ? format : fallback;
    const char* specBegin = ImParseFormatFindStart(fmt);
    if (*specBegin == 0)
    {
        // A format without a conversion: the text is shown as written, and edited as a number.
        if (!bare)
            return AppendFormatLiteral(buffer, bufferSize, 0, fmt, fmt + std::strlen(fmt));
        fmt = fallback;
        specBegin = fmt;
    }
    const char* specEnd = ImParseFormatFindEnd(specBegin);
    const char conversion = FormatConversion(specBegin, specEnd);
    char spec[32];
    const int specLength = static_cast<int>(std::min<std::ptrdiff_t>(specEnd - specBegin, static_cast<std::ptrdiff_t>(sizeof(spec)) - 1));
    std::memcpy(spec, specBegin, static_cast<std::size_t>(specLength));
    spec[specLength] = 0;
    bool integerConversion = IsIntegerConversion(conversion);
    if (!integerConversion && !IsFloatConversion(conversion))
    {
        // An unknown conversion: fall back to the default of the field.
        std::snprintf(spec, sizeof(spec), "%s", fallback);
        integerConversion = integer;
    }

    int length = 0;
    if (!bare)
        length = AppendFormatLiteral(buffer, bufferSize, 0, fmt, specBegin);
    char number[kNumberTextCapacity];
    FormatConversionText(number, kNumberTextCapacity, spec, value, integerConversion);
    for (const char* n = number; *n != 0 && length < bufferSize - 1; ++n)
        buffer[length++] = *n;
    buffer[length] = 0;
    if (bare)
    {
        // Only the number: blanks a width spec added are not part of it.
        int start = 0;
        while (start < length && IsBlank(buffer[start]))
            ++start;
        if (start > 0)
        {
            std::memmove(buffer, buffer + start, static_cast<std::size_t>(length - start + 1));
            length -= start;
        }
        while (length > 0 && IsBlank(buffer[length - 1]))
            buffer[--length] = 0;
        return length;
    }
    return AppendFormatLiteral(buffer, bufferSize, length, specEnd, specEnd + std::strlen(specEnd));
}

// ---- Behaviour -----------------------------------------------------------------------------

namespace
{

struct NumberFieldState
{
    char edit[kNumberTextCapacity] = {};  // the text the field edits
    bool typing = false;                 // drag mode: typing a value
    bool activationPending = false;      // drag mode: take the keyboard in the next call
    bool typedOnce = false;              // drag mode: the typing session got the keyboard
    ImRect frame;                        // the frame of the last call (the wheel's hover test)
    float wheelRest = 0.0f;              // fractions of a wheel notch not used yet
};

// What the stepper slot reports back to the field.
struct NumberStepperContext
{
    bool decreaseDisabled = false;
    bool increaseDisabled = false;
    int decreasePresses = 0;
    int increasePresses = 0;
    ImGuiID decreaseId = 0;
    ImGuiID increaseId = 0;
};

void NumberStepperSlot(const TextFieldSlot& slot, void* user)
{
    NumberStepperContext& context = *static_cast<NumberStepperContext*>(user);
    const Strings& strings = GetStrings();
    context.increaseId = ImGui::GetIDWithSeed("increase", nullptr, slot.fieldId);
    context.decreaseId = ImGui::GetIDWithSeed("decrease", nullptr, slot.fieldId);
    // Decrease first in the submission, so the arrows meet them in their visual order.
    const ItemState decrease = TextFieldSlotButton(slot, 1, context.decreaseId, Icon("minus"),
                                                   {.repeat = true,
                                                    .disabled = context.decreaseDisabled || slot.readOnly,
                                                    .name = strings.decrease});
    const ItemState increase = TextFieldSlotButton(slot, 0, context.increaseId, Icon("plus"),
                                                   {.repeat = true,
                                                    .disabled = context.increaseDisabled || slot.readOnly,
                                                    .name = strings.increase});
    context.decreasePresses += decrease.pressed ? 1 : 0;
    context.increasePresses += increase.pressed ? 1 : 0;
}

// Typing filters: what a plain number can contain. Commas pass (ParseNumber reads them).
bool NumberCharAllowed(ImWchar c, bool exponent)
{
    return (c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' || c == ',' || (exponent && (c == 'e' || c == 'E'));
}

int NumberCharFilterReal(ImGuiInputTextCallbackData* data)
{
    return NumberCharAllowed(data->EventChar, true) ? 0 : 1;
}

int NumberCharFilterInteger(ImGuiInputTextCallbackData* data)
{
    return NumberCharAllowed(data->EventChar, false) ? 0 : 1;
}

StepSize CurrentStepSize(const ImGuiIO& io)
{
    return io.KeyCtrl ? StepSize::Coarse : (io.KeyShift ? StepSize::Fine : StepSize::Normal);
}

// Presses of `key` this frame (repeats counted from the elapsed time) when `owner` may read it.
int NumberKeyPresses(ImGuiKey key, ImGuiID owner)
{
    if (!ImGui::TestKeyOwner(key, owner))
        return 0;
    const ImGuiIO& io = ImGui::GetIO();
    return ImGui::GetKeyPressedAmount(key, io.KeyRepeatDelay, io.KeyRepeatRate);
}

} // namespace

NumberFieldResult NumberFieldBehavior(const char* label, double* value, const NumberFieldParams& params)
{
    NumberFieldResult result;
    ImGuiContext* context = ImGui::GetCurrentContext();
    if (context == nullptr || !context->WithinFrameScope || context->CurrentWindow == nullptr ||
        context->CurrentWindow->SkipItems)
        return result;
    ImGuiContext& g = *context;
    ImGuiIO& io = g.IO;
    if (label == nullptr)
        label = "";

    const ImGuiID id = g.CurrentWindow->GetID(label);
    NumberFieldState& state = GetState<NumberFieldState>(id);
    const ImGuiItemFlags itemFlags = g.CurrentItemFlags | g.NextItemData.ItemFlagsSet;
    const bool disabled = (itemFlags & ImGuiItemFlags_Disabled) != 0;
    const bool readOnly = params.readOnly || value == nullptr || (itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    const bool liveScalar = (itemFlags & ImGuiItemFlags_LiveEditOnInputScalar) != 0;
    double current = value != nullptr ? *value : 0.0;

    ValueMapping mapping;
    mapping.min = std::min(params.min, params.max);
    mapping.max = std::max(params.min, params.max);
    mapping.integer = params.integer;
    mapping.wrap = params.wrap;
    mapping = NormalizeMapping(mapping);
    const bool stepping = params.step > 0.0 && std::isfinite(params.step) && !readOnly && !disabled;
    const double stepFast = params.stepFast > 0.0 && std::isfinite(params.stepFast) ? params.stepFast : params.step;

    bool markEdited = false;
    const auto write = [&](double next) {
        if (readOnly || !std::isfinite(next))
            return false;
        next = std::clamp(next, mapping.min, mapping.max);
        if (params.integer)
            next = std::round(next);
        if (next == 0.0)
            next = 0.0;
        if (value == nullptr || next == *value)
            return false;
        *value = next;
        current = next;
        result.changed = true;
        markEdited = true;
        return true;
    };
    const auto stepFrom = [&](double from, int direction, StepSize stepSize, int count) {
        double unit = params.step;
        if (stepSize == StepSize::Coarse)
            unit = stepFast;
        else if (stepSize == StepSize::Fine && !params.integer)
            unit = params.step * kFineDragFactor;
        return OffsetValue(mapping, from, static_cast<double>(direction * count) * unit);
    };

    const bool typingMode = !params.drag || state.typing;
    const bool activeBefore = g.ActiveId == id;
    ImGuiInputTextState* text = activeBefore && typingMode ? ImGui::GetInputTextState(id) : nullptr;

    // ---- Steps while typing: Up, Down, Page Up, Page Down and the wheel, before the text runs ----
    if (text != nullptr && stepping)
    {
        const ImGuiKey keys[] = {ImGuiKey_UpArrow, ImGuiKey_DownArrow, ImGuiKey_PageUp, ImGuiKey_PageDown};
        for (const ImGuiKey key : keys)
            ImGui::SetKeyOwner(key, id);
        g.ActiveIdUsingNavDirMask |= (1 << ImGuiDir_Up) | (1 << ImGuiDir_Down);
        const int lines = NumberKeyPresses(ImGuiKey_UpArrow, id) - NumberKeyPresses(ImGuiKey_DownArrow, id);
        const int pages = NumberKeyPresses(ImGuiKey_PageUp, id) - NumberKeyPresses(ImGuiKey_PageDown, id);
        int notches = 0;
        const bool overField = state.frame.GetWidth() > 0.0f && ImGui::IsMouseHoveringRect(state.frame.Min, state.frame.Max) &&
                               g.HoveredWindow == g.CurrentWindow;
        if (overField)
        {
            ImGui::SetKeyOwner(ImGuiKey_MouseWheelY, id);
            if (ImGui::TestKeyOwner(ImGuiKey_MouseWheelY, id))
            {
                if (io.MouseWheel * state.wheelRest < 0.0f)
                    state.wheelRest = 0.0f;
                state.wheelRest += io.MouseWheel;
                notches = static_cast<int>(std::trunc(state.wheelRest));
                state.wheelRest -= static_cast<float>(notches);
            }
        }
        else
        {
            state.wheelRest = 0.0f;
        }
        if (lines != 0 || pages != 0 || notches != 0)
        {
            // From the typed number when it reads as one, else from the value.
            double base = current;
            double typed = 0.0;
            if (ParseNumber(text->TextA.Data, &typed) == NumberParse::Ok)
                base = typed;
            double next = base;
            const StepSize stepSize = CurrentStepSize(io);
            if (lines != 0)
                next = stepFrom(next, lines > 0 ? 1 : -1, stepSize, std::abs(lines));
            if (notches != 0)
                next = stepFrom(next, notches > 0 ? 1 : -1, stepSize, std::abs(notches));
            if (pages != 0)
                next = stepFrom(next, pages > 0 ? 1 : -1, StepSize::Coarse, std::abs(pages));
            write(next);
            result.stepped = true;
            FormatNumber(state.edit, kNumberTextCapacity, current, params.format, params.formatter,
                         params.formatterUser, params.integer, true);
            text->ReloadUserBufAndSelectAll();
        }
    }

    // ---- The text: the bare number while typing, the formatted value otherwise ----
    char display[128];
    const char* displayText = nullptr;
    const bool showValue = !(activeBefore && typingMode);
    if (showValue && !state.activationPending)
    {
        if (params.mixed)
            state.edit[0] = 0;
        else
            FormatNumber(state.edit, kNumberTextCapacity, current, params.format, params.formatter, params.formatterUser,
                         params.integer, true);
    }
    if (showValue && !params.mixed)
    {
        FormatNumber(display, static_cast<int>(sizeof(display)), current, params.format, params.formatter,
                     params.formatterUser, params.integer, false);
        if (std::strcmp(display, state.edit) != 0)
            displayText = display;
    }

    NumberStepperContext steppers;
    steppers.decreaseDisabled = !mapping.wrap && current <= mapping.min;
    steppers.increaseDisabled = !mapping.wrap && current >= mapping.max;

    TextFieldParams field;
    field.placeholder = params.placeholder;
    field.suffix = params.unit;
    field.selectAllOnFocus = true;
    field.flags = ImGuiInputTextFlags_CallbackCharFilter;
    field.callback = params.integer ? NumberCharFilterInteger : NumberCharFilterReal;
    field.size = params.size;
    field.width = params.width;
    field.readOnly = readOnly;
    field.mixed = params.mixed;
    field.error = params.error;
    field.helpText = params.helpText;
    field.displayText = displayText;
    if (params.steppers && params.step > 0.0)
    {
        field.trailingWidth = TextFieldSlotButtonsWidth(2, params.size);
        field.trailingSlot = NumberStepperSlot;
        field.trailingUser = &steppers;
    }

    ItemState dragItem;
    ItemState labelItem;
    TextFieldHooks hooks;
    hooks.liveEdit = liveScalar ? 1 : 0;
    hooks.trackExternalChanges = false;
    hooks.activate = params.drag && state.activationPending;
    hooks.dragMode = params.drag && !state.typing;
    hooks.dragItem = &dragItem;
    hooks.dragLabel = params.drag;
    hooks.labelItem = &labelItem;
    result.field = TextFieldCall(label, nullptr, state.edit, sizeof(state.edit), field, hooks);
    state.frame = result.field.frame;
    if (markEdited)
        ImGui::MarkItemEdited(id);
    markEdited = false;

    // ---- Typed text ----
    if (typingMode && result.field.edited && !hooks.dragMode)
    {
        double typed = 0.0;
        const NumberParse parsed = ParseNumber(state.edit, &typed);
        if (parsed == NumberParse::Ok)
            write(typed);
        else if (!result.field.active)
            result.parseFailed = true;  // the field shows the value again from the next frame on
    }
    result.typing = result.field.active && typingMode;

    // ---- Drag mode ----
    if (params.drag)
    {
        if (state.activationPending)
        {
            state.activationPending = false;
            state.typedOnce = result.field.active;
            if (!result.field.active)
                state.typing = false;
        }
        else if (state.typing && !result.field.active)
        {
            state.typing = false;  // the typing session ended: drag mode from the next frame on
        }
        if (hooks.dragMode)
        {
            const Theme& t = T();
            const float threshold = Px(t.drag.startThreshold);
            const bool pastThreshold = io.MouseDragMaxDistanceSqr[ImGuiMouseButton_Left] >= threshold * threshold;
            double speed = params.speed > 0.0 ? params.speed / static_cast<double>(Px(1.0f)) : 0.0;
            if (speed <= 0.0)
            {
                if (std::isfinite(mapping.min) && std::isfinite(mapping.max) && mapping.max > mapping.min)
                    speed = (mapping.max - mapping.min) / std::max(1.0, static_cast<double>(result.field.frame.GetWidth()));
                else
                    speed = std::max(params.step > 0.0 ? params.step : 1.0, params.integer ? 1.0 : 0.0) /
                            std::max(1.0, static_cast<double>(Px(t.spacing.xs)));
            }
            ValueDragOptions drag;
            drag.mapping = mapping;
            drag.grabRadius = -1.0f;
            drag.speed = speed;
            drag.keyStep = params.step > 0.0 ? params.step : 0.0;
            drag.readOnly = readOnly;
            const auto run = [&](ItemState item, bool keyboard) {
                if (item.id == 0)
                    return;
                // A press becomes a drag only after the start threshold, so a click types instead.
                if (item.held && !pastThreshold)
                    item.held = false;
                drag.keyboard = keyboard;
                drag.wheel = keyboard;
                double next = current;
                const ValueDragResult edit = ValueDragBehavior(item, &next, item.rect, drag);
                result.dragging = result.dragging || edit.dragging;
                if (edit.changed && value != nullptr && !readOnly)
                {
                    *value = next;
                    current = next;
                    result.changed = true;
                }
                if (edit.keyboard && edit.changed)
                    result.stepped = true;
                const bool click = item.pressed && (item.keyboardPressed || !pastThreshold);
                if (click && !readOnly && !disabled)
                {
                    state.typing = true;
                    state.activationPending = true;
                }
            };
            run(dragItem, true);
            run(labelItem, false);
        }
    }

    // ---- Steppers ----
    const int stepperPresses = steppers.increasePresses - steppers.decreasePresses;
    if (stepping && (steppers.increasePresses > 0 || steppers.decreasePresses > 0))
    {
        const StepSize stepSize = CurrentStepSize(io);
        double next = current;
        if (stepperPresses != 0)
            next = stepFrom(current, stepperPresses > 0 ? 1 : -1, stepSize, std::abs(stepperPresses));
        if (write(next))
        {
            result.stepped = true;
            // The stepper is the active part while held: its press and release are one edit session.
            ImGui::MarkItemEdited(steppers.increasePresses > 0 ? steppers.increaseId : steppers.decreaseId);
            markEdited = false;
        }
    }
    if (markEdited)
        ImGui::MarkItemEdited(id);
    result.field.committed = IsItemEditCommitted();
    result.field.canceled = IsItemEditCanceled();
    return result;
}

} // namespace facet::internal
