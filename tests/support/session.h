#pragma once

// One Dear ImGui and Facet session on a WARP device, for tests that need a context, a device,
// facet::Init, real frames or scripted input. Every test that drives frames uses this helper
// instead of building its own setup, so all of them run under the same conditions.
//
// What a Session sets up, in this order:
//   - a WARP device and its immediate context (no GPU needed);
//   - a fresh Dear ImGui context: display size from the options, a fixed delta time, no ini
//     file, keyboard navigation on (NavEnableKeyboard), error recovery without asserts or
//     tooltips (to the debug log only), so a test can provoke a usage error and count the report
//     (internal::UsageErrorCount in src/core/core_internal.h), and the renderer flag
//     ImGuiBackendFlags_RendererHasTextures; the session plays the renderer and marks every
//     texture request as done after each frame, so fonts and atlases work without a backend;
//   - the main viewport size and the circle tessellation tables, which a frame would set up,
//     so draw lists can also be recorded outside a frame;
//   - a clipboard of its own (Clipboard, SetClipboard), empty at the start, so copy and paste
//     in a test never read or change the system clipboard;
//   - facet::SetFollowSystemMotion(false), so the Windows animation setting does not decide
//     whether motion runs: a server or a remote session has animations off and would run every
//     test with reduced motion. A test of reduced motion sets theme.motion.reduceMotion itself;
//   - facet::Init with the device (unless options.initFacet is false; then call InitFacet).
// The destructor runs facet::Shutdown and destroys the context. One Session at a time.
//
// Frames: SetUi stores the code of one frame. Frame() runs ImGui::NewFrame, facet::NewFrame,
// the ui code (inside a full-display window named "test" unless options.window is false),
// facet::EndFrame and ImGui::Render. The time step is fixed, so motion and key repeat do not
// depend on how fast the test runs.
//
// Input goes through the Dear ImGui input queue (io.Add*Event), exactly like a platform
// backend; Facet reads input only from ImGuiIO, so every widget can be driven this way. The
// helpers that take time step the frames themselves:
//
//     facet::test::Session session;
//     REQUIRE(session.Ready());
//     float value = 0.5f;
//     session.SetUi([&] { ImGui::SliderFloat("value", &value, 0.0f, 1.0f); });
//     session.Frame();
//     const ImVec2 at = session.ItemCenter("value");     // rect recorded by Track below
//     session.Click(at);
//     session.KeyPress(ImGuiKey_Tab | ImGuiMod_Shift);
//
// Item positions: call session.Track("name") right after submitting an item inside the ui
// code; ItemRect / ItemCenter return its rect from the latest frame.
//
// Each test case builds its own Session, so no state leaks from one case to the next. Facet
// state that lives for the whole process (the theme, the scale, registered icons) is not reset
// by a Session; a test that changes it restores it.

#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <d3d11.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <wrl/client.h>

namespace facet::test
{

struct SessionOptions
{
    ImVec2 displaySize = ImVec2(800.0f, 600.0f);  // pixels
    float deltaTime = 1.0f / 60.0f;               // seconds per frame
    bool initFacet = true;                        // false: call InitFacet() yourself
    bool window = true;                           // wrap the ui code in a full-display window
};

class Session
{
public:
    explicit Session(const SessionOptions& options = {});
    ~Session();
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    // The device, the context and (when asked for) facet::Init all succeeded.
    bool Ready() const { return ready; }
    // Runs facet::Init with the session's device; for sessions built with initFacet false.
    bool InitFacet();

    ID3D11Device* Device() const { return device.Get(); }
    ID3D11DeviceContext* DeviceContext() const { return context.Get(); }
    const SessionOptions& Options() const { return options; }

    // --- Frames ---------------------------------------------------------------------------
    void SetUi(std::function<void()> ui);
    void Frame();
    void Frames(int count);
    // Frames for this many seconds of frame time (at least one).
    void Seconds(float seconds);
    int FrameCount() const { return frames; }

    // Records the last submitted item under `name` (call inside the ui code).
    void Track(std::string_view name);
    // The rect of a tracked item in the latest frame; a zero rect when it was not tracked.
    ImRect ItemRect(std::string_view name) const;
    ImVec2 ItemCenter(std::string_view name) const;
    // The id of a tracked item in the latest frame, 0 when it was not tracked.
    ImGuiID ItemId(std::string_view name) const;
    // Whether the center of a tracked item lay in the visible part of its window (the clip rect
    // it was submitted with) in the latest frame; false when it was not tracked.
    bool ItemVisible(std::string_view name) const;

    // --- Clipboard ------------------------------------------------------------------------
    // The session's own clipboard, which Dear ImGui reads and writes (Ctrl+C, Ctrl+V).
    const std::string& Clipboard() const { return clipboard; }
    void SetClipboard(std::string_view text) { clipboard = text; }

    // --- Mouse ----------------------------------------------------------------------------
    // Each call queues events and renders frames, so the next query sees the result.
    void MouseMove(ImVec2 pos);                        // one frame
    void MouseDown(ImGuiMouseButton button = ImGuiMouseButton_Left);  // one frame
    void MouseUp(ImGuiMouseButton button = ImGuiMouseButton_Left);    // one frame
    // Move, press, release: three frames, then one more so the result is drawn. A right click is
    // Click(pos, ImGuiMouseButton_Right).
    void Click(ImVec2 pos, ImGuiMouseButton button = ImGuiMouseButton_Left);
    // Click, then a second press and release at the same place right after it, well within
    // io.MouseDoubleClickTime, so Dear ImGui counts a double click.
    void DoubleClick(ImVec2 pos, ImGuiMouseButton button = ImGuiMouseButton_Left);
    // Presses at path[0], moves through every point of the path, releases at the last one.
    // `framesPerSegment` frames per straight segment, positions interpolated linearly.
    void Drag(const std::vector<ImVec2>& path, int framesPerSegment = 4,
              ImGuiMouseButton button = ImGuiMouseButton_Left);
    // Drag, but the button stays down with the mouse still at the last point for `holdFrames`
    // frames before the release: a drag resting on a target.
    void DragAndHold(const std::vector<ImVec2>& path, int framesPerSegment, int holdFrames,
                     ImGuiMouseButton button = ImGuiMouseButton_Left);
    // Moves to pos, then turns the wheel (positive: up / left) for one frame.
    void Wheel(ImVec2 pos, float wheelY, float wheelX = 0.0f);

    // --- Keyboard -------------------------------------------------------------------------
    // Key plus ImGuiMod_* modifiers: modifiers go down first and up last.
    void KeyDown(ImGuiKeyChord chord);  // one frame
    void KeyUp(ImGuiKeyChord chord);    // one frame
    // Down for one frame, up, then two more frames.
    void KeyPress(ImGuiKeyChord chord);
    // Holds the chord for `seconds` of frame time (key repeat runs), then releases it.
    void HoldKey(ImGuiKeyChord chord, float seconds);
    // UTF-8 text as character input (no IME composition), one frame.
    void Type(const char* text);

private:
    struct TrackedItem
    {
        std::string name;
        ImGuiID id = 0;
        ImRect rect;
        bool visible = false;
    };

    void ApplyTextureRequests();
    const TrackedItem* FindTracked(std::string_view name) const;
    static const char* GetClipboardText(ImGuiContext* context);
    static void SetClipboardText(ImGuiContext* context, const char* text);

    SessionOptions options;
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    ImGuiContext* imgui = nullptr;
    std::function<void()> ui;
    std::vector<TrackedItem> tracked;
    std::string clipboard;
    ImTextureID nextTextureId = 1;
    int frames = 0;
    bool facetStarted = false;
    bool ready = false;
};

} // namespace facet::test
