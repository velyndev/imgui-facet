// BeginSettingsRow, EndSettingsRow and IsSettingsRowResetPressed (layout.h). Planned: shows the
// control inside the frame placeholder of src/widgets/structure_stub.h until Phase 2 implements
// the row. The label is pushed on the id stack until EndSettingsRow, as rule 2 requires.

#include "facet/widgets/layout.h"
#include "widgets/structure_stub.h"

namespace facet
{

void BeginSettingsRow(const char* label, const SettingsRowOptions&)
{
    internal::BeginStubFrame("BeginSettingsRow", "SettingsRow", label, true);
}

void EndSettingsRow()
{
    internal::EndStubFrame("BeginSettingsRow");
}

bool IsSettingsRowResetPressed()
{
    return false;
}

} // namespace facet
