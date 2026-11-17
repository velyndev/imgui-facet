// TimelineScrubber (facet/widgets/media.h): planned. A placeholder until Phase 2 implements it.

#include "facet/widgets/media.h"
#include "widgets/content_stub.h"

namespace facet
{
namespace
{

void TimelineScrubberStub(const char* label, const TimelineScrubberOptions& options)
{
    namespace stub = internal::content;
    stub::Stub("TimelineScrubber", label, stub::ResolveWidth(options.width, 0.0f),
               stub::ResolveHeight(options.height, stub::ControlRows(stub::kTallControlRows)));
}

} // namespace

bool TimelineScrubber(const char* label, double*, double, const TimelineScrubberOptions& options)
{
    TimelineScrubberStub(label, options);
    return false;
}

bool TimelineScrubber(const char* label, double*, double*, double*, double, const TimelineScrubberOptions& options)
{
    TimelineScrubberStub(label, options);
    return false;
}

} // namespace facet