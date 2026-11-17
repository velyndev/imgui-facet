#pragma once

// Library-internal links between the core modules (theme, scale, motion, fonts, style).

#include <cstdint>

namespace facet
{
struct Theme;
}

namespace facet::internal
{

// theme.cpp: increments whenever T() changes (SetTheme, SetAccent, every transition step).
// PushAccent and PopAccent do not change it: they leave the current theme alone.
std::uint64_t ThemeRevision();

// theme.cpp: the current theme without the accent of PushAccent. ApplyImGuiStyle writes this
// one, so a stock style never takes a region's scoped accent.
const Theme& UnscopedTheme();

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

// log.cpp: reports a contract violation by the caller (a Pop without its Push, a scope left
// open at EndFrame): "facet: error: ..." to the debugger output and stderr, and, while a
// Dear ImGui context exists, through IM_ASSERT_USER_ERROR, so a host with Dear ImGui error
// recovery gets a log line instead of a crash. The caller then repairs Facet's own state.
void ReportUsageError(const char* format, ...);
// log.cpp: number of ReportUsageError calls since the process started, for tests.
int UsageErrorCount();

} // namespace facet::internal
