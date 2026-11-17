// TruncatedText (facet/widgets/display.h): planned. A placeholder until Phase 2 implements it.

#include <algorithm>

#include "facet/widgets/display.h"
#include "widgets/content_stub.h"

namespace facet
{

void TruncatedText(const char* text, const TruncatedTextOptions& options)
{
    namespace stub = internal::content;
    const float lines = static_cast<float>(std::max(1, options.maxLines));
    stub::Stub("TruncatedText", text, stub::ResolveWidth(options.width, 0.0f), stub::LineHeight(options.style) * lines);
}

} // namespace facet