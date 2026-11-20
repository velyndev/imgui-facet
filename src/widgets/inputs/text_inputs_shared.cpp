// Helpers shared by the text-like inputs (text_inputs_shared.h).

#include "widgets/inputs/text_inputs_shared.h"

namespace facet::internal::text_inputs
{

ImGuiInputTextFlags TextCallbackEvents(ImGuiInputTextFlags events)
{
    constexpr ImGuiInputTextFlags kForwarded = ImGuiInputTextFlags_CallbackCompletion |
                                               ImGuiInputTextFlags_CallbackHistory |
                                               ImGuiInputTextFlags_CallbackAlways |
                                               ImGuiInputTextFlags_CallbackCharFilter | ImGuiInputTextFlags_CallbackEdit;
    return events & kForwarded;
}

} // namespace facet::internal::text_inputs
