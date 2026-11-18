// PathField: planned. Draws the placeholder at the field's default size until Phase 2
// implements the widget in this file on the text field core.

#include "facet/widgets/inputs.h"

#include "widgets/controls_stub.h"

namespace facet
{
namespace
{

void Stub(const char* label, const PathFieldOptions& options)
{
    namespace stub = internal::controls;
    stub::ControlStub("PathField", label, stub::StubWidth(options.width, 0.0f),
                          stub::StubHeight(options.size, options.error, options.helpText));
}

} // namespace

bool PathField(const char* label, std::string*, const PathFieldOptions& options)
{
    Stub(label, options);
    return false;
}

bool PathField(const char* label, char*, std::size_t, const PathFieldOptions& options)
{
    Stub(label, options);
    return false;
}

bool PathFieldBrowsePressed()
{
    return false;
}

} // namespace facet
