// Per-frame and per-device work of the other pickers of pickers.h: DatePicker, DateRangePicker,
// TimePicker, AnglePicker, IconPicker, ThumbnailPicker and FileBrowser (for example folder reads
// that finish on a worker thread). The entry is in the subsystem table (src/core/subsystems.cpp)
// from the start, so these pickers fill their hooks here without touching a shared file. No hooks
// yet.

#include "internal/subsystem.h"

namespace facet::internal
{

Subsystem WidgetsDatePickersSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "widgets_date_pickers";
    return subsystem;
}

} // namespace facet::internal
