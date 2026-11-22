// Per-frame and per-device work of the search, preset and app control composites
// (SearchFilterBar, SettingsSearch, KeybindList, PresetList, ThemeModeControl, AccentPicker,
// UiScaleControl, Onboarding, ConsoleView). The entry is in the subsystem table
// (src/core/subsystems.cpp). Its hooks close the frame's settings search hit counts, so
// SettingsSearchHitCount reads the last completed frame, and forget the counts at shutdown.

#include "internal/subsystem.h"
#include "widgets/composites/settings_search.h"

namespace facet::internal
{

Subsystem WidgetsSettingsSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "widgets_settings";
    subsystem.endFrame = &SettingsSearchEndFrame;
    subsystem.shutdown = &SettingsSearchShutdown;
    return subsystem;
}

} // namespace facet::internal
