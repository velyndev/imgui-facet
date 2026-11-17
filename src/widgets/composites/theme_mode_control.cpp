// ThemeModeControl (facet/widgets/composites.h): planned. A placeholder until Phase 2 implements it.

#include "facet/widgets/composites.h"
#include "widgets/content_stub.h"

namespace facet
{

bool ThemeModeControl(const char* label, ThemeMode*, const ThemeModeControlOptions& options)
{
    namespace stub = internal::content;
    // Three segments as wide as the longest segment text.
    const float natural = stub::ContentWidth("ThemeModeControl", 3.0f * stub::TextWidth("System", ControlTextStyle(options.size)));
    stub::Stub("ThemeModeControl", label, stub::ResolveWidth(options.width, natural), ControlHeight(options.size));
    return false;
}

} // namespace facet