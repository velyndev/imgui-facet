// SatValSquare: planned. Draws the placeholder at the square's default size until Phase 2
// implements the widget in this file.

#include "facet/widgets/pickers.h"

#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "widgets/controls_stub.h"

namespace facet
{

bool SatValSquare(const char* label, ImVec4*, const SatValSquareOptions& options)
{
    const float available = internal::StubWidth(kFill, 0.0f);
    const float preferred = Px(T().spacing.xxxl) * internal::kStubPickerInXxxl;
    const float side = internal::StubWidth(options.width, available < preferred ? available : preferred);
    internal::ControlStub("SatValSquare", label, side, side);
    return false;
}

} // namespace facet
