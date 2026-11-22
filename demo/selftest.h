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
//
// Determinism: every step is a fixed number of frames at the fixed time step of 1/60 s, and
// steps that last a while take seconds of frame time (HoldKey, Hover, Drag, WaitSeconds), so a
// script means the same at any speed of the machine. Positions are taken from the probes of
// the latest frame; offsets are in design units and follow the run's UI scale.
//
// The steps beyond clicks and key presses, with examples:
//
//     using T = facet::demo::Target;
//     test.MouseMove(T::Item("slider", 0.25f));                  // a quarter into the item
//     test.MouseDown();  test.MouseUp();                         // press and release apart
//     test.Drag({T::Item("slider", 0.2f), T::Item("slider", 0.8f), T::Item("slider", 0.3f)},
//               0.5f);                                          // through points, in 0.5 s
//     test.Drag({T::Item("slider", 0.5f), T::Display(1.2f, 0.5f)}, 0.25f);  // out of the window
//     test.Wheel(T::Item("list"), -2.0f);                        // two notches down
//     test.KeyPress(ImGuiKey_A | ImGuiMod_Ctrl);                 // modifiers: Ctrl, Shift, Alt
//     test.HoldKey(ImGuiKey_RightArrow, 1.0f);                   // key repeat runs for 1 s
//     test.Hover(T::Item("button"), 0.6f);                       // rest the mouse for 0.6 s
//     test.Type("hello");                                        // UTF-8 text input, without IME
//     test.Click("list", ImGuiMouseButton_Right);                // a right click
//     test.DoubleClick("field");                                 // two clicks within the double click time
//     test.KeyPress(ImGuiKey_Menu);                              // the Menu key (also Shift+F10)
//     test.KeyPress(ImGuiKey_LeftArrow | ImGuiMod_Shift);        // extends a text selection
//     test.SetClipboard("pasted");                               // the run's own clipboard
//     test.KeyPress(ImGuiKey_C | ImGuiMod_Ctrl);
//     test.ExpectClipboard("Ctrl+C copied the word", "hello");
//     test.DragAndHold({T::Item("row.2"), T::Item("row.0")}, 0.3f, 0.7f);  // rests 0.7 s on the target
//     test.ExpectNear("value is about 0.3", [] { return value; }, 0.3, 0.05);
//     test.ExpectText("the field holds hi", [] { return std::string(text); }, "hi");
//     test.ExpectVisible("nav.composites-editors");              // scrolled into view
//     test.ExpectNotVisible("nav.tokens");                       // scrolled out of view
//     test.SelectPage("composites-editors");                     // as --page would
//     test.ExpectPage("composites-editors");                     // the page shown
//
// The clipboard: every run gets a clipboard of its own, empty at the start, so Ctrl+C and
// Ctrl+V in a script never read or change the system clipboard.

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
    bool visible = false;  // the item's center lies inside the clip rect it was drawn with
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
    // Scrolls the item of this probe into the middle of its window when it is recorded
    // next, the way a person scrolls to a control before clicking it.
    void RequestReveal(std::string_view name) { pendingReveal = name; }
    // True when a probe of this name was recorded in any frame of the run so far, also when the
    // latest frame skipped it (a virtualized list submits only its visible rows).
    bool SeenInRun(std::string_view name) const;

private:
    std::vector<std::pair<std::string, ProbeRecord>> entries;
    std::vector<std::string> seen;  // every name recorded in the run; Clear keeps it
    std::string pendingReveal;
};

// A point for the mouse: a position inside a probed item, or on the display.
struct Target
{
    std::string probe;                   // empty: the display
    ImVec2 fraction = ImVec2(0.5f, 0.5f);  // of the item rect (or the display); outside 0 to 1
                                           // lies outside it, which is how a drag leaves a window
    ImVec2 offset = ImVec2(0.0f, 0.0f);    // design units, times the run's UI scale

    static Target Item(std::string_view probe, float fx = 0.5f, float fy = 0.5f);
    static Target Display(float fx, float fy);
    Target Offset(float dx, float dy) const;
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
        MouseMove,
        MouseButton,
        Drag,
        Wheel,
        KeyHold,
        KeyChange,
        Type,
        ExpectNear,
        ExpectText,
        ExpectVisible,
        Reveal,
        SelectPage,
        ExpectPage,
        DoubleClick,
        SetClipboard,
        ExpectClipboard,
        ExpectNotVisible,
    };

    struct Step
    {
        StepKind kind = StepKind::Wait;
        std::string text;            // probe name, description, typed text or page id
        ImGuiKeyChord chord = 0;     // key steps: key plus optional ImGuiMod_* flags
        int frames = 0;              // Wait
        float seconds = 0.0f;        // Drag, KeyHold, MouseMove (Hover)
        float hold = 0.0f;           // Drag: seconds the button stays down at the last point
        bool down = false;           // MouseButton, KeyChange
        ImGuiMouseButton button = ImGuiMouseButton_Left;  // Click, DoubleClick, MouseButton, Drag
        std::vector<Target> path;    // mouse steps: one point, or the drag path
        ImVec2 wheel = ImVec2(0.0f, 0.0f);
        double expected = 0.0;       // ExpectNear
        double tolerance = 0.0;
        std::string expectedText;    // ExpectText, ExpectClipboard
        std::function<void()> action;
        std::function<bool()> check;
        std::function<double()> number;
        std::function<std::string()> string;
    };

    // Runs arbitrary code between frames, typically to reset page state.
    void Do(std::function<void()> action);
    // Moves the mouse to the center of the probed item, presses and releases `button`. An item
    // outside the visible area of its window is scrolled into view first.
    void Click(std::string_view probe, ImGuiMouseButton button = ImGuiMouseButton_Left);
    // Like Click, then presses and releases `button` a second time at the same place one frame
    // later, well within io.MouseDoubleClickTime, so Dear ImGui counts a double click.
    void DoubleClick(std::string_view probe, ImGuiMouseButton button = ImGuiMouseButton_Left);
    // Presses and releases a key, with modifiers when given (ImGuiKey_Tab | ImGuiMod_Shift).
    void KeyPress(ImGuiKeyChord chord);
    // Renders this many extra frames.
    void Wait(int frames);
    // Renders frames for this many seconds of frame time (at least one frame).
    void WaitSeconds(float seconds);
    // Records PASS when check() returns true, FAIL otherwise.
    void Expect(std::string_view description, std::function<bool()> check);
    // Records PASS when the keyboard focus is on the probed item.
    void ExpectFocused(std::string_view probe);

    // Moves the mouse to the target in one frame. An item target outside the visible area of
    // its window is scrolled into view first, as for Click.
    void MouseMove(const Target& target);
    // Rests the mouse on the target for `seconds` of frame time (hover delays, tooltips).
    void Hover(const Target& target, float seconds);
    // Presses or releases a mouse button where the mouse is, one frame each.
    void MouseDown(ImGuiMouseButton button = ImGuiMouseButton_Left);
    void MouseUp(ImGuiMouseButton button = ImGuiMouseButton_Left);
    // Presses at the first point, moves along the path through every point at an even speed
    // over `seconds` (at least one frame per segment), and releases at the last point. Every
    // point is resolved when the drag starts; only the first one is scrolled into view.
    void Drag(std::vector<Target> path, float seconds = 0.25f, ImGuiMouseButton button = ImGuiMouseButton_Left);
    // Drag, but the button stays down with the mouse still at the last point for `holdSeconds`
    // of frame time before it is released: a drag that rests over a target (spring-loaded
    // folders, a list scrolling while the drag sits at its edge).
    void DragAndHold(std::vector<Target> path, float seconds, float holdSeconds,
                     ImGuiMouseButton button = ImGuiMouseButton_Left);
    // Moves to the target, then turns the wheel by (wheelX, wheelY) notches in one frame;
    // positive wheelY scrolls up.
    void Wheel(const Target& target, float wheelY, float wheelX = 0.0f);
    // Holds a key (with modifiers) for `seconds` of frame time, so key repeat runs, then
    // releases it.
    void HoldKey(ImGuiKeyChord chord, float seconds);
    // Presses or releases a key with its modifiers and leaves it there, one frame each.
    void KeyDown(ImGuiKeyChord chord);
    void KeyUp(ImGuiKeyChord chord);
    // Types UTF-8 text as character input, in one frame.
    void Type(std::string_view text);

    // Records PASS when value() is within `tolerance` of `expected`; the result line shows
    // the value found.
    void ExpectNear(std::string_view description, std::function<double()> value, double expected, double tolerance);
    // Records PASS when value() equals `expected`; the result line shows the text found.
    void ExpectText(std::string_view description, std::function<std::string()> value, std::string_view expected);
    // Records PASS when the center of the probed item lies in the visible part of its window.
    void ExpectVisible(std::string_view probe);
    // Records PASS when the probed item is outside the visible part of its window: its center
    // lies outside the clip rect, or it was skipped in the latest frame although it was recorded
    // earlier in the run (a virtualized list no longer submits it). A name never recorded in the
    // run fails, so a mistyped probe cannot pass.
    void ExpectNotVisible(std::string_view probe);
    // Replaces the text of the run's own clipboard (see the top of this file).
    void SetClipboard(std::string_view text);
    // Records PASS when the run's clipboard holds `expected`; the result line shows the text found.
    void ExpectClipboard(std::string_view description, std::string_view expected);
    // Scrolls the probed item into the middle of its window, as a person scrolls to look at
    // it, so a following ExpectVisible checks that it is drawn at all in a small viewport.
    // Records FAIL only when the probe is missing.
    void Reveal(std::string_view probe);
    // Shows the page with this id, as a click in the navigation or --page would.
    void SelectPage(std::string_view pageId);
    // Records PASS when the page with this id is the one shown.
    void ExpectPage(std::string_view pageId);

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
