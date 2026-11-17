#pragma once

// Open Facet scopes of the current frame, so facet::EndFrame can name a Begin whose End was
// never called (docs/api-conventions.md, rule 14).
//
// Every widget Begin that the caller must close calls PushScope with its own function name
// when it opens (for a bool Begin: only when it returns true), and the matching End calls
// PopScope with the Begin's name before doing its own work. The stack holds names only; it
// does not repair Dear ImGui windows, which each Begin and End manage themselves.
//
// No allocation: a fixed stack of kScopeStackDepth entries; deeper scopes are reported once
// and counted, so their pops still balance.

namespace facet::internal
{

inline constexpr int kScopeStackDepth = 64;

// Opens a scope. `name` is the Begin function's name as a string literal ("BeginCard"); it is
// stored, not copied.
void PushScope(const char* name);

// Closes the innermost scope named `name`. Returns true when it was open. When scopes opened
// after it are still open, each of them is reported as left open and closed with it. When no
// scope of that name is open, the call is reported, nothing changes and the result is false;
// the End then skips its work, because its Begin did not run.
bool PopScope(const char* name);

// Number of open scopes, overflow included.
int OpenScopeCount();

// The innermost open scope's name, or nullptr.
const char* InnermostScope();

// facet::EndFrame: reports every scope still open, innermost first, and clears the stack.
void CloseOpenScopes();

} // namespace facet::internal
