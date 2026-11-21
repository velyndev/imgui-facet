// BeginOverlay, EndOverlay and the input region queries (containers.h): the controller of an
// overlay menu drawn over a game or another app.
//
// The windows of the menu are the ones Dear ImGui begins between BeginOverlay and EndOverlay (their
// begin order within the context is at least the count of windows begun before BeginOverlay).
// The fade is an animated opacity (motion.durationStandard, at once with reduced motion) that
// EndOverlay applies to the vertices of those windows' draw lists, so Facet's own drawing, which
// does not go through Dear ImGui's style alpha, fades too. While the menu fades out its windows
// take no input: EndOverlay marks them NoMouseInputs and NoNavInputs, which Dear ImGui reads when
// it looks for the hovered and focused window at the start of the next frame, and the keyboard
// focus leaves them. The rectangles of the windows that take input are kept per frame for the
// host's queries; facet::EndFrame makes them the last frame's.

#include "facet/widgets/containers.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/anim.h"
#include "facet/core/color.h"
#include "facet/core/context.h"
#include "facet/core/input.h"
#include "facet/core/theme.h"
#include "internal/item.h"
#include "internal/scope_stack.h"
#include "widgets/containers/container_layout.h"

namespace facet
{
namespace
{

constexpr int kOverlayFadeSub = internal::kItemAnimSubBase - 0x200;
constexpr int kOverlayDepth = 2;

struct OverlayRegionsFrame
{
    ScreenRegion regions[internal::kOverlayRegionCount];
    ImGuiID windows[internal::kOverlayRegionCount] = {};
    int count = 0;     // regions recorded; may exceed the array when more windows took input
    bool overlay = false;      // a BeginOverlay ran in the frame
    bool menuInput = false;    // its menu was visible and not fading out
};

struct OverlayFrames
{
    OverlayRegionsFrame current;
    OverlayRegionsFrame previous;
    int shownFrame = -1;       // the frame of the last BeginOverlay
    bool shown = false;        // its menu showed (visible or fading in) in that frame
};

OverlayFrames& OverlayRecords()
{
    static OverlayFrames frames;
    return frames;
}

struct OverlayEntry
{
    ImGuiID id = 0;
    int beginOrder = 0;
    float alpha = 1.0f;
    bool fadingOut = false;
};

struct OverlayStack
{
    OverlayEntry entries[kOverlayDepth];
    int depth = 0;
    int overflow = 0;
};

OverlayStack& Overlays()
{
    static OverlayStack stack;
    return stack;
}

bool OverlayInFrame()
{
    const ImGuiContext* g = ImGui::GetCurrentContext();
    return IsInitialized() && g != nullptr && g->WithinFrameScope;
}

// Multiplies the opacity of every vertex of `drawList` by `alpha`.
void FadeDrawList(ImDrawList* drawList, float alpha)
{
    for (ImDrawVert& v : drawList->VtxBuffer)
    {
        const float a = static_cast<float>((v.col >> IM_COL32_A_SHIFT) & 0xFF) * alpha;
        const ImU32 faded = static_cast<ImU32>(std::lround(std::clamp(a, 0.0f, 255.0f)));
        v.col = (v.col & ~IM_COL32_A_MASK) | (faded << IM_COL32_A_SHIFT);
    }
}

bool InRegions(const OverlayRegionsFrame& frame, ImVec2 point)
{
    const int count = std::min(frame.count, internal::kOverlayRegionCount);
    for (int i = 0; i < count; ++i)
    {
        const ScreenRegion& r = frame.regions[i];
        if (point.x >= r.min.x && point.x < r.max.x && point.y >= r.min.y && point.y < r.max.y)
            return true;
    }
    return false;
}

} // namespace

bool BeginOverlay(const char* id, bool* visible, const OverlayOptions& options)
{
    if (!OverlayInFrame() || visible == nullptr)
        return false;
    if (id == nullptr)
        id = "";
    ImGuiContext& g = *GImGui;
    const Theme& t = T();
    const ImGuiID stateId = ImHashStr(id);
    if (options.toggleShortcut.IsValid() && ShortcutPressed(options.toggleShortcut))
        *visible = !*visible;
    const float alpha = Tween(stateId, kOverlayFadeSub, *visible ? 1.0f : 0.0f, t.motion.durationStandard, Ease::OutCubic);

    OverlayFrames& frames = OverlayRecords();
    frames.current.overlay = true;
    frames.current.menuInput = *visible;
    frames.shownFrame = g.FrameCount;
    frames.shown = *visible || alpha > 0.0f;
    if (!*visible && alpha <= 0.0f)
        return false;

    // The keyboard focus leaves a menu that is fading out.
    if (!*visible && g.NavWindow != nullptr && frames.previous.menuInput)
    {
        const ImGuiID root = g.NavWindow->RootWindow != nullptr ? g.NavWindow->RootWindow->ID : g.NavWindow->ID;
        for (int i = 0; i < std::min(frames.previous.count, internal::kOverlayRegionCount); ++i)
            if (frames.previous.windows[i] == root)
            {
                ImGui::FocusWindow(nullptr);
                break;
            }
    }
    if (options.scrim)
    {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const ImVec4 scrim = WithAlpha(t.colors.scrim, t.colors.scrim.w * alpha);
        ImGui::GetBackgroundDrawList()->AddRectFilled(
            viewport->Pos, ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y), ToU32(scrim));
    }

    OverlayStack& stack = Overlays();
    internal::PushScope("BeginOverlay");
    if (stack.depth >= kOverlayDepth)
    {
        ++stack.overflow;
        return true;
    }
    OverlayEntry& entry = stack.entries[stack.depth++];
    entry.id = stateId;
    entry.beginOrder = g.WindowsActiveCount;
    entry.alpha = alpha;
    entry.fadingOut = !*visible;
    return true;
}

void EndOverlay()
{
    if (!internal::PopScope("BeginOverlay"))
        return;
    OverlayStack& stack = Overlays();
    if (stack.overflow > 0)
    {
        --stack.overflow;
        return;
    }
    if (stack.depth == 0)
        return;
    const OverlayEntry entry = stack.entries[--stack.depth];
    ImGuiContext& g = *GImGui;
    for (ImGuiWindow* window : g.Windows)
    {
        if (window->LastFrameActive != g.FrameCount || window->BeginOrderWithinContext < entry.beginOrder || window->Hidden)
            continue;
        if (entry.alpha < 1.0f)
            FadeDrawList(window->DrawList, entry.alpha);
        const bool root = window->RootWindow == window;
        if (entry.fadingOut)
        {
            window->Flags |= ImGuiWindowFlags_NoMouseInputs | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus;
            continue;
        }
        if (!root || (window->Flags & (ImGuiWindowFlags_Tooltip | ImGuiWindowFlags_NoMouseInputs)) != 0)
            continue;
        internal::AddOverlayInputRegion(window);
    }
}

bool OverlayWantsMouse()
{
    const ImGuiContext* g = ImGui::GetCurrentContext();
    if (g == nullptr)
        return false;
    const OverlayRegionsFrame& last = OverlayRecords().previous;
    if (!g->IO.WantCaptureMouse)
        return false;
    // A drag that started on a window taking input keeps the mouse until it ends.
    if (g->ActiveId != 0 && g->ActiveIdWindow != nullptr)
    {
        const ImGuiID root = g->ActiveIdWindow->RootWindow != nullptr ? g->ActiveIdWindow->RootWindow->ID : 0;
        for (int i = 0; i < std::min(last.count, internal::kOverlayRegionCount); ++i)
            if (last.windows[i] == root)
                return true;
    }
    return InRegions(last, g->IO.MousePos);
}

bool OverlayWantsKeyboard()
{
    const ImGuiContext* g = ImGui::GetCurrentContext();
    if (g == nullptr || !g->IO.WantCaptureKeyboard || g->NavWindow == nullptr)
        return false;
    const OverlayRegionsFrame& last = OverlayRecords().previous;
    const ImGuiID root = g->NavWindow->RootWindow != nullptr ? g->NavWindow->RootWindow->ID : g->NavWindow->ID;
    for (int i = 0; i < std::min(last.count, internal::kOverlayRegionCount); ++i)
        if (last.windows[i] == root)
            return true;
    return false;
}

bool OverlayHitTest(ImVec2 point)
{
    return InRegions(OverlayRecords().previous, point);
}

int GetOverlayInputRegions(std::span<ScreenRegion> regions)
{
    const OverlayRegionsFrame& last = OverlayRecords().previous;
    const int stored = std::min(last.count, internal::kOverlayRegionCount);
    const int copied = std::min(stored, static_cast<int>(regions.size()));
    for (int i = 0; i < copied; ++i)
        regions[static_cast<std::size_t>(i)] = last.regions[i];
    return last.count;
}

namespace internal
{

void AddOverlayInputRegion(ImGuiWindow* window)
{
    OverlayRegionsFrame& frame = OverlayRecords().current;
    const ImGuiID id = window->ID;
    const int stored = std::min(frame.count, kOverlayRegionCount);
    for (int i = 0; i < stored; ++i)
        if (frame.windows[i] == id)
            return;
    if (frame.count < kOverlayRegionCount)
    {
        frame.regions[frame.count].min = window->Pos;
        frame.regions[frame.count].max = ImVec2(window->Pos.x + window->Size.x, window->Pos.y + window->Size.y);
        frame.windows[frame.count] = id;
    }
    ++frame.count;
}

bool OverlayMenuShown()
{
    const OverlayFrames& frames = OverlayRecords();
    const ImGuiContext* g = ImGui::GetCurrentContext();
    if (g == nullptr)
        return false;
    // This frame's state once BeginOverlay ran, else the last frame's.
    if (frames.shownFrame == g->FrameCount || frames.shownFrame == g->FrameCount - 1)
        return frames.shown;
    return false;
}

void ResetOverlayRecords()
{
    OverlayRecords() = OverlayFrames{};
    Overlays() = OverlayStack{};
}

void RotateOverlayRegions()
{
    OverlayFrames& frames = OverlayRecords();
    frames.previous = frames.current;
    frames.current = OverlayRegionsFrame{};
    Overlays().depth = 0;
    Overlays().overflow = 0;
}

} // namespace internal

} // namespace facet
