// Per-frame work of DataTable and its cell editing (display.h). The rest of the display family has
// its own entry (display_frame.cpp). EndFrame drops the tables a caller left open (the scope stack
// reports them by name); Shutdown gives the keys a focused table owns back to Dear ImGui, so
// facet::Shutdown leaves no Facet owner in the host's context.

#include <imgui.h>

#include "internal/keys.h"
#include "internal/subsystem.h"
#include "widgets/display/data_table_internal.h"

namespace facet::internal
{
namespace
{

void DataTableShutdown()
{
    DataTableEndFrame();
    if (ImGui::GetCurrentContext() != nullptr)
    {
        const ImGuiID owners[] = {DataTableKeyOwner()};
        ReleaseKeyOwners(owners);
    }
}

} // namespace

Subsystem WidgetsDataTableSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "widgets_data_table";
    subsystem.endFrame = &DataTableEndFrame;
    subsystem.shutdown = &DataTableShutdown;
    return subsystem;
}

} // namespace facet::internal
