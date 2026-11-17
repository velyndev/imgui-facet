// Badge (facet/widgets/display.h): planned. A placeholder until Phase 2 implements it.

#include "facet/widgets/display.h"
#include "widgets/content_stub.h"

namespace facet
{

void Badge(const char* text, const BadgeOptions&)
{
    namespace stub = internal::content;
    const float height = ControlHeight(ControlSize::Small);
    stub::Stub("Badge", text, stub::TextWidth(text, TextStyle::Footnote, height), height);
}

} // namespace facet