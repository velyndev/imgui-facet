// CopyField: planned. Draws the placeholder at the field's default size until Phase 2
// implements the widget in this file.

#include "facet/widgets/inputs.h"

#include "facet/core/text.h"
#include "widgets/controls_stub.h"

namespace facet
{
namespace
{

// Visible lines of a multiline field until the theme has a token for it.
constexpr float kMultilineLines = 3.0f;

} // namespace

bool CopyField(const char* label, const char*, const CopyFieldOptions& options)
{
    float height = internal::StubHeight(options.size, options.error, options.helpText);
    if (options.multiline)
        height += (kMultilineLines - 1.0f) * MeasureText(ControlTextStyle(options.size), "Ag").y;
    internal::ControlStub("CopyField", label, internal::StubWidth(options.width, 0.0f), height);
    return false;
}

} // namespace facet
