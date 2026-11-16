#pragma once

// Library-internal links between the core modules (theme, scale, motion, fonts, style).

#include <cstdint>

namespace facet::internal
{

// theme.cpp: increments whenever T() changes (SetTheme, SetAccent, every transition step).
std::uint64_t ThemeRevision();

// scale.cpp: increments whenever Scale() changes.
std::uint64_t ScaleRevision();
// scale.cpp: writes the font scale factors of Scale() into the current Dear ImGui style.
void ApplyFontScale();

// imgui_style.cpp: runs ApplyImGuiStyle() when SyncImGuiStyle is on and the theme or the
// scale changed since the last application. Called from the scale module's NewFrame hook,
// which runs after the theme hook.
void SyncImGuiStyleIfNeeded();

// anim.cpp: seconds of motion time since Init, advanced by facet::DeltaTime() in every
// NewFrame. Shared clock of the looping indicators so they stay in phase.
double MotionClock();

// log.cpp: writes "facet: warning: ..." to the debugger output and to stderr.
void LogWarning(const char* format, ...);

} // namespace facet::internal
