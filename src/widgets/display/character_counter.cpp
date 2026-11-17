// CharacterCounter (facet/widgets/display.h): planned. A placeholder until Phase 2 implements it.

#include <cstdio>

#include "facet/widgets/display.h"
#include "widgets/content_stub.h"

namespace facet
{

void CharacterCounter(int count, int max, const CharacterCounterOptions& options)
{
    namespace stub = internal::content;
    const float height = ControlHeight(ControlSize::Small);
    if (options.ring)
    {
        stub::Stub("CharacterCounter", nullptr, height, height);
        return;
    }
    char text[32];
    if (max > 0)
        std::snprintf(text, sizeof(text), "%d / %d", count, max);
    else
        std::snprintf(text, sizeof(text), "%d", count);
    stub::Stub("CharacterCounter", text, stub::TextWidth(text, TextStyle::Footnote, height), height);
}

} // namespace facet