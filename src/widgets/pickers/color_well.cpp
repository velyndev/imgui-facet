// ColorWell: planned. Draws the placeholder at the well's default size until Phase 2
// implements the widget in this file on the popup host.

#include "facet/widgets/pickers.h"

#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "widgets/controls_stub.h"

namespace facet
{
namespace
{

// The swatch is this many control heights wide.
constexpr float kSwatchInControls = 1.5f;
// Width of the hex text in spacing.xxxl.
constexpr float kHexInXxxl = 2.5f;

} // namespace

bool ColorWell(const char* label, ImVec4*, const ColorWellOptions& options)
{
    const float height = ControlHeight(options.size);
    float content = internal::StubLabelWidth(label, options.size) + height * kSwatchInControls;
    if (options.showHex)
        content += Px(T().spacing.xxxl) * kHexInXxxl;
    internal::ControlStub("ColorWell", label, internal::StubWidth(options.width, content),
                          internal::StubHeight(options.size, options.error, options.helpText));
    return false;
}

} // namespace facet
