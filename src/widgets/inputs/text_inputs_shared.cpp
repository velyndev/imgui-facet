// Helpers shared by the text-like inputs (text_inputs_shared.h).

#include "widgets/inputs/text_inputs_shared.h"

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

ImGuiInputTextFlags TextCallbackEvents(ImGuiInputTextFlags events)
{
    constexpr ImGuiInputTextFlags kForwarded = ImGuiInputTextFlags_CallbackCompletion |
                                               ImGuiInputTextFlags_CallbackHistory |
                                               ImGuiInputTextFlags_CallbackAlways |
                                               ImGuiInputTextFlags_CallbackCharFilter | ImGuiInputTextFlags_CallbackEdit;
    return events & kForwarded;
}

} // namespace facet::internal::text_inputs
