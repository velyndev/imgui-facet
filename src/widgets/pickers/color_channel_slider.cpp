// ColorChannelSlider: planned. Draws the placeholder at the slider's default size until Phase 2
// implements the widget in this file.

#include "facet/widgets/pickers.h"

#include "widgets/controls_stub.h"

namespace facet
{

bool ColorChannelSlider(const char* label, ImVec4*, const ColorChannelSliderOptions& options)
{
    namespace stub = internal::controls;
    const ImVec2 size =
        stub::StubSliderSize(options.orientation, options.width, options.height, options.size, nullptr, nullptr);
    stub::ControlStub("ColorChannelSlider", label, size.x, size.y);
    return false;
}

} // namespace facet
