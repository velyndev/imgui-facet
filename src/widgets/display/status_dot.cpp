// StatusDot (facet/widgets/display.h): planned. A placeholder until Phase 2 implements it.

#include "facet/widgets/display.h"
#include "widgets/content_stub.h"

namespace facet
{

void StatusDot(const char* label, const StatusDotOptions&)
{
    namespace stub = internal::content;
    const float height = ControlHeight(ControlSize::Small);
    stub::Stub("StatusDot", label, stub::ContentWidth("StatusDot", height + stub::TextWidth(label, TextStyle::Body)), height);
}

} // namespace facet