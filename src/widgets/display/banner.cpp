// Banner (facet/widgets/display.h): planned. A placeholder until Phase 2 implements it.

#include "facet/widgets/display.h"
#include "widgets/content_stub.h"

namespace facet
{

BannerAction Banner(const char* label, bool* open, const char*, const BannerOptions& options)
{
    namespace stub = internal::content;
    if (open != nullptr && !*open)
        return BannerAction::None;  // a dismissed banner takes no space
    stub::Stub("Banner", label, stub::ResolveWidth(options.width, 0.0f), stub::ControlRows(stub::kTallControlRows));
    return BannerAction::None;
}

} // namespace facet