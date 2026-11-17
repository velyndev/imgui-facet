// BeginCombo, ComboItem and EndCombo (lists.h). Planned: draws the placeholders of
// src/widgets/structure_stub.h until Phase 2 implements the widget. The list never opens, so
// BeginCombo returns false and EndCombo is never due.

#include "facet/widgets/lists.h"
#include "internal/scope_stack.h"
#include "widgets/structure_stub.h"

namespace facet
{

bool BeginCombo(const char* label, const char*, const ComboOptions& options)
{
    internal::StubItem("BeginCombo", label, internal::StubWidth(options.width, true), ControlHeight(options.size));
    return false;
}

void EndCombo()
{
    // BeginCombo never returns true yet, so any call here is unbalanced; PopScope reports it.
    internal::PopScope("BeginCombo");
}

bool ComboItem(const char* label, bool, const ComboItemOptions&)
{
    internal::StubItem("ComboItem", label, internal::StubWidth(0.0f, true), ControlHeight(ControlSize::Inherit));
    return false;
}

} // namespace facet
