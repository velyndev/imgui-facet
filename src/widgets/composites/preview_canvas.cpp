// BeginPreviewCanvas and EndPreviewCanvas (facet/widgets/composites.h): a letterboxed preview of a
// screen of `referenceSize` reference pixels, scaled to fit the widget. The bands around the screen
// are fill[3]; the screen is the window background with the controlBorder hairline of a control
// (DrawOutline), the optional grid every gridSpacing reference pixels in the separator color, and the
// optional safe area as a dashed frame (media.dashLength and dashGap) in controlBorder, a reference
// line as quiet as the screen edge. Canvas elements (canvas_element.cpp) are
// submitted between Begin and End; End draws, on top of them, the center guides while one is moved
// and the guide an element snapped to, in the focusRing color (3:1 on every surface). A click on
// empty canvas clears the selection. Content is always submitted; EndPreviewCanvas is always called.

#include "facet/widgets/composites.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/item.h"
#include "internal/scope_stack.h"
#include "internal/state_storage.h"
#include "widgets/composites/editors_frame.h"

namespace facet
{
namespace
{

// More grid lines than this along one axis would turn the screen into a texture: the grid is left out.
constexpr int kPreviewCanvasMaxGridLines = 64;
// The aspect of a canvas without a usable reference size (height per width).
constexpr float kPreviewCanvasEmptyAspect = 9.0f / 16.0f;

struct PreviewCanvasState
{
    ImGuiID selected = 0;
    ImRect rects[2][internal::kPreviewCanvasMaxElements];
    ImGuiID ids[2][internal::kPreviewCanvasMaxElements] = {};
    int counts[2] = {};
    int frame = -1;
    int current = 0;  // the slot this frame fills; the other holds the last frame
};

struct PreviewCanvasEntry
{
    internal::PreviewCanvasFrame frame;
    PreviewCanvasState* state = nullptr;
    bool open = false;  // the group and clip rect were pushed
};

struct PreviewCanvasStack
{
    PreviewCanvasEntry entries[4];
    int depth = 0;
    int overflow = 0;
};

PreviewCanvasStack& Stack()
{
    static PreviewCanvasStack stack;
    return stack;
}

// A dashed hairline rectangle.
void PreviewCanvasDashedRect(ImDrawList* drawList, const ImRect& r, float dash, float gap, ImU32 color)
{
    const float step = std::max(1.0f, dash + gap);
    for (float x = r.Min.x; x < r.Max.x; x += step)
    {
        HairlineH(drawList, x, std::min(x + dash, r.Max.x), r.Min.y, color);
        HairlineH(drawList, x, std::min(x + dash, r.Max.x), r.Max.y - HairlineThickness(), color);
    }
    for (float y = r.Min.y; y < r.Max.y; y += step)
    {
        HairlineV(drawList, r.Min.x, y, std::min(y + dash, r.Max.y), color);
        HairlineV(drawList, r.Max.x - HairlineThickness(), y, std::min(y + dash, r.Max.y), color);
    }
}

} // namespace

void BeginPreviewCanvas(const char* id, ImVec2 referenceSize, const PreviewCanvasOptions& options)
{
    internal::PushScope("BeginPreviewCanvas");
    PreviewCanvasStack& stack = Stack();
    if (stack.depth >= static_cast<int>(IM_ARRAYSIZE(stack.entries)))
    {
        ++stack.overflow;
        return;
    }
    PreviewCanvasEntry& entry = stack.entries[stack.depth++];
    entry = PreviewCanvasEntry{};
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return;
    if (id == nullptr)
        id = "";
    ImGuiContext& g = *GImGui;
    const Theme& t = T();
    const Colors& c = t.colors;
    const internal::NextItemData next = internal::TakeNextItemData();
    const ImGuiID canvasId = window->GetID(id);
    PreviewCanvasState& state = internal::GetState<PreviewCanvasState>(canvasId);
    if (state.frame != g.FrameCount)
    {
        state.current = 1 - state.current;
        state.counts[state.current] = 0;
        state.frame = g.FrameCount;
    }

    const bool usable = referenceSize.x > 0.0f && referenceSize.y > 0.0f && std::isfinite(referenceSize.x) &&
                        std::isfinite(referenceSize.y);
    const float width = std::round(internal::ResolveItemWidth(options.width, next, kFill));
    const float aspect = usable ? referenceSize.y / referenceSize.x : kPreviewCanvasEmptyAspect;
    const float height = std::max(1.0f, std::round(options.height > 0.0f ? Px(options.height) : width * aspect));
    const ImVec2 origin(std::round(window->DC.CursorPos.x), std::round(window->DC.CursorPos.y));
    const ImRect frame(origin, ImVec2(origin.x + width, origin.y + height));

    // The screen, letterboxed into the frame.
    float scale = 1.0f;
    ImRect screen = frame;
    if (usable)
    {
        scale = std::min(width / referenceSize.x, height / referenceSize.y);
        const ImVec2 size(referenceSize.x * scale, referenceSize.y * scale);
        const ImVec2 min(std::round(frame.Min.x + (width - size.x) * 0.5f), std::round(frame.Min.y + (height - size.y) * 0.5f));
        screen = ImRect(min, ImVec2(min.x + std::round(size.x), min.y + std::round(size.y)));
    }

    ImGui::BeginGroup();
    ImGui::ItemSize(frame.GetSize());
    ImGui::PushID(id);
    // Empty canvas takes the click that clears the selection; it is no Tab stop.
    internal::ItemOptions backOptions;
    backOptions.navigable = false;
    backOptions.hitTarget = false;
    backOptions.handCursor = false;
    backOptions.allowOverlap = true;  // the elements submitted after it take their own hover and clicks
    const internal::ForcedItemRequest noForced;
    backOptions.forced = &noForced;
    internal::ApplyNextItemData(next);
    const internal::ItemState back = internal::PartBehavior(ImGui::GetID("##canvas"), frame, backOptions);
    if (back.pressed)
        state.selected = 0;

    ImDrawList* drawList = window->DrawList;
    if (back.visible)
    {
        FillRect(drawList, frame.Min, frame.Max, ToU32(c.fill[3]), Px(t.radii.control));
        if (usable)
        {
            FillRect(drawList, screen.Min, screen.Max, ToU32(c.bg));
            if (options.gridSpacing > 0.0f)
            {
                const float step = options.gridSpacing * scale;
                if (step >= 1.0f && referenceSize.x / options.gridSpacing <= kPreviewCanvasMaxGridLines &&
                    referenceSize.y / options.gridSpacing <= kPreviewCanvasMaxGridLines)
                {
                    const ImU32 grid = ToU32(c.separator);
                    for (float x = screen.Min.x + step; x < screen.Max.x - 0.5f; x += step)
                        HairlineV(drawList, std::round(x), screen.Min.y, screen.Max.y, grid);
                    for (float y = screen.Min.y + step; y < screen.Max.y - 0.5f; y += step)
                        HairlineH(drawList, screen.Min.x, screen.Max.x, std::round(y), grid);
                }
            }
            if (options.safeArea > 0.0f)
            {
                const float share = std::min(options.safeArea, 0.5f);
                const ImVec2 inset(screen.GetWidth() * share, screen.GetHeight() * share);
                const ImRect safe(ImVec2(std::round(screen.Min.x + inset.x), std::round(screen.Min.y + inset.y)),
                                  ImVec2(std::round(screen.Max.x - inset.x), std::round(screen.Max.y - inset.y)));
                PreviewCanvasDashedRect(drawList, safe, Px(t.media.dashLength), Px(t.media.dashGap), ToU32(c.controlBorder));
            }
            // The screen edge bounds the area elements are dragged in, so it takes the outline of a
            // control (controlBorder); the grid inside stays in separator.
            internal::DrawOutline(drawList, screen, 0.0f, internal::ControlOutlineColor({}));
        }
    }

    internal::PreviewCanvasFrame& f = entry.frame;
    f.valid = usable;
    f.id = canvasId;
    f.window = window;
    f.frame = frame;
    f.screen = screen;
    f.outerClip = window->ClipRect;
    f.reference = referenceSize;
    f.scale = scale;
    f.safeArea = std::clamp(options.safeArea, 0.0f, 0.5f);
    f.centerGuides = options.centerGuides;
    f.selected = &state.selected;
    const int last = 1 - state.current;
    f.others = state.rects[last];
    f.otherIds = state.ids[last];
    f.otherCount = state.counts[last];
    entry.state = &state;
    entry.open = true;
    // Elements larger than the canvas stay inside the widget.
    ImGui::PushClipRect(frame.Min, frame.Max, true);
}

void EndPreviewCanvas()
{
    if (!internal::PopScope("BeginPreviewCanvas"))
        return;
    PreviewCanvasStack& stack = Stack();
    if (stack.overflow > 0)
    {
        --stack.overflow;
        return;
    }
    if (stack.depth == 0)
        return;
    const PreviewCanvasEntry entry = stack.entries[--stack.depth];
    if (!entry.open || GImGui->CurrentWindow != entry.frame.window)
        return;
    const internal::PreviewCanvasFrame& f = entry.frame;
    const Colors& c = T().colors;
    ImDrawList* drawList = f.window->DrawList;
    const ImU32 guide = ToU32(c.focusRing);
    if (f.valid && f.dragging && f.centerGuides)
    {
        HairlineV(drawList, std::round(f.screen.GetCenter().x), f.screen.Min.y, f.screen.Max.y, ToU32(c.labelTertiary));
        HairlineH(drawList, f.screen.Min.x, f.screen.Max.x, std::round(f.screen.GetCenter().y), ToU32(c.labelTertiary));
    }
    if (f.valid && f.snapX >= 0.0f)
        HairlineV(drawList, std::round(f.screen.Min.x + f.snapX * f.scale), f.frame.Min.y, f.frame.Max.y, guide);
    if (f.valid && f.snapY >= 0.0f)
        HairlineH(drawList, f.frame.Min.x, f.frame.Max.x, std::round(f.screen.Min.y + f.snapY * f.scale), guide);
    ImGui::PopClipRect();
    ImGui::PopID();
    // Back to the end of the canvas, whatever the elements did with the cursor.
    f.window->DC.CursorPos = ImVec2(f.frame.Min.x, f.frame.Max.y);
    ImGui::Dummy(ImVec2(f.frame.GetWidth(), 0.0f));
    ImGui::EndGroup();
    internal::FinishWidgetGroup(f.id);
}

namespace internal
{

PreviewCanvasFrame* CurrentPreviewCanvas()
{
    PreviewCanvasStack& stack = Stack();
    if (stack.depth == 0 || stack.overflow > 0)
        return nullptr;
    PreviewCanvasEntry& entry = stack.entries[stack.depth - 1];
    return entry.open ? &entry.frame : nullptr;
}

void PreviewCanvasRecordElement(ImGuiID elementId, const ImRect& rect)
{
    PreviewCanvasStack& stack = Stack();
    if (stack.depth == 0 || stack.overflow > 0)
        return;
    PreviewCanvasEntry& entry = stack.entries[stack.depth - 1];
    if (entry.state == nullptr)
        return;
    PreviewCanvasState& state = *entry.state;
    int& count = state.counts[state.current];
    if (count >= kPreviewCanvasMaxElements)
        return;
    state.rects[state.current][count] = rect;
    state.ids[state.current][count] = elementId;
    ++count;
}

void ClearPreviewCanvasStack()
{
    PreviewCanvasStack& stack = Stack();
    stack.depth = 0;
    stack.overflow = 0;
}

} // namespace internal

} // namespace facet
