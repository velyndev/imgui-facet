// ColorModeEditor and SampleAnimatedColor: planned. The editor draws the placeholder at its
// default size until Phase 2 implements both in this file.

#include "facet/widgets/pickers.h"

#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "widgets/controls_stub.h"

namespace facet
{
namespace
{

// Rows of the editor: the animation choice, the colors and the speed.
constexpr float kRows = 3.0f;

} // namespace

bool ColorModeEditor(const char* label, AnimatedColor*, const ColorModeEditorOptions& options)
{
    const float control = ControlHeight(ControlSize::Inherit);
    const float height = kRows * control + (kRows - 1.0f) * Px(T().spacing.sm)
                         + internal::StubMessageHeight(options.error, options.helpText);
    internal::ControlStub("ColorModeEditor", label, internal::StubWidth(options.width, 0.0f), height);
    return false;
}

// Planned: until Phase 2 the base color, the same result as Static and reduced motion.
ImVec4 SampleAnimatedColor(const AnimatedColor& animated, double)
{
    return animated.color;
}

} // namespace facet
