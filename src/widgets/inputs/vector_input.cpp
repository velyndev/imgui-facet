// VectorInput: planned. Draws the placeholder at the widget's default size until Phase 2
// implements it in this file on the value drag core.

#include "facet/widgets/inputs.h"

#include "widgets/controls_stub.h"

namespace facet
{
namespace
{

void Stub(const char* label, const VectorInputOptions& options)
{
    namespace stub = internal::controls;
    stub::ControlStub("VectorInput", label, stub::StubWidth(options.width, 0.0f),
                          stub::StubHeight(options.size, options.error, options.helpText));
}

} // namespace

bool VectorInput(const char* label, std::span<float>, const VectorInputOptions& options)
{
    Stub(label, options);
    return false;
}

bool VectorInput(const char* label, std::span<int>, const VectorInputOptions& options)
{
    Stub(label, options);
    return false;
}

bool VectorInput(const char* label, ImVec2*, const VectorInputOptions& options)
{
    Stub(label, options);
    return false;
}

bool VectorInput(const char* label, ImVec4*, const VectorInputOptions& options)
{
    Stub(label, options);
    return false;
}

} // namespace facet
