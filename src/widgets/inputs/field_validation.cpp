// FieldValidation and ValidationSummary: planned. Both draw the placeholder until Phase 2
// implements them in this file, with the per-frame message list in the controls subsystem.

#include "facet/widgets/inputs.h"

#include "facet/core/text.h"
#include "widgets/controls_stub.h"

namespace facet
{

void FieldValidation(const char* message, const FieldValidationOptions&)
{
    if (message == nullptr || !internal::StubCanDraw())
        return;
    // One Footnote line under the last item, the width of the content region, with an id under
    // the item it belongs to so several messages in a window never share one.
    ImGui::PushID(static_cast<int>(GImGui->LastItemData.ID));
    internal::ControlStub("FieldValidation", "##message", internal::StubWidth(0.0f, 0.0f),
                          MeasureText(TextStyle::Footnote, "Ag").y);
    ImGui::PopID();
}

bool ValidationSummary(const char* label, const ValidationSummaryOptions& options)
{
    internal::ControlStub("ValidationSummary", label, internal::StubWidth(options.width, 0.0f),
                          ControlHeight(ControlSize::Inherit));
    return false;
}

} // namespace facet
