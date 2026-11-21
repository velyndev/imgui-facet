// Per-frame and per-device work of the color pickers (ColorWell, ColorPicker, ColorInputs,
// SwatchPalette, SatValSquare, ColorChannelSlider, ColorModeEditor and SampleAnimatedColor in
// pickers.h). The entry is in the subsystem table (src/core/subsystems.cpp) from the start, so the
// color pickers fill their hooks here without touching a shared file. No hooks yet.

#include "internal/subsystem.h"

namespace facet::internal
{

Subsystem WidgetsColorPickersSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "widgets_color_pickers";
    return subsystem;
}

} // namespace facet::internal
