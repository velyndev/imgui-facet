// MaskedInput: a text field on the text field core whose text follows a mask. The field edits the
// formatted text (slot characters with the literals between them, kept under the id); after every
// edit, typed or pasted, Dear ImGui's edit callback rebuilds it from the slot characters, so
// characters a slot does not take are dropped, literals are inserted as the caret passes them and
// the caret stays behind the same slot. The rest of the mask is drawn after the text as a hint,
// empty slots in the placeholder character. The caller's text gets the slot characters, or the
// formatted text with keepLiterals.

#include "facet/widgets/inputs.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/state_storage.h"
#include "internal/text_field.h"
#include "widgets/inputs/text_inputs_shared.h"

namespace facet
{
namespace
{

// The size of the hint buffer drawn after the text; a longer rest of the mask is cut.
constexpr int kMaskedHintSize = 256;

// One element of a mask.
struct MaskedElement
{
    enum Kind
    {
        Literal,
        Digit,   // '9'
        Letter,  // 'A'
        Any,     // '*'
    };
    Kind kind = Literal;
    unsigned int literal = 0;  // the code point of a literal
};

// Reads the mask element at `*pos` and moves `*pos` past it. Returns false at the end.
bool MaskedNext(const char** pos, MaskedElement* out)
{
    const char* p = *pos;
    if (p == nullptr || *p == '\0')
        return false;
    bool escaped = false;
    if (*p == '\\' && p[1] != '\0')
    {
        escaped = true;
        ++p;
    }
    unsigned int c = 0;
    const int bytes = ImTextCharFromUtf8(&c, p, nullptr);
    p += std::max(1, bytes);
    *pos = p;
    out->literal = c;
    out->kind = MaskedElement::Literal;
    if (!escaped)
    {
        if (c == '9')
            out->kind = MaskedElement::Digit;
        else if (c == 'A')
            out->kind = MaskedElement::Letter;
        else if (c == '*')
            out->kind = MaskedElement::Any;
    }
    return true;
}

bool MaskedAccepts(MaskedElement::Kind kind, unsigned int c)
{
    switch (kind)
    {
    case MaskedElement::Digit:
        return c >= '0' && c <= '9';
    case MaskedElement::Letter:
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c >= 0x80;  // non-ASCII letters too
    case MaskedElement::Any:
        return c >= 0x20;
    case MaskedElement::Literal:
        break;
    }
    return false;
}

void MaskedAppend(std::string& out, unsigned int c)
{
    char bytes[5] = {};
    const int length = ImTextCharToUtf8(bytes, c);
    out.append(bytes, static_cast<std::size_t>(length));
}

// The slot characters in [text, textEnd) as the mask takes them: a literal in its place is
// skipped, a character a slot does not take is dropped, and characters past the last slot are
// dropped. Writes them to `slots` and returns how many characters of the text, counted from its
// start up to `stopAt` (a byte offset; -1 for none), filled slots.
int MaskedExtract(const char* mask, const char* text, const char* textEnd, std::string& slots, int stopAt = -1)
{
    slots.clear();
    int filledBeforeStop = 0;
    const char* m = mask;
    MaskedElement element;
    bool haveElement = MaskedNext(&m, &element);
    for (const char* p = text; p < textEnd && haveElement;)
    {
        unsigned int c = 0;
        const int bytes = std::max(1, ImTextCharFromUtf8(&c, p, textEnd));
        const bool beforeStop = stopAt < 0 || (p - text) < stopAt;
        p += bytes;
        // Literals in the mask: the same character in the text is consumed with them; any other
        // character moves past them to the next slot.
        bool consumed = false;
        while (haveElement && element.kind == MaskedElement::Literal)
        {
            if (c == element.literal)
            {
                consumed = true;
                haveElement = MaskedNext(&m, &element);
                break;
            }
            haveElement = MaskedNext(&m, &element);
        }
        if (consumed || !haveElement)
            continue;
        if (MaskedAccepts(element.kind, c))
        {
            MaskedAppend(slots, c);
            if (beforeStop)
                ++filledBeforeStop;
            haveElement = MaskedNext(&m, &element);
        }
    }
    return filledBeforeStop;
}

// The formatted text of `slots`: each slot character with the literals before it. Returns the byte
// offset just after slot character number `caretSlots` (0: the start).
int MaskedFormat(const char* mask, const std::string& slots, std::string& out, int caretSlots)
{
    out.clear();
    int caret = 0;
    int used = 0;
    const char* s = slots.data();
    const char* sEnd = slots.data() + slots.size();
    const char* m = mask;
    const char* pending = nullptr;  // the first literal waiting for a slot character after it
    MaskedElement element;
    for (const char* at = m; s < sEnd && MaskedNext(&m, &element); at = m)
    {
        if (element.kind == MaskedElement::Literal)
        {
            if (pending == nullptr)
                pending = at;
            continue;
        }
        // The literals before this slot go in now that a character follows them.
        for (const char* l = pending; l != nullptr && l < at;)
        {
            MaskedElement literal;
            if (!MaskedNext(&l, &literal))
                break;
            MaskedAppend(out, literal.literal);
        }
        pending = nullptr;
        unsigned int c = 0;
        const int bytes = std::max(1, ImTextCharFromUtf8(&c, s, sEnd));
        out.append(s, static_cast<std::size_t>(bytes));
        s += bytes;
        ++used;
        if (used == caretSlots)
            caret = static_cast<int>(out.size());
    }
    return caretSlots <= 0 ? 0 : caret;
}

// The rest of the mask after `filled` slots, as the hint drawn after the text: literals as they
// are, slots as `placeholder`.
void MaskedHint(const char* mask, int filled, unsigned int placeholder, char* out, int outSize)
{
    int length = 0;
    int slots = 0;
    bool started = filled == 0;
    const char* m = mask;
    MaskedElement element;
    out[0] = '\0';
    while (MaskedNext(&m, &element))
    {
        const bool slot = element.kind != MaskedElement::Literal;
        if (!started)
        {
            if (slot)
            {
                ++slots;
                started = slots == filled;
            }
            continue;
        }
        char bytes[5] = {};
        const int count = ImTextCharToUtf8(bytes, slot ? placeholder : element.literal);
        if (length + count >= outSize)
            break;
        std::memcpy(out + length, bytes, static_cast<std::size_t>(count));
        length += count;
        out[length] = '\0';
    }
}

// Kept under the field id.
struct MaskedInputState
{
    std::string formatted;  // the text the field edits
    std::string slots;      // scratch for the slot characters
    std::string scratch;    // scratch for formatting
};

struct MaskedCallbackData
{
    const char* mask = nullptr;
    MaskedInputState* state = nullptr;
};

// After every edit: rebuild the formatted text, keeping the caret behind the same slot.
int MaskedEditCallback(ImGuiInputTextCallbackData* data)
{
    if (data->EventFlag != ImGuiInputTextFlags_CallbackEdit)
        return 0;
    MaskedCallbackData& user = *static_cast<MaskedCallbackData*>(data->UserData);
    MaskedInputState& state = *user.state;
    try
    {
        const int before = MaskedExtract(user.mask, data->Buf, data->Buf + data->BufTextLen, state.slots, data->CursorPos);
        const int caret = MaskedFormat(user.mask, state.slots, state.scratch, before);
        if (state.scratch.size() != static_cast<std::size_t>(data->BufTextLen) ||
            std::memcmp(state.scratch.data(), data->Buf, state.scratch.size()) != 0)
        {
            data->DeleteChars(0, data->BufTextLen);
            data->InsertChars(0, state.scratch.data(), state.scratch.data() + state.scratch.size());
        }
        data->CursorPos = std::min(caret, data->BufTextLen);
        data->SelectionStart = data->SelectionEnd = data->CursorPos;
    }
    catch (...)
    {
        // Out of memory: the text stays as typed until the next edit.
    }
    return 0;
}

// The text length of a buffer that may lack its terminator.
std::size_t MaskedBufferLength(const char* buffer, std::size_t bufferSize)
{
    const void* zero = std::memchr(buffer, 0, bufferSize);
    return zero != nullptr ? static_cast<std::size_t>(static_cast<const char*>(zero) - buffer) : bufferSize;
}

template <class Call>
bool MaskedInputCall(const char* label, std::string* text, char* buffer, std::size_t bufferSize, const char* mask,
                     const MaskedInputOptions& options, Call plain)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const bool writable = text != nullptr || (buffer != nullptr && bufferSize > 0);

    internal::TextFieldParams params;
    params.size = options.size;
    params.width = options.width;
    params.readOnly = options.readOnly;
    params.mixed = options.mixed;
    params.error = options.error;
    params.helpText = options.helpText;
    if (mask == nullptr || mask[0] == '\0')
    {
        // No mask: a plain field.
        const internal::TextFieldResult field = plain(label, params);
        return field.edited && writable && !options.readOnly;
    }

    const ImGuiID id = window->GetID(label);
    MaskedInputState& state = internal::GetState<MaskedInputState>(id);
    const ImGuiContext& g = *GImGui;

    // While the field does not have the keyboard, the formatted text follows the caller's text.
    const std::string_view current =
        text != nullptr ? std::string_view(*text)
        : writable      ? std::string_view(buffer, MaskedBufferLength(buffer, bufferSize))
                        : std::string_view();
    if (g.ActiveId != id)
    {
        try
        {
            MaskedExtract(mask, current.data(), current.data() + current.size(), state.slots);
            MaskedFormat(mask, state.slots, state.formatted, 0);
        }
        catch (...)
        {
            state.formatted.clear();
        }
    }

    MaskedCallbackData callbackData{mask, &state};
    params.flags = ImGuiInputTextFlags_CallbackEdit;
    params.callback = MaskedEditCallback;
    params.callbackUser = &callbackData;
    const internal::TextFieldResult field = internal::TextFieldBehavior(label, &state.formatted, params);

    bool written = false;
    if (field.edited && writable && !options.readOnly)
    {
        try
        {
            MaskedExtract(mask, state.formatted.data(), state.formatted.data() + state.formatted.size(), state.slots);
            const std::string& value = options.keepLiterals ? state.formatted : state.slots;
            if (value != current)
                written = internal::text_inputs::WriteFieldText(text, buffer, bufferSize, value.c_str());
        }
        catch (...)
        {
            // Out of memory: the caller's text keeps its old value.
        }
    }

    // The rest of the mask after the text, while it fits.
    const bool showHint = !(options.mixed && state.formatted.empty());
    if (showHint && ImGui::IsRectVisible(field.text.Min, field.text.Max))
    {
        const internal::ControlTextScope scope(internal::GetControlMetrics(options.size));
        unsigned int placeholder = '_';
        if (options.placeholder != nullptr && options.placeholder[0] != '\0')
            ImTextCharFromUtf8(&placeholder, options.placeholder, nullptr);
        // The slots the shown text fills (the formatted text holds exactly the filled slots).
        int filled = 0;
        try
        {
            MaskedExtract(mask, state.formatted.data(), state.formatted.data() + state.formatted.size(), state.slots);
            filled = ImTextCountCharsFromUtf8(state.slots.data(), state.slots.data() + state.slots.size());
        }
        catch (...)
        {
        }
        char hint[kMaskedHintSize];
        MaskedHint(mask, filled, placeholder, hint, kMaskedHintSize);
        const float textWidth = ImGui::CalcTextSize(state.formatted.c_str()).x;
        const float x = field.text.Min.x + textWidth;
        const float hintWidth = ImGui::CalcTextSize(hint).x;
        if (hint[0] != '\0' && x + hintWidth <= field.text.Max.x)
        {
            const bool disabled = (g.CurrentItemFlags & ImGuiItemFlags_Disabled) != 0;
            const float y = CenteredTextY(field.frame.Min.y, field.frame.GetHeight());
            window->DrawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(x, y),
                                      ToU32(internal::ControlSecondaryColor(disabled)), hint);
        }
    }
    return written;
}

} // namespace

bool MaskedInput(const char* label, std::string* text, const char* mask, const MaskedInputOptions& options)
{
    return MaskedInputCall(label, text, nullptr, 0, mask, options,
                           [&](const char* l, const internal::TextFieldParams& p) {
                               return internal::TextFieldBehavior(l, text, p);
                           });
}

bool MaskedInput(const char* label, char* buffer, std::size_t bufferSize, const char* mask,
                 const MaskedInputOptions& options)
{
    return MaskedInputCall(label, nullptr, buffer, bufferSize, mask, options,
                           [&](const char* l, const internal::TextFieldParams& p) {
                               return internal::TextFieldBehavior(l, buffer, bufferSize, p);
                           });
}

} // namespace facet
