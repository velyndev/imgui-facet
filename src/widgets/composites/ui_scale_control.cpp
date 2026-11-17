// UiScaleControl (facet/widgets/composites.h): planned. A placeholder until Phase 2 implements it.

#include "facet/widgets/composites.h"
#include "widgets/content_stub.h"

namespace facet
{

bool UiScaleControl(const char* label, float*, float, float, const UiScaleControlOptions& options)
{
    namespace stub = internal::content;
    stub::Stub("UiScaleControl", label, stub::ResolveWidth(options.width, 0.0f), ControlHeight(options.size));
    return false;
}

} // namespace facet