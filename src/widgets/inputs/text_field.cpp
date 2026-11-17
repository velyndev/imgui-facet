// TextField: planned. Draws the placeholder at the field's default size until Phase 2
// implements the widget in this file on the text field core.

#include "facet/widgets/inputs.h"

#include "widgets/controls_stub.h"

namespace facet
{
namespace
{

void Stub(const char* label, const TextFieldOptions& options)
{
    internal::ControlStub("TextField", label, internal::StubWidth(options.width, 0.0f),
                          internal::StubHeight(options.size, options.error, options.helpText));
}

} // namespace

bool TextField(const char* label, std::string*, const TextFieldOptions& options)
{
    Stub(label, options);
    return false;
}

bool TextField(const char* label, char*, std::size_t, const TextFieldOptions& options)
{
    Stub(label, options);
    return false;
}

} // namespace facet
