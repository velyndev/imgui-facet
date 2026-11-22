// Per-frame and per-device work of the search, preset and app control composites
// (SearchFilterBar, SettingsSearch, KeybindList, PresetList, ThemeModeControl, AccentPicker,
// UiScaleControl, Onboarding, ConsoleView), for example closing the frame's settings search hit
// counts so SettingsSearchHitCount reads the last completed frame. The entry is in the subsystem
// table (src/core/subsystems.cpp) from the start, so the package fills its hooks here without
// touching a shared file. No hooks yet.

#include "internal/subsystem.h"

namespace facet::internal
{

Subsystem WidgetsSettingsSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "widgets_settings";
    return subsystem;
}

} // namespace facet::internal
