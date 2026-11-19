// Text field core (text_field.h): wraps Dear ImGui's text editing (IME, undo, clipboard stay
// Dear ImGui's) and draws the Facet frame around it.
// The functions below are linking stubs of the frozen interface until the text field core is
// implemented in this file: a field submits nothing and reports nothing. Hooks are optional; a
// text field core without per-frame work leaves them null.

#include "internal/text_field.h"

#include "internal/subsystem.h"

namespace facet::internal
{

TextFieldResult TextFieldBehavior(const char*, std::string*, const TextFieldParams&)
{
    return {};
}

TextFieldResult TextFieldBehavior(const char*, char*, std::size_t, const TextFieldParams&)
{
    return {};
}

PopupAnchor TextFieldPopupAnchor(const TextFieldResult&, TextFieldAnchor)
{
    return {};
}

Subsystem TextFieldSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "text_field";
    return subsystem;
}

} // namespace facet::internal
