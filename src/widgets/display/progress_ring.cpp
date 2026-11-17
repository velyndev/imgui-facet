// ProgressRing (facet/widgets/display.h): planned. A placeholder until Phase 2 implements it.

#include "facet/widgets/display.h"
#include "widgets/content_stub.h"

namespace facet
{

void ProgressRing(const char* label, float, const ProgressRingOptions& options)
{
    namespace stub = internal::content;
    const float diameter = options.diameter > 0.0f ? Px(options.diameter) : ControlHeight(options.size);
    stub::Stub("ProgressRing", label,
               stub::ContentWidth("ProgressRing", diameter + stub::TextWidth(label, ControlTextStyle(options.size))), diameter);
}

} // namespace facet