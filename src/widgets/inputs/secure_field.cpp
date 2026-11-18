// SecureField: planned. Draws the placeholder at the field's default size until Phase 2
// implements the widget in this file on the text field core.

#include "facet/widgets/inputs.h"

#include "widgets/controls_stub.h"

namespace facet
{
namespace
{

void Stub(const char* label, const SecureFieldOptions& options)
{
    namespace stub = internal::controls;
    stub::ControlStub("SecureField", label, stub::StubWidth(options.width, 0.0f),
                          stub::StubHeight(options.size, options.error, options.helpText));
}

} // namespace

bool SecureField(const char* label, std::string*, const SecureFieldOptions& options)
{
    Stub(label, options);
    return false;
}

bool SecureField(const char* label, char*, std::size_t, const SecureFieldOptions& options)
{
    Stub(label, options);
    return false;
}

} // namespace facet
