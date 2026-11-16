#pragma once

// Scripted input tests for demo pages. A page marks the items it wants to operate with
// ctx.Probe("name") right after submitting them, then registers a script:
//
//     FACET_DEMO_SELFTEST("foundation")
//     {
//         test.Do([] { state = {}; });
//         test.Click("checkbox");
//         test.Expect("checkbox turns on", [] { return state.checked; });
//         test.KeyPress(ImGuiKey_Tab);
//         test.ExpectFocused("increment");
//     }
//
// facet_demo --selftest renders the page offscreen with a fixed time step, executes the
// steps in order with synthetic mouse and keyboard events, and prints PASS or FAIL for
// every expectation.

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>

#include "run_options.h"

#define FACET_DEMO_CONCAT_INNER(a, b) a##b
#define FACET_DEMO_CONCAT(a, b) FACET_DEMO_CONCAT_INNER(a, b)

namespace facet::demo
{

class AppShell;

struct ProbeRecord
{
    ImGuiID id = 0;
    ImVec2 min;
    ImVec2 max;
};

// Item rects recorded by PageCtx::Probe during the most recent frame.
class ProbeTable
{
public:
    void Clear();
    void RecordLastItem(std::string_view name);
    const ProbeRecord* Find(std::string_view name) const;
    // Name of the probe recorded for this item id, or nullptr.
    const char* NameOf(ImGuiID id) const;

private:
    std::vector<std::pair<std::string, ProbeRecord>> entries;
};

class SelftestScript
{
public:
    enum class StepKind
    {
        Do,
        Click,
        KeyPress,
        Wait,
        Expect,
        ExpectFocused,
    };

    struct Step
    {
        StepKind kind = StepKind::Wait;
        std::string text;            // probe name or expectation description
        ImGuiKeyChord chord = 0;     // KeyPress: key plus optional ImGuiMod_* flags
        int frames = 0;              // Wait
        std::function<void()> action;
        std::function<bool()> check;
    };

    // Runs arbitrary code between frames, typically to reset page state.
    void Do(std::function<void()> action);
    // Moves the mouse to the center of the probed item, presses and releases the left button.
    void Click(std::string_view probe);
    // Presses and releases a key, with modifiers when given (ImGuiKey_Tab | ImGuiMod_Shift).
    void KeyPress(ImGuiKeyChord chord);
    // Renders this many extra frames.
    void Wait(int frames);
    // Records PASS when check() returns true, FAIL otherwise.
    void Expect(std::string_view description, std::function<bool()> check);
    // Records PASS when the keyboard focus is on the probed item.
    void ExpectFocused(std::string_view probe);

    const std::vector<Step>& Steps() const { return steps; }

private:
    std::vector<Step> steps;
};

using SelftestFn = void (*)(SelftestScript& test);

struct SelftestRegistrar
{
    SelftestRegistrar(const char* pageId, SelftestFn build);
};

// Script builder registered for this page id, or nullptr.
SelftestFn FindSelftest(std::string_view pageId);

// Page ids that have a selftest script, sorted.
std::vector<std::string> SelftestPageIds();
// Page ids that registered a second script; the later registration was ignored.
const std::vector<std::string>& DuplicateSelftestIds();

struct SelftestOptions
{
    std::string page;  // empty: every page that registered a script
    int width = 1280;  // offscreen size in pixels
    int height = 800;
    float scale = 1.0f;
    ThemeSelection themes = ThemeSelection::Dark;  // Both runs every script once per theme
};

// Runs the scripts offscreen on the shell's device. Every result line names the page and
// the theme. Returns the process exit code: 0 when every expectation passed, 1 otherwise.
int RunSelftest(AppShell& shell, const SelftestOptions& options);

} // namespace facet::demo

#define FACET_DEMO_SELFTEST(pageId)                                                                \
    static void FACET_DEMO_CONCAT(FacetDemoSelftest_, __LINE__)(::facet::demo::SelftestScript &   \
                                                                test);                             \
    static const ::facet::demo::SelftestRegistrar FACET_DEMO_CONCAT(facetDemoSelftestRegistrar_,  \
                                                                    __LINE__){                     \
        pageId, &FACET_DEMO_CONCAT(FacetDemoSelftest_, __LINE__)};                                 \
    static void FACET_DEMO_CONCAT(FacetDemoSelftest_, __LINE__)(::facet::demo::SelftestScript &   \
                                                                test)
