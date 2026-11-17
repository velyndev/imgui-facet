// AvatarGroup (facet/widgets/media.h): planned. A placeholder until Phase 2 implements it.

#include <algorithm>

#include "facet/widgets/media.h"
#include "widgets/content_stub.h"

namespace facet
{

bool AvatarGroup(const char* label, std::span<const AvatarItem> items, const AvatarGroupOptions& options)
{
    namespace stub = internal::content;
    const float diameter = options.diameter > 0.0f ? Px(options.diameter) : ControlHeight(options.size);
    int shown = static_cast<int>(items.size());
    if (options.maxVisible > 0)
        shown = std::min(shown, options.maxVisible);
    stub::Stub("AvatarGroup", label, stub::ContentWidth("AvatarGroup", diameter * static_cast<float>(std::max(1, shown))),
               diameter);
    return false;
}

} // namespace facet