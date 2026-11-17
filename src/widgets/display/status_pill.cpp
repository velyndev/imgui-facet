// StatusPill (facet/widgets/display.h): planned. A placeholder until Phase 2 implements it.

#include "facet/widgets/display.h"
#include "widgets/content_stub.h"

namespace facet
{

void StatusPill(const char* label, const StatusPillOptions& options)
{
    namespace stub = internal::content;
    const float height = ControlHeight(options.size);
    stub::Stub("StatusPill", label,
               stub::ContentWidth("StatusPill", height + stub::TextWidth(label, ControlTextStyle(options.size))), height);
}

} // namespace facet