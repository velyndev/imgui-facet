// BeginAppFrame, EndAppFrame, the hit test, the size limits and the placement storage
// (containers.h): the root surface of a borderless desktop app.
//
// The frame is a Dear ImGui window without decorations that covers the main viewport, drawn in
// the theme's bg, kept behind every other window (it never comes to the front on focus) and without
// padding, so a title bar submitted first sits at the very top. BeginAppFrame records the frame's
// rectangle and its resize band; EndFrame turns this frame's record and the title bar regions
// (container_layout.h) into the last frame's, which AppFrameHitTest reads, so a host can answer
// its window messages between frames.

#include "facet/widgets/containers.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/context.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "internal/scope_stack.h"
#include "internal/state_storage.h"
#include "widgets/containers/container_layout.h"

namespace facet
{
namespace
{

struct AppFrameRecord
{
    bool valid = false;
    ImGuiID window = 0;
    ImRect rect;            // pixels
    float band = 0.0f;      // resize band; 0 while the host is maximized
    ImVec2 minUnits = {0.0f, 0.0f};
    ImVec2 maxUnits = {0.0f, 0.0f};
};

struct AppFrameRecords
{
    AppFrameRecord current;
    AppFrameRecord previous;
    ImGuiWindow* open = nullptr;  // the frame window between Begin and End
};

AppFrameRecords& Records()
{
    static AppFrameRecords records;
    return records;
}

// Whether the topmost window at `point` in the last frame belongs to another root window than
// the app frame: a popup, a tooltip or a floating window there takes the click, so the host's
// drag region or window buttons must not swallow it. Dear ImGui keeps its windows in display
// order, back to front. Between frames a window's Active flag still holds the last frame; inside
// a frame that is WasActive.
bool AppFrameCoveredAt(ImGuiID frameWindow, ImVec2 point)
{
    const ImGuiContext* g = GImGui;
    if (g == nullptr)
        return false;
    for (int i = g->Windows.Size - 1; i >= 0; --i)
    {
        const ImGuiWindow* window = g->Windows[i];
        const bool shown = g->WithinFrameScope ? window->WasActive : window->Active;
        if (!shown || window->Hidden || window->IsFallbackWindow || (window->Flags & ImGuiWindowFlags_NoMouseInputs) != 0)
            continue;
        if (!window->OuterRectClipped.Contains(point))
            continue;
        const ImGuiWindow* root = window->RootWindow != nullptr ? window->RootWindow : window;
        return root->ID != frameWindow;
    }
    return false;
}

// The placement of the host window, saved in the ini file.
struct FramePlacementState
{
    static constexpr const char* kStateName = "AppFrame";
    FramePlacement placement;
    bool known = false;

    static bool ReadPair(const char* value, ImVec2* out)
    {
        char* end = nullptr;
        const float a = std::strtof(value, &end);
        if (end == value || *end != ',')
            return false;
        const char* second = end + 1;
        const float b = std::strtof(second, &end);
        if (end == second || !std::isfinite(a) || !std::isfinite(b))
            return false;
        *out = ImVec2(a, b);
        return true;
    }
    void WriteIni(ImGuiTextBuffer& out) const
    {
        if (!known)
            return;
        out.appendf("pos=%.1f,%.1f\nsize=%.2f,%.2f\nscale=%.4f\nmaximized=%d\n", placement.position.x,
                    placement.position.y, placement.restoredSize.x, placement.restoredSize.y, placement.scale,
                    placement.maximized ? 1 : 0);
    }
    void ReadIni(const char* key, const char* value)
    {
        ImVec2 pair;
        if (std::strcmp(key, "pos") == 0 && ReadPair(value, &pair))
        {
            placement.position = pair;
            known = true;
        }
        else if (std::strcmp(key, "size") == 0 && ReadPair(value, &pair))
        {
            placement.restoredSize = pair;
        }
        else if (std::strcmp(key, "scale") == 0)
        {
            const float scale = std::strtof(value, nullptr);
            placement.scale = std::isfinite(scale) && scale > 0.0f ? scale : 0.0f;
        }
        else if (std::strcmp(key, "maximized") == 0)
        {
            placement.maximized = std::atoi(value) != 0;
        }
    }
};

bool AppFrameInFrame()
{
    const ImGuiContext* g = ImGui::GetCurrentContext();
    return IsInitialized() && g != nullptr && g->WithinFrameScope;
}

bool HasPoint(const ImRect& r, ImVec2 p)
{
    return r.GetWidth() > 0.0f && r.GetHeight() > 0.0f && r.Contains(p);
}

} // namespace

bool BeginAppFrame(const char* id, const AppFrameOptions& options)
{
    if (!AppFrameInFrame())
        return false;
    if (id == nullptr)
        id = "";
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    AppFrameRecords& records = Records();
    if (viewport->Size.x <= 0.0f || viewport->Size.y <= 0.0f)
        return false;  // the host window is minimized
    const Theme& t = T();
    ImGui::SetNextWindowPos(viewport->Pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(viewport->Size, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
                                        ImGuiWindowFlags_NoBackground;
    const bool began = ImGui::Begin(id, nullptr, kFlags);
    ImGui::PopStyleVar(3);
    ImGuiWindow* window = GImGui->CurrentWindow;
    if (!began || window == nullptr || window->SkipItems)
    {
        ImGui::End();
        return false;
    }
    FillRect(window->DrawList, window->Pos, ImVec2(window->Pos.x + window->Size.x, window->Pos.y + window->Size.y),
             ToU32(t.colors.bg), 0.0f);

    AppFrameRecord& record = records.current;
    record.valid = true;
    record.window = window->ID;
    record.rect = window->Rect();
    record.band = options.hostMaximized ? 0.0f
                                        : Px(options.resizeBorder > 0.0f ? options.resizeBorder : t.containers.resizeBorder);
    record.minUnits = options.minSize;
    record.maxUnits = options.maxSize;
    records.open = window;
    internal::PushScope("BeginAppFrame");
    return true;
}

void EndAppFrame()
{
    if (!internal::PopScope("BeginAppFrame"))
        return;
    AppFrameRecords& records = Records();
    ImGuiWindow* window = records.open;
    records.open = nullptr;
    if (window == nullptr || GImGui->CurrentWindow != window)
        return;  // the caller left a window open inside the frame; Dear ImGui reports that itself
    ImGui::End();
}

FrameHit AppFrameHitTest(ImVec2 point)
{
    const AppFrameRecord& frame = Records().previous;
    if (!frame.valid || !frame.rect.Contains(point))
        return FrameHit::Client;
    // Another window over the point takes it, also over the resize band.
    if (AppFrameCoveredAt(frame.window, point))
        return FrameHit::Client;
    // The resize band: corners reach twice the band along each edge, so they are easy to hit.
    const ImRect& r = frame.rect;
    const float b = frame.band;
    if (b > 0.0f)
    {
        const bool left = point.x < r.Min.x + b;
        const bool right = point.x >= r.Max.x - b;
        const bool top = point.y < r.Min.y + b;
        const bool bottom = point.y >= r.Max.y - b;
        const bool nearLeft = point.x < r.Min.x + b * 2.0f;
        const bool nearRight = point.x >= r.Max.x - b * 2.0f;
        const bool nearTop = point.y < r.Min.y + b * 2.0f;
        const bool nearBottom = point.y >= r.Max.y - b * 2.0f;
        if ((top && nearLeft) || (left && nearTop))
            return FrameHit::TopLeft;
        if ((top && nearRight) || (right && nearTop))
            return FrameHit::TopRight;
        if ((bottom && nearLeft) || (left && nearBottom))
            return FrameHit::BottomLeft;
        if ((bottom && nearRight) || (right && nearBottom))
            return FrameHit::BottomRight;
        if (left)
            return FrameHit::Left;
        if (right)
            return FrameHit::Right;
        if (top)
            return FrameHit::Top;
        if (bottom)
            return FrameHit::Bottom;
    }
    // The title bars of the frame: window buttons, then the caller's items, then the caption.
    const internal::TitleBarRegions* regions = nullptr;
    const int count = internal::PreviousTitleBarRegions(&regions);
    for (int i = 0; i < count; ++i)
    {
        const internal::TitleBarRegions& bar = regions[i];
        if (bar.rootWindow != frame.window || !HasPoint(bar.bar, point))
            continue;
        if (HasPoint(bar.buttons[0], point))
            return FrameHit::MinimizeButton;
        if (HasPoint(bar.buttons[1], point))
            return FrameHit::MaximizeButton;
        if (HasPoint(bar.buttons[2], point))
            return FrameHit::CloseButton;
        if (HasPoint(bar.items, point))
            return FrameHit::Client;
        return FrameHit::Caption;
    }
    return FrameHit::Client;
}

ImVec2 AppFrameMinSize()
{
    const AppFrameRecord& frame = Records().previous;
    if (!frame.valid)
        return ImVec2(0.0f, 0.0f);
    return ImVec2(frame.minUnits.x > 0.0f ? Px(frame.minUnits.x) : 0.0f,
                  frame.minUnits.y > 0.0f ? Px(frame.minUnits.y) : 0.0f);
}

ImVec2 AppFrameMaxSize()
{
    const AppFrameRecord& frame = Records().previous;
    if (!frame.valid)
        return ImVec2(0.0f, 0.0f);
    return ImVec2(frame.maxUnits.x > 0.0f ? Px(frame.maxUnits.x) : 0.0f,
                  frame.maxUnits.y > 0.0f ? Px(frame.maxUnits.y) : 0.0f);
}

void SaveAppFramePlacement(const char* id, const FramePlacement& placement)
{
    if (ImGui::GetCurrentContext() == nullptr || !IsInitialized())
        return;
    FramePlacementState& state =
        internal::GetPersistentState<FramePlacementState>(ImHashStr(id != nullptr ? id : ""));
    state.placement = placement;
    state.known = true;
    internal::MarkStateDirty();
}

bool LoadAppFramePlacement(const char* id, FramePlacement* placement)
{
    if (placement == nullptr || ImGui::GetCurrentContext() == nullptr || !IsInitialized())
        return false;
    const FramePlacementState& state =
        internal::GetPersistentState<FramePlacementState>(ImHashStr(id != nullptr ? id : ""));
    if (!state.known)
        return false;
    *placement = state.placement;
    return true;
}

namespace internal
{

void RotateAppFrameRecord()
{
    AppFrameRecords& records = Records();
    records.previous = records.current;
    records.current = AppFrameRecord{};
    records.open = nullptr;
}

} // namespace internal

} // namespace facet
