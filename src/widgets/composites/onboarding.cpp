// BeginOnboarding and EndOnboarding (facet/widgets/composites.h): planned. A placeholder until
// Phase 2 implements them. While *open is true the placeholder sits in the page flow where the
// panel would float; it reports the panel as closed, so the caller submits no step content.

#include "facet/widgets/composites.h"
#include "widgets/content_stub.h"

namespace facet
{

bool BeginOnboarding(const char* label, bool* open, int*, int, const OnboardingOptions& options)
{
    namespace stub = internal::content;
    if (open == nullptr || !*open)
        return false;
    const float width = stub::ResolveWidth(options.width, stub::Spans(stub::kCardWidthSpans), false);
    stub::Stub("Onboarding", label, width, stub::ControlRows(stub::kZoneControlRows));
    return false;
}

void EndOnboarding()
{
    internal::content::StubEnd("BeginOnboarding");
}

} // namespace facet