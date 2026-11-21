// Per-frame and per-device work of the color pickers (ColorWell, ColorPicker, ColorInputs,
// SwatchPalette, SatValSquare, ColorChannelSlider, ColorModeEditor and SampleAnimatedColor in
// pickers.h): the two-axis edit session of color_plane.h is forgotten at facet::Init and Shutdown,
// so nothing carries over to another Dear ImGui context.

#include "internal/subsystem.h"
#include "widgets/pickers/color_plane.h"

namespace facet::internal
{
namespace
{

bool ColorPickersInit()
{
    ResetColorPlaneState();
    return true;
}

void ColorPickersShutdown()
{
    ResetColorPlaneState();
}

} // namespace

Subsystem WidgetsColorPickersSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "widgets_color_pickers";
    subsystem.init = ColorPickersInit;
    subsystem.shutdown = ColorPickersShutdown;
    return subsystem;
}

} // namespace facet::internal
