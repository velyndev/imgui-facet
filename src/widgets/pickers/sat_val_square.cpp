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
    namespace stub = internal::controls;
    const float available = stub::StubWidth(kFill, 0.0f);
    const float preferred = Px(T().spacing.xxxl) * stub::kStubPickerInXxxl;
    const float side = stub::StubWidth(options.width, available < preferred ? available : preferred);
    stub::ControlStub("SatValSquare", label, side, side);
    return false;
}

} // namespace facet
