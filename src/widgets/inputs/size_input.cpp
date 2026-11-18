// SizeInput: planned. Draws the placeholder at the widget's default size until Phase 2
// implements it in this file.

#include "facet/widgets/inputs.h"

#include "widgets/controls_stub.h"

namespace facet
{
namespace
{

void Stub(const char* label, const SizeInputOptions& options)
{
    namespace stub = internal::controls;
    stub::ControlStub("SizeInput", label, stub::StubWidth(options.width, 0.0f),
                          stub::StubHeight(options.size, options.error, options.helpText));
}

} // namespace

bool SizeInput(const char* label, ImVec2*, const SizeInputOptions& options)
{
    Stub(label, options);
    return false;
}

bool SizeInput(const char* label, int*, int*, const SizeInputOptions& options)
{
    Stub(label, options);
    return false;
}

} // namespace facet
