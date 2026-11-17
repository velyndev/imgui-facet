// Avatar (facet/widgets/media.h): planned. A placeholder until Phase 2 implements it.

#include "facet/widgets/media.h"
#include "widgets/content_stub.h"

namespace facet
{

bool Avatar(const char* label, ImageHandle, const AvatarOptions& options)
{
    namespace stub = internal::content;
    const float diameter = options.diameter > 0.0f ? Px(options.diameter) : ControlHeight(options.size);
    stub::Stub("Avatar", label, stub::ContentWidth("Avatar", diameter), diameter);
    return false;
}

} // namespace facet