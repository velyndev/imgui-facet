// IconPicker: planned. Draws the placeholder at the field's default size until Phase 2
// implements the widget in this file on the popup host.

#include "facet/widgets/pickers.h"

#include "widgets/controls_stub.h"

namespace facet
{
namespace
{

void Stub(const char* label, const IconPickerOptions& options)
{
    namespace stub = internal::controls;
    stub::ControlStub("IconPicker", label, stub::StubWidth(options.width, 0.0f),
                          stub::StubHeight(options.size, options.error, options.helpText));
}

} // namespace

bool IconPicker(const char* label, Icon*, const IconPickerOptions& options)
{
    Stub(label, options);
    return false;
}

bool IconPicker(const char* label, Icon*, std::span<const ChoiceItem>, const IconPickerOptions& options)
{
    Stub(label, options);
    return false;
}

} // namespace facet
