#include "widgets/structure_stub.h"

#include <cstdio>
#include <cstring>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "facet/widgets/common.h"
#include "internal/planned_widget.h"
#include "internal/scope_stack.h"

namespace facet::internal
{
namespace
{

// One open frame of BeginStubFrame. The rect fields are the window's values before the frame
// narrowed them, restored by the End.
struct StubFrame
{
    const char* scope = nullptr;
    ImGuiWindow* window = nullptr;  // null when nothing was drawn
    ImVec2 start = {0.0f, 0.0f};
    float right = 0.0f;
    float inset = 0.0f;
    float savedWorkMaxX = 0.0f;
    float savedContentMaxX = 0.0f;
    bool pushedId = false;
};

struct StubFrames
{
    StubFrame frames[kScopeStackDepth];
    int depth = 0;
    int overflow = 0;  // frames beyond the capacity: counted, not drawn
};

StubFrames& Frames()
{
    static StubFrames frames;
    return frames;
}

bool SameName(const char* a, const char* b)
{
    return a == b || (a != nullptr && b != nullptr && std::strcmp(a, b) == 0);
}

// "<Widget>: planned", plus the label's visible part in parentheses.
void ComposeText(const char* widget, const char* label, char* out, std::size_t outSize)
{
    if (widget == nullptr)
        widget = "";
    const char* labelEnd = label != nullptr ? ImGui::FindRenderedTextEnd(label) : nullptr;
    if (label != nullptr && labelEnd > label)
        std::snprintf(out, outSize, "%s: planned (%.*s)", widget, static_cast<int>(labelEnd - label), label);
    else
        std::snprintf(out, outSize, "%s: planned", widget);
}

// Draws the placeholder text into `box`, inset by spacing.sm on both sides, vertically
// centered in the first `lineHeight` pixels of the box, shortened to what fits.
void DrawStubText(ImDrawList* drawList, const ImRect& box, float lineHeight, const char* widget, const char* label)
{
    // The caption every planned widget shares, on the first `lineHeight` pixels of the box.
    DrawPlannedCaption(drawList, ImRect(box.Min, ImVec2(box.Max.x, box.Min.y + lineHeight)), widget, label,
                       ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(box.Min, box.Max));
}

void CloseFrame(const StubFrame& frame)
{
    if (frame.window == nullptr)
        return;
    frame.window->WorkRect.Max.x = frame.savedWorkMaxX;
    frame.window->ContentRegionRect.Max.x = frame.savedContentMaxX;
    ImGuiContext& g = *GImGui;
    if (g.CurrentWindow != frame.window)
        return;  // the caller ended the window inside the frame; Dear ImGui reports that itself

    if (frame.pushedId)
        ImGui::PopID();
    ImGui::Unindent(frame.inset);
    ImGui::Dummy(ImVec2(frame.right - frame.start.x, frame.inset));
    ImGui::EndGroup();

    DrawPlannedBox(ImGui::GetWindowDrawList(), ImRect(frame.start, ImVec2(frame.right, ImGui::GetItemRectMax().y)));
}

} // namespace

int StubListRows()
{
    return T().list.inlineRows;
}

bool StubCanDraw()
{
    return PlannedCanDraw();
}

float StubWidth(float widthUnits, bool fill, float contentWidth)
{
    // A content width of 0 means one pixel, not "fill": only `fill` asks for the available width.
    return PlannedWidth(widthUnits, fill ? 0.0f : ImMax(contentWidth, 1.0f));
}

float StubContentWidth(const char* widget, const char* label)
{
    char text[160];
    ComposeText(widget, label, text, sizeof(text));
    return MeasureText(TextStyle::Footnote, text).x + 2.0f * Px(T().spacing.sm);
}

void StubItem(const char* widget, const char* label, float width, float height)
{
    // A tall placeholder (a list) keeps its text on the first line, where the finished widget
    // starts; the shared caption does that.
    PlannedWidget(widget, label, ImMax(width, 1.0f), ImMax(height, 1.0f));
}

void StubLayer(const char* widget, const char* label, bool visible)
{
    if (!visible || !StubCanDraw())
        return;
    StubItem(widget, label, StubWidth(0.0f, true), ControlHeight(ControlSize::Regular));
}

void BeginStubFrame(const char* scope, const char* widget, const char* label, bool pushId)
{
    PushScope(scope);
    StubFrames& frames = Frames();
    if (frames.depth >= kScopeStackDepth)
    {
        ++frames.overflow;
        return;
    }
    StubFrame& frame = frames.frames[frames.depth++];
    frame = StubFrame{};
    frame.scope = scope;
    if (!StubCanDraw())
        return;

    ImGuiWindow* window = GImGui->CurrentWindow;
    const Theme& theme = T();
    frame.window = window;
    frame.inset = Px(theme.spacing.sm);
    frame.start = ImGui::GetCursorScreenPos();
    frame.right = frame.start.x + StubWidth(0.0f, true);
    frame.savedWorkMaxX = window->WorkRect.Max.x;
    frame.savedContentMaxX = window->ContentRegionRect.Max.x;

    ImGui::BeginGroup();
    const float header = ControlHeight(ControlSize::Small) + frame.inset;
    const ImRect box(frame.start, ImVec2(frame.right, frame.start.y + header));
    const ImGuiID id = label != nullptr ? ImGui::GetID(label) : 0;
    ImGui::ItemSize(box);
    if (ImGui::ItemAdd(box, id, nullptr, ImGuiItemFlags_NoNav | ImGuiItemFlags_NoTabStop))
    {
        const ImRect line(ImVec2(box.Min.x, box.Min.y + frame.inset), box.Max);
        DrawStubText(ImGui::GetWindowDrawList(), line, line.GetHeight(), widget, label);
    }
    ImGui::Indent(frame.inset);
    const float innerRight = frame.right - frame.inset;
    window->WorkRect.Max.x = ImMin(window->WorkRect.Max.x, innerRight);
    window->ContentRegionRect.Max.x = ImMin(window->ContentRegionRect.Max.x, innerRight);
    if (pushId)
    {
        ImGui::PushID(label != nullptr ? label : widget);
        frame.pushedId = true;
    }
}

bool EndStubFrame(const char* scope)
{
    if (!PopScope(scope))
        return false;
    StubFrames& frames = Frames();
    if (frames.overflow > 0)
    {
        --frames.overflow;
        return true;
    }
    int found = -1;
    for (int i = frames.depth - 1; i >= 0; --i)
    {
        if (SameName(frames.frames[i].scope, scope))
        {
            found = i;
            break;
        }
    }
    if (found < 0)
        return true;
    for (int i = frames.depth - 1; i >= found; --i)
        CloseFrame(frames.frames[i]);
    frames.depth = found;
    return true;
}

void ClearStubFrames()
{
    StubFrames& frames = Frames();
    for (int i = frames.depth - 1; i >= 0; --i)
    {
        StubFrame& frame = frames.frames[i];
        if (frame.window != nullptr)
        {
            frame.window->WorkRect.Max.x = frame.savedWorkMaxX;
            frame.window->ContentRegionRect.Max.x = frame.savedContentMaxX;
        }
    }
    frames.depth = 0;
    frames.overflow = 0;
}

} // namespace facet::internal
