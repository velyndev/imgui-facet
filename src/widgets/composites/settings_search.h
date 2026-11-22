#pragma once

// The frame hooks of the settings search (settings_search.cpp), called from the settings frame
// subsystem (settings_frame.cpp):
//
//     subsystem.endFrame = &SettingsSearchEndFrame;   // the frame's hit counts become the last ones
//     subsystem.shutdown = &SettingsSearchShutdown;   // forgets every count and reveal record
//
// SettingsSearchHitCount reads the counts of the last completed frame, so a navigation drawn
// before the settings page sees the hits of the page drawn one frame earlier.

namespace facet::internal
{

void SettingsSearchEndFrame();
void SettingsSearchShutdown();

} // namespace facet::internal
