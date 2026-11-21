// Per-frame and per-device work of the other pickers of pickers.h: DatePicker, DateRangePicker,
// TimePicker, AnglePicker, IconPicker, ThumbnailPicker and FileBrowser. The entry is in the
// subsystem table (src/core/subsystems.cpp) from the start, so these pickers fill their hooks here
// without touching a shared file.
//
// Shutdown stops the folder reader of FileBrowser (file_browser_parts.h): its worker thread is
// joined and its jobs and results are dropped, so nothing reaches a later Dear ImGui context.

#include "internal/subsystem.h"
#include "widgets/pickers/file_browser_parts.h"

namespace facet::internal
{

Subsystem WidgetsDatePickersSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "widgets_date_pickers";
    subsystem.shutdown = &FileBrowserReaderShutdown;
    return subsystem;
}

} // namespace facet::internal
