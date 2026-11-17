// ProgressBar (facet/widgets/display.h): planned. A placeholder until Phase 2 implements it.

#include "facet/widgets/display.h"
#include "widgets/content_stub.h"

namespace facet
{

void ProgressBar(const char* label, float, const ProgressBarOptions& options)
{
    namespace stub = internal::content;
    stub::Stub("ProgressBar", label, stub::ResolveWidth(options.width, 0.0f), ControlHeight(options.size));
}

} // namespace facet