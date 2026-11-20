// Helpers shared by the text-like inputs (text_inputs_shared.h).

#include "widgets/inputs/text_inputs_shared.h"

#include <cstring>

namespace facet::internal::text_inputs
{

namespace
{

// What is known about Caps Lock: -1 unknown, 0 off, 1 on.
int& CapsLockKnown()
{
    static int known = -1;
    return known;
}

} // namespace

bool CapsLockLikelyOn()
{
    return CapsLockKnown() == 1;
}

void UpdateCapsLockGuess()
{
    if (ImGui::GetCurrentContext() == nullptr)
        return;
    const ImGuiIO& io = ImGui::GetIO();
    int& known = CapsLockKnown();
    // A press of the key flips what is known; while nothing is known it tells nothing.
    if (ImGui::IsKeyPressed(ImGuiKey_CapsLock, false) && known >= 0)
        known = known == 1 ? 0 : 1;
    const bool shift = io.KeyShift;
    for (const ImWchar c : io.InputQueueCharacters)
    {
        if (c >= 'A' && c <= 'Z')
            known = shift ? 0 : 1;
        else if (c >= 'a' && c <= 'z')
            known = shift ? 1 : 0;
    }
}

void ResetCapsLockGuess()
{
    CapsLockKnown() = -1;
}

bool WriteFieldText(std::string* text, char* buffer, std::size_t bufferSize, const char* value)
{
    if (value == nullptr)
        value = "";
    if (text != nullptr)
    {
        if (*text == value)
            return false;
        try
        {
            text->assign(value);
        }
        catch (...)
        {
            return false;  // out of memory: the old text stays
        }
        return true;
    }
    if (buffer == nullptr || bufferSize == 0)
        return false;
    std::size_t length = std::strlen(value);
    if (length > bufferSize - 1)
    {
        length = bufferSize - 1;
        // Back onto the start of a code point.
        while (length > 0 && (static_cast<unsigned char>(value[length]) & 0xC0) == 0x80)
            --length;
    }
    if (std::strncmp(buffer, value, length) == 0 && buffer[length] == '\0')
        return false;
    std::memmove(buffer, value, length);
    buffer[length] = '\0';
    return true;
}

ImGuiInputTextFlags TextCallbackEvents(ImGuiInputTextFlags events)
{
    constexpr ImGuiInputTextFlags kForwarded = ImGuiInputTextFlags_CallbackCompletion |
                                               ImGuiInputTextFlags_CallbackHistory |
                                               ImGuiInputTextFlags_CallbackAlways |
                                               ImGuiInputTextFlags_CallbackCharFilter | ImGuiInputTextFlags_CallbackEdit;
    return events & kForwarded;
}

} // namespace facet::internal::text_inputs
