// StyledText (facet/widgets/display.h): planned. A placeholder until Phase 2 implements it.

#include "facet/widgets/display.h"
#include "widgets/content_stub.h"

namespace facet
{

void StyledText(const char* text, const StyledTextOptions& options)
{
    namespace stub = internal::content;
    const float natural = options.wrap ? 0.0f : stub::ContentWidth("StyledText", stub::TextWidth(text, options.style));
    stub::Stub("StyledText", text, stub::ResolveWidth(options.width, natural), stub::LineHeight(options.style));
}

} // namespace facet