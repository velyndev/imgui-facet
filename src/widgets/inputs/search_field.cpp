// SearchField: planned. Draws the placeholder at the field's default size until Phase 2
// implements the widget in this file on the text field core.

#include "facet/widgets/inputs.h"

#include "widgets/controls_stub.h"

namespace facet
{
namespace
{

void Stub(const char* label, const SearchFieldOptions& options)
{
    namespace stub = internal::controls;
    stub::ControlStub("SearchField", label, stub::StubWidth(options.width, 0.0f), ControlHeight(options.size));
}

} // namespace

bool SearchField(const char* label, std::string*, const SearchFieldOptions& options)
{
    Stub(label, options);
    return false;
}

bool SearchField(const char* label, char*, std::size_t, const SearchFieldOptions& options)
{
    Stub(label, options);
    return false;
}

} // namespace facet
