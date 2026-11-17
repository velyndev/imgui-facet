// InlineCode (facet/widgets/display.h): planned. A placeholder until Phase 2 implements it.

#include "facet/widgets/display.h"
#include "widgets/content_stub.h"

namespace facet
{

bool InlineCode(const char* label, const InlineCodeOptions&)
{
    namespace stub = internal::content;
    const float height = ControlHeight(ControlSize::Small);
    stub::Stub("InlineCode", label, stub::ContentWidth("InlineCode", stub::TextWidth(label, TextStyle::Body, height)), height);
    return false;
}

} // namespace facet