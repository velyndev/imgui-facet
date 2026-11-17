// Spinner (facet/widgets/display.h): planned. A placeholder until Phase 2 implements it.

#include "facet/widgets/display.h"
#include "widgets/content_stub.h"

namespace facet
{

void Spinner(const char* label, const SpinnerOptions& options)
{
    namespace stub = internal::content;
    const float height = ControlHeight(options.size);
    stub::Stub("Spinner", label, stub::ContentWidth("Spinner", height + stub::TextWidth(label, ControlTextStyle(options.size))),
               height);
}

} // namespace facet