// Link: planned. Draws the placeholder at the link's default size until Phase 2 implements the
// widget in this file.

#include "facet/widgets/buttons.h"

#include "widgets/controls_stub.h"

namespace facet
{

bool Link(const char* label, const LinkOptions& options)
{
    namespace stub = internal::controls;
    stub::ControlStub("Link", label, stub::StubLabelWidth(label, options.size),
                          ControlHeight(options.size));
    return false;
}

} // namespace facet
