// ShortcutHint (facet/widgets/display.h): planned. A placeholder until Phase 2 implements it.

#include "facet/widgets/display.h"
#include "widgets/content_stub.h"

namespace facet
{

void ShortcutHint(const char* text, ImGuiKeyChord, const ShortcutHintOptions& options)
{
    namespace stub = internal::content;
    stub::Stub("ShortcutHint", text, stub::ResolveWidth(options.width, 0.0f), ControlHeight(ControlSize::Small));
}

void ShortcutHint(const char* text, ShortcutId, const ShortcutHintOptions& options)
{
    namespace stub = internal::content;
    stub::Stub("ShortcutHint", text, stub::ResolveWidth(options.width, 0.0f), ControlHeight(ControlSize::Small));
}

} // namespace facet