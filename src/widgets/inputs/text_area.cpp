// TextArea: planned. Draws the placeholder at the field's default size until Phase 2
// implements the widget in this file on the text field core.

#include "facet/widgets/inputs.h"

#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "widgets/controls_stub.h"

namespace facet
{
namespace
{

void Stub(const char* label, const TextAreaOptions& options)
{
    namespace stub = internal::controls;
    const int lines = options.minLines > 0 ? options.minLines : 1;
    const float lineHeight = MeasureText(TextStyle::Body, "Ag").y;
    const float height = stub::StubLabelLine(label) + static_cast<float>(lines) * lineHeight
                         + 2.0f * Px(T().spacing.sm) + stub::StubMessageHeight(options.error, options.helpText);
    stub::ControlStub("TextArea", label, stub::StubWidth(options.width, 0.0f), height);
}

} // namespace

bool TextArea(const char* label, std::string*, const TextAreaOptions& options)
{
    Stub(label, options);
    return false;
}

bool TextArea(const char* label, char*, std::size_t, const TextAreaOptions& options)
{
    Stub(label, options);
    return false;
}

} // namespace facet
