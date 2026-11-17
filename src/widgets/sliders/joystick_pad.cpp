// JoystickPad: planned. Draws the placeholder at the pad's default size until Phase 2
// implements the widget in this file.

#include "facet/widgets/sliders.h"

#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "widgets/controls_stub.h"

namespace facet
{
namespace
{

// Default pad diameter in spacing.xxxl until the theme has a token for it.
constexpr float kJoystickInXxxl = 4.0f;

} // namespace

bool JoystickPad(const char* label, ImVec2*, const JoystickPadOptions& options)
{
    const float diameter = options.diameter > 0.0f ? Px(options.diameter) : Px(T().spacing.xxxl) * kJoystickInXxxl;
    internal::ControlStub("JoystickPad", label, diameter,
                          diameter + internal::StubMessageHeight(options.error, options.helpText));
    return false;
}

} // namespace facet
