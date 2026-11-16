#pragma once

// Demo page registry. Each page lives in its own demo/pages/page_<name>.cpp and registers
// itself with FACET_DEMO_PAGE, so adding a page never touches a shared file:
//
//     FACET_DEMO_PAGE("buttons", "Buttons", "Controls", 10, "default", "pressed")
//     {
//         if (ctx.capture && ctx.captureState == 1) { ... force the pressed look ... }
//         ...
//     }
//
// Arguments: unique id (used on the command line and in capture file names), title shown in
// the navigation, group heading, order inside the group, then the optional capture state
// names. Without state names the page has the single state "default". Capture mode renders
// every page once per state with ctx.captureState set to the index of that state.
// When a state changes something below the first screen, call ctx.RevealInCapture() right
// after that content so the change is scrolled into the captured image.

#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "selftest.h"

namespace facet::demo
{

// Per-frame information passed to a page.
struct PageCtx
{
    bool capture = false;     // true in --capture runs: the page should show captureState
    int captureState = 0;     // index into the page's state names; 0 outside capture runs
    float scale = 1.0f;       // UI scale: monitor DPI / 96 in the window, --scale offscreen
    ProbeTable* probes = nullptr;  // set during --selftest runs

    // Records the rect and id of the last submitted item under this name so that
    // selftest steps can click it or check its focus. Does nothing outside selftest runs.
    void Probe(std::string_view name) const;

    // In capture runs, scrolls the page so the current cursor line sits at this fraction
    // of the visible height (0 top, 0.5 center, 1 bottom). Call it right after the content
    // a capture state changes, so the change is visible even when the page is taller than
    // the capture size. Does nothing outside capture runs.
    void RevealInCapture(float ratio = 0.5f) const;
};

using PageFn = void (*)(PageCtx& ctx);

struct PageInfo
{
    std::string id;
    std::string title;
    std::string group;
    int order = 0;
    std::vector<std::string> states;  // never empty; states[0] is the default state
    PageFn draw = nullptr;
};

// All registered pages. Groups are ordered by the lowest page order they contain (ties by
// group name); pages inside a group by order, then title.
std::span<const PageInfo> Pages();

// Index into Pages() of the page with this id, or -1.
int FindPage(std::string_view id);

// Checks the page and selftest registrations: every page id must be unique, every page may
// register at most one selftest script, and every script must belong to a registered page.
// Prints one error line per problem to stderr and returns false when any was found.
// Duplicate ids would otherwise overwrite each other's capture files silently.
bool ValidateRegistry();

struct PageRegistrar
{
    PageRegistrar(const char* id, const char* title, const char* group, int order, PageFn draw,
                  std::initializer_list<const char*> states);
};

} // namespace facet::demo

#define FACET_DEMO_PAGE(id, title, group, order, ...)                                              \
    static void FACET_DEMO_CONCAT(FacetDemoPage_, __LINE__)(                                       \
        [[maybe_unused]] ::facet::demo::PageCtx & ctx);                                            \
    static const ::facet::demo::PageRegistrar FACET_DEMO_CONCAT(facetDemoPageRegistrar_,          \
                                                                __LINE__){                         \
        id, title, group, order, &FACET_DEMO_CONCAT(FacetDemoPage_, __LINE__), {__VA_ARGS__}};     \
    static void FACET_DEMO_CONCAT(FacetDemoPage_, __LINE__)(                                       \
        [[maybe_unused]] ::facet::demo::PageCtx & ctx)
