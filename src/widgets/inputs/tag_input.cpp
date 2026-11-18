// TagInput: planned. Draws the placeholder at the field's default size until Phase 2
// implements the widget in this file on the text field core.

#include "facet/widgets/inputs.h"

#include "widgets/controls_stub.h"

namespace facet
{

bool TagInput(const char* label, std::vector<std::string>*, const TagInputOptions& options)
{
    namespace stub = internal::controls;
    stub::ControlStub("TagInput", label, stub::StubWidth(options.width, 0.0f),
                          stub::StubHeight(options.size, options.error, options.helpText));
    return false;
}

} // namespace facet
