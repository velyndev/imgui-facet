#pragma once

// The Dear ImGui configuration of the consumer example. The consumer's Dear ImGui target
// names this file in IMGUI_USER_CONFIG, and Facet links that target (FACET_IMGUI_TARGET), so
// Facet's own sources are compiled with these settings too.

// Lets main.cpp check that this file is in effect.
#define CONSUMER_IMCONFIG 1

// 32-bit draw indices. This changes a Dear ImGui type, so a Facet library compiled against
// another configuration would write the wrong index size into the draw lists.
#define ImDrawIdx unsigned int

// The example never shows the Dear ImGui demo window.
#define IMGUI_DISABLE_DEMO_WINDOWS
