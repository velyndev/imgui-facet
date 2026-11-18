// Knob: planned. Draws the placeholder at the knob's default size until Phase 2 implements the
// widget in this file.

#include "facet/widgets/sliders.h"

#include "widgets/controls_stub.h"

namespace facet
{
namespace
{

void Stub(const char* label, const KnobOptions& options)
{
    namespace stub = internal::controls;
    const float diameter = stub::StubDial(options.diameter, options.size);
    // The label line sits under the dial.
    const float height = diameter + stub::StubLabelLine(label) + stub::StubMessageHeight(options.error, options.helpText);
    stub::ControlStub("Knob", label, diameter, height);
}

} // namespace

bool Knob(const char* label, float*, float, float, const KnobOptions& options)
{
    Stub(label, options);
    return false;
}

bool Knob(const char* label, int*, int, int, const KnobOptions& options)
{
    Stub(label, options);
    return false;
}

} // namespace facet
