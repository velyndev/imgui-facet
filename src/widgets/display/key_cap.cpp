// KeyCap (facet/widgets/display.h): planned. A placeholder until Phase 2 implements it.

#include "facet/widgets/display.h"
#include "widgets/content_stub.h"

namespace facet
{

void KeyCap(ImGuiKeyChord chord, const KeyCapOptions& options)
{
    namespace stub = internal::content;
    char text[64] = "";
    ChordToText(chord, text, static_cast<int>(sizeof(text)));
    const float height = ControlHeight(options.size);
    stub::Stub("KeyCap", text, stub::ContentWidth("KeyCap", stub::TextWidth(text, TextStyle::Footnote, height)), height);
}

} // namespace facet