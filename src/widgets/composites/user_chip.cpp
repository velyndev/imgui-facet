// UserChip (facet/widgets/composites.h): planned. A placeholder until Phase 2 implements it.

#include "facet/widgets/composites.h"
#include "widgets/content_stub.h"

namespace facet
{

bool UserChip(const char* label, ImageHandle, const UserChipOptions& options)
{
    namespace stub = internal::content;
    const float height = ControlHeight(options.size);
    const float width = options.compact ? height : height + stub::TextWidth(label, ControlTextStyle(options.size));
    stub::Stub("UserChip", label, width, height);
    return false;
}

} // namespace facet