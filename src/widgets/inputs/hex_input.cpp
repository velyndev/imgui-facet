// HexInput: planned. Draws the placeholder at the field's default size until Phase 2
// implements the widget in this file on the text field core.

#include "facet/widgets/inputs.h"

#include "widgets/controls_stub.h"

namespace facet
{
namespace
{

void Stub(const char* label, const HexInputOptions& options)
{
    internal::ControlStub("HexInput", label, internal::StubWidth(options.width, 0.0f),
                          internal::StubHeight(options.size, options.error, options.helpText));
}

} // namespace

bool HexInput(const char* label, std::uint32_t*, const HexInputOptions& options)
{
    Stub(label, options);
    return false;
}

bool HexInput(const char* label, std::uint64_t*, const HexInputOptions& options)
{
    Stub(label, options);
    return false;
}

} // namespace facet
