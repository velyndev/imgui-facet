// Checkbox: planned. Draws the placeholder at the checkbox's default size until Phase 2
// implements the widget in this file.

#include "facet/widgets/selection.h"

#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "widgets/controls_stub.h"

namespace facet
{
namespace
{

void Stub(const char* label, const CheckboxOptions& options)
{
    const Theme& t = T();
    const float content = Px(t.metrics.checkboxSize) + Px(t.spacing.sm) + internal::StubLabelWidth(label, options.size);
    internal::ControlStub("Checkbox", label, content, internal::StubHeight(options.size, options.error, options.helpText));
}

} // namespace

bool Checkbox(const char* label, bool*, const CheckboxOptions& options)
{
    Stub(label, options);
    return false;
}

bool Checkbox(const char* label, CheckState*, const CheckboxOptions& options)
{
    Stub(label, options);
    return false;
}

} // namespace facet
