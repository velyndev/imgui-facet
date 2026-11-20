// TextField: single-line text on the text field core (src/internal/text_field.h). The core does
// the editing (Dear ImGui's own), the frame, the label in front, the placeholder, the leading
// icon, the clear button, the spinner and the message line; this file maps the options onto it.

#include "facet/widgets/inputs.h"

#include "internal/control.h"
#include "internal/text_field.h"
#include "widgets/inputs/text_inputs_shared.h"

namespace facet
{
namespace
{

internal::TextFieldParams TextFieldParamsOf(const TextFieldOptions& options)
{
    internal::TextFieldParams params;
    params.placeholder = options.placeholder;
    params.icon = options.icon;
    params.clearButton = options.clearButton;
    params.maxLength = options.maxLength;
    params.selectAllOnFocus = options.selectAllOnFocus;
    params.callback = options.callback;
    params.callbackUser = options.callbackUser;
    params.flags = options.callback != nullptr ? internal::text_inputs::TextCallbackEvents(options.callbackEvents)
                                               : ImGuiInputTextFlags_None;
    params.size = options.size;
    params.width = options.width;
    params.readOnly = options.readOnly;
    params.loading = options.loading;
    params.mixed = options.mixed;
    params.error = options.error;
    params.helpText = options.helpText;
    return params;
}

} // namespace

bool TextField(const char* label, std::string* text, const TextFieldOptions& options)
{
    if (internal::ControlWindow() == nullptr)
        return false;
    const internal::TextFieldResult field = internal::TextFieldBehavior(label, text, TextFieldParamsOf(options));
    return field.edited && internal::text_inputs::TextWritable(text) && !options.readOnly;
}

bool TextField(const char* label, char* buffer, std::size_t bufferSize, const TextFieldOptions& options)
{
    if (internal::ControlWindow() == nullptr)
        return false;
    const internal::TextFieldResult field =
        internal::TextFieldBehavior(label, buffer, bufferSize, TextFieldParamsOf(options));
    return field.edited && internal::text_inputs::TextWritable(buffer, bufferSize) && !options.readOnly;
}

} // namespace facet
