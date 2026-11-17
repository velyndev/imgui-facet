// EmptyState (facet/widgets/display.h): planned. A placeholder until Phase 2 implements it.

#include "facet/widgets/display.h"
#include "widgets/content_stub.h"

namespace facet
{

bool EmptyState(const char* label, const EmptyStateOptions& options)
{
    namespace stub = internal::content;
    const float natural = stub::ControlRows(options.compact ? 1.0f : stub::kZoneControlRows);
    stub::Stub("EmptyState", label, stub::ResolveWidth(options.width, 0.0f), stub::ResolveHeight(options.height, natural));
    return false;
}

} // namespace facet