// HexInput: a fixed-width hexadecimal field on the text field core (src/internal/text_field.h),
// in overtype mode. The number core does not fit: it edits a double, which holds 53 bits exactly,
// and a 64-bit value needs all 64. The field edits its own digit text: a Dear ImGui callback keeps
// it at the field's width after every edit (a typed digit replaces the one after the caret,
// deleted digits become zeros), Up and Down change the digit after the caret by one with the carry,
// and the text is read back into the value on Enter, Tab or focus loss.

#include "facet/widgets/inputs.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include <imgui.h>
#include <imgui_internal.h>

#include "internal/control.h"
#include "internal/state_storage.h"
#include "internal/text_field.h"
#include "widgets/inputs/value_inputs_common.h"

namespace facet
{
namespace
{

constexpr int kHexTextSize = 48;

struct HexInputState
{
    char edit[kHexTextSize] = {};    // the digits the field edits
    std::uint64_t start = 0;         // the value when the edit began, for Escape
    bool editing = false;
};

struct HexCallbackContext
{
    int digits = 8;
    bool uppercase = true;
};

int HexDigitValue(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

// Writes `value` as exactly `digits` hex digits (more when the value needs them).
int HexFormat(char* out, int size, std::uint64_t value, int digits, bool uppercase)
{
    const char* table = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    char reversed[17];
    int count = 0;
    do
    {
        reversed[count++] = table[value & 0xF];
        value >>= 4;
    } while (value != 0 && count < 16);
    while (count < digits && count < 16)
        reversed[count++] = '0';
    const int length = std::min(count, size - 1);
    for (int i = 0; i < length; ++i)
        out[i] = reversed[count - 1 - i];
    out[length] = 0;
    return length;
}

// The shown text: "0x" before the digits with `prefix`, a gap after every two with `groupBytes`.
void HexDisplay(char* out, int size, const char* digits, const HexInputOptions& options)
{
    int length = 0;
    const auto put = [&](char c) {
        if (length < size - 1)
            out[length++] = c;
    };
    if (options.prefix)
    {
        put('0');
        put('x');
    }
    const int count = static_cast<int>(std::strlen(digits));
    for (int i = 0; i < count; ++i)
    {
        if (options.groupBytes && i > 0 && (count - i) % 2 == 0)
            put(' ');
        put(digits[i]);
    }
    out[length] = 0;
}

// Reads digits back; false when there are none or one is not a hex digit.
bool HexParse(const char* text, std::uint64_t* out)
{
    std::uint64_t value = 0;
    int count = 0;
    for (const char* p = text; *p != 0; ++p)
    {
        const int digit = HexDigitValue(*p);
        if (digit < 0)
            return false;
        if (count >= 16 && value != 0)
            return false;
        value = (value << 4) | static_cast<std::uint64_t>(digit);
        ++count;
    }
    if (count == 0)
        return false;
    *out = value;
    return true;
}

// Hex digits only, in the field's case; keeps the text exactly `digits` long after every edit.
int HexCallback(ImGuiInputTextCallbackData* data)
{
    const HexCallbackContext& context = *static_cast<const HexCallbackContext*>(data->UserData);
    if (data->EventFlag == ImGuiInputTextFlags_CallbackCharFilter)
    {
        const ImWchar c = data->EventChar;
        if (c >= 0x80 || HexDigitValue(static_cast<char>(c)) < 0)
            return 1;
        data->EventChar = static_cast<ImWchar>(context.uppercase ? ImToUpper(static_cast<char>(c))
                                                                 : (c >= 'A' && c <= 'F' ? c - 'A' + 'a' : c));
        return 0;
    }
    if (data->EventFlag == ImGuiInputTextFlags_CallbackAlways)
    {
        if (data->BufTextLen > context.digits)
        {
            // Overtype: the digits typed before the caret replace the ones after it; at the end of a
            // full field the typed digits are dropped.
            int extra = data->BufTextLen - context.digits;
            const int after = std::min(extra, data->BufTextLen - data->CursorPos);
            if (after > 0)
            {
                data->DeleteChars(data->CursorPos, after);
                extra -= after;
            }
            if (extra > 0)
                data->DeleteChars(data->CursorPos - extra, extra);
        }
        else if (data->BufTextLen < context.digits)
        {
            // Deleted digits become zeros at the caret, so the width stays and the caret keeps its place.
            char zeros[17];
            const int missing = std::min(16, context.digits - data->BufTextLen);
            std::memset(zeros, '0', static_cast<std::size_t>(missing));
            zeros[missing] = 0;
            const int caret = data->CursorPos;
            data->InsertChars(caret, zeros);
            data->CursorPos = caret;
            data->SelectionStart = data->SelectionEnd = caret;
        }
    }
    return 0;
}

template <class Value>
bool RunHexInput(const char* label, Value* value, const HexInputOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    ImGuiContext& g = *GImGui;
    constexpr int maxDigits = static_cast<int>(sizeof(Value) * 2);
    const int digits = options.digits > 0 ? std::clamp(options.digits, 1, maxDigits) : maxDigits;
    const std::uint64_t mask = digits >= 16 ? ~std::uint64_t{0} : ((std::uint64_t{1} << (digits * 4)) - 1);
    const ImGuiID id = window->GetID(label);
    HexInputState& state = internal::GetState<HexInputState>(id);
    const bool readOnly = options.readOnly || value == nullptr;
    const std::uint64_t current = value != nullptr ? static_cast<std::uint64_t>(*value) : 0;
    bool written = false;

    const bool active = g.ActiveId == id;
    ImGuiInputTextState* text = active ? ImGui::GetInputTextState(id) : nullptr;
    if (!active)
    {
        state.editing = false;
        if (options.mixed)
            state.edit[0] = 0;
        else
            HexFormat(state.edit, kHexTextSize, current, digits, options.uppercase);
    }
    else if (!state.editing)
    {
        state.editing = true;
        state.start = current;
    }

    // Up and Down change the digit after the caret by one, with the carry, and write at once.
    if (text != nullptr && !readOnly)
    {
        ImGui::SetKeyOwner(ImGuiKey_UpArrow, id);
        ImGui::SetKeyOwner(ImGuiKey_DownArrow, id);
        g.ActiveIdUsingNavDirMask |= (1 << ImGuiDir_Up) | (1 << ImGuiDir_Down);
        const int up = ImGui::GetKeyPressedAmount(ImGuiKey_UpArrow, g.IO.KeyRepeatDelay, g.IO.KeyRepeatRate);
        const int down = ImGui::GetKeyPressedAmount(ImGuiKey_DownArrow, g.IO.KeyRepeatDelay, g.IO.KeyRepeatRate);
        std::uint64_t typed = 0;
        if (up != down && HexParse(text->TextA.Data, &typed))
        {
            const int length = text->TextLen;
            const int index = std::clamp(text->GetCursorPos(), 0, std::max(0, length - 1));
            const int shift = (length - 1 - index) * 4;
            const std::uint64_t unit = shift < 64 ? std::uint64_t{1} << shift : 0;
            const std::uint64_t delta = unit * static_cast<std::uint64_t>(up > down ? up - down : down - up);
            const std::uint64_t next = (up > down ? typed + delta : typed - delta) & mask;
            HexFormat(state.edit, kHexTextSize, next, digits, options.uppercase);
            internal::TextFieldSetSelection(id, index, index);  // the caret stays before the stepped digit
            if (value != nullptr && static_cast<std::uint64_t>(*value) != next)
            {
                *value = static_cast<Value>(next);
                written = true;
            }
        }
    }

    char display[kHexTextSize * 2];
    HexDisplay(display, static_cast<int>(sizeof(display)), state.edit, options);
    HexCallbackContext context{digits, options.uppercase};
    internal::TextFieldParams params;
    params.flags = ImGuiInputTextFlags_CallbackCharFilter | ImGuiInputTextFlags_CallbackAlways;
    params.callback = HexCallback;
    params.callbackUser = &context;
    params.size = options.size;
    params.width = options.width;
    params.readOnly = readOnly;
    params.mixed = options.mixed;
    params.error = internal::ValueInputError(id, options.error);
    params.helpText = options.helpText;
    params.displayText = options.mixed ? nullptr : display;
    const internal::TextFieldResult field = internal::TextFieldBehavior(label, state.edit, sizeof(state.edit), params);
    if (written)
        ImGui::MarkItemEdited(id);

    if (!field.active && state.editing)
    {
        state.editing = false;
        if (field.canceled && value != nullptr && !readOnly && static_cast<std::uint64_t>(*value) != state.start)
        {
            // Escape also takes back the steps of the session.
            *value = static_cast<Value>(state.start);
            written = true;
        }
        else if (field.committed && value != nullptr && !readOnly)
        {
            std::uint64_t parsed = 0;
            if (HexParse(state.edit, &parsed))
            {
                parsed &= mask;
                if (static_cast<std::uint64_t>(*value) != parsed)
                {
                    *value = static_cast<Value>(parsed);
                    written = true;
                }
                internal::ValueInputSetRejected(id, false);
            }
            else
            {
                internal::ValueInputSetRejected(id, true);
            }
        }
    }
    return written;
}

} // namespace

bool HexInput(const char* label, std::uint32_t* value, const HexInputOptions& options)
{
    return RunHexInput(label, value, options);
}

bool HexInput(const char* label, std::uint64_t* value, const HexInputOptions& options)
{
    return RunHexInput(label, value, options);
}

} // namespace facet
