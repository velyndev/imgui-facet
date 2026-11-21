// Parts the layers of the overlays family share (overlay_parts.h).

#include "widgets/overlays/overlay_parts.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "core/core_internal.h"
#include "facet/core/color.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "facet/widgets/buttons.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/scroll.h"
#include "internal/state_storage.h"

namespace facet::internal
{
namespace
{

struct OverlayLayerState
{
    bool shown = false;  // the level was opened by the open flag and has not closed since
};

struct OverlayScrollState
{
    float contentWidth = 0.0f;   // pixels, measured in the last frame
    float contentHeight = 0.0f;
};

struct OverlayScrollStack
{
    ImGuiID ids[kOverlayScrollDepth] = {};
    ImVec2 parentMax[kOverlayScrollDepth];  // the parent's content bounds before the area
    ImVec2 boxMax[kOverlayScrollDepth];     // the bottom-right corner of the content box
    int depth = 0;
};

OverlayScrollStack& ScrollStack()
{
    static OverlayScrollStack stack;
    return stack;
}

} // namespace

ImRect OverlayWorkArea()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float margin = Px(T().overlay.viewportMargin);
    ImRect area(ImVec2(viewport->WorkPos.x + margin, viewport->WorkPos.y + margin),
                ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - margin,
                       viewport->WorkPos.y + viewport->WorkSize.y - margin));
    area.Max.x = std::max(area.Max.x, area.Min.x);
    area.Max.y = std::max(area.Max.y, area.Min.y);
    return area;
}

OverlayLayerSync OverlaySyncLayer(ImGuiID level, bool* open, const PopupAnchor& anchor, const PopupOptions& options)
{
    OverlayLayerSync sync;
    if (ImGui::GetCurrentContext() == nullptr || level == 0)
        return sync;
    OverlayLayerState& state = GetState<OverlayLayerState>(level);
    sync.reason = PopupLevelClosedReason(level);
    const bool isOpen = IsPopupLevelOpen(level);
    if (open == nullptr)
    {
        if (isOpen)
            ClosePopupLevel(level);
        state.shown = false;
        return sync;
    }
    if (*open)
    {
        if (!isOpen)
        {
            if (state.shown)
            {
                // The host closed it: report the dismissal through the flag.
                *open = false;
                state.shown = false;
                sync.dismissed = true;
                return sync;
            }
            OpenPopupLevel(level, anchor, options);
            state.shown = true;
            sync.opened = true;
        }
    }
    else
    {
        if (isOpen)
            ClosePopupLevel(level);
        state.shown = false;
    }
    sync.open = IsPopupLevelOpen(level);
    return sync;
}

// ---- Scrolling content --------------------------------------------------------------------------

ImVec2 OverlayScrollSize(ImGuiID id, const OverlayScrollDesc& desc)
{
    const OverlayScrollState& state = GetState<OverlayScrollState>(id);
    float width = desc.width;
    if (width <= 0.0f)
    {
        width = std::max(desc.minWidth, state.contentWidth);
        if (desc.maxWidth > 0.0f)
            width = std::min(width, desc.maxWidth);
    }
    float height = state.contentHeight;
    if (desc.maxHeight > 0.0f)
        height = std::min(height, desc.maxHeight);
    return ImVec2(std::ceil(std::max(width, 1.0f)), std::ceil(std::max(height, 1.0f)));
}

void OverlayScrollResetStack()
{
    ScrollStack().depth = 0;
}

bool BeginOverlayScroll(ImGuiID id, const OverlayScrollDesc& desc)
{
    OverlayScrollStack& stack = ScrollStack();
    if (stack.depth >= kOverlayScrollDepth)
    {
        ReportUsageError("overlay content nested deeper than %d; it is not scrolled", kOverlayScrollDepth);
        return false;
    }
    // The area reaches its padding past the content box on every side, so the content lines up with
    // what the layer draws above it (the title) and focus rings at its edge show whole.
    const float padding = FocusRingOutset();
    const ImVec2 box = desc.fill ? ImGui::GetContentRegionAvail() : OverlayScrollSize(id, desc);
    const ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImGui::SetCursorScreenPos(ImVec2(cursor.x - padding, cursor.y - padding));
    ScrollAreaDesc area;
    area.size = ImVec2(std::max(1.0f, box.x) + padding * 2.0f, std::max(1.0f, box.y) + padding * 2.0f);
    area.fadeSurface = desc.surface;
    area.edgeFades = desc.edgeFades;
    area.padding = padding;
    const ImVec2 parentMax = GImGui->CurrentWindow->DC.CursorMaxPos;
    if (!BeginScrollAreaCore(id, area))
        return false;
    stack.ids[stack.depth] = id;
    stack.parentMax[stack.depth] = parentMax;
    stack.boxMax[stack.depth] = ImVec2(cursor.x + std::max(1.0f, box.x), cursor.y + std::max(1.0f, box.y));
    ++stack.depth;
    return true;
}

void EndOverlayScroll()
{
    OverlayScrollStack& stack = ScrollStack();
    if (stack.depth <= 0)
    {
        ReportUsageError("EndOverlayScroll without BeginOverlayScroll");
        return;
    }
    const int index = --stack.depth;
    const ImGuiID id = stack.ids[index];
    const ImGuiWindow* area = GImGui->CurrentWindow;
    OverlayScrollState& state = GetState<OverlayScrollState>(id);
    state.contentWidth = std::max(0.0f, area->DC.CursorMaxPos.x - area->DC.CursorStartPos.x);
    state.contentHeight = std::max(0.0f, area->DC.CursorMaxPos.y - area->DC.CursorStartPos.y);
    EndScrollAreaCore();
    // The area's outset lies in the layer's padding: it does not count as content of the layer, so
    // the layer, sized to its content, never shows a scrollbar of its own.
    ImGuiWindow* parent = GImGui->CurrentWindow;
    parent->DC.CursorMaxPos.x = std::max(stack.parentMax[index].x, stack.boxMax[index].x);
    parent->DC.CursorMaxPos.y = std::max(stack.parentMax[index].y, stack.boxMax[index].y);
}

// ---- Title rows ----------------------------------------------------------------------------------

float OverlayTitleHeight()
{
    const float line = MeasureText(TextStyle::Title3, "Ag").y;
    return std::ceil(std::max(line, ControlHeight(ControlSize::Small)));
}

void DrawOverlayTitle(ImGuiID id, const char* label, ImVec2 pos, float width)
{
    if (label == nullptr)
        return;
    const char* end = ImGui::FindRenderedTextEnd(label);
    if (end == label)
        return;
    ImGuiWindow* window = GImGui->CurrentWindow;
    const float height = OverlayTitleHeight();
    PushTextStyle(TextStyle::Title3);
    const FittedText shown = FitText(label, end, std::max(0.0f, width));
    window->DrawList->AddText(ImVec2(pos.x, CenteredTextY(pos.y, height)), ToU32(T().colors.label), shown.text);
    PopTextStyle();
    const ImRect rect(pos, ImVec2(pos.x + width, pos.y + height));
    const bool hovered = ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(rect.Min, rect.Max);
    CutTextTooltip(ImHashStr("##facet.title", 0, id), rect, hovered, false, label, end, shown.truncated);
}

bool OverlayCloseButton(ImGuiID level, float right, float centerY)
{
    const float side = ControlHeight(ControlSize::Small);
    ImGui::SetCursorScreenPos(ImVec2(right - side, std::round(centerY - side * 0.5f)));
    char label[96];
    ImFormatString(label, sizeof(label), "%s###facet.close", GetStrings().close);
    const bool pressed = IconButton(label, Icon("close"), {.variant = ButtonVariant::Plain, .size = ControlSize::Small});
    const ImRect rect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    OverlayRecordRects(OverlayPartKey("close", level), &rect, 1);
    return pressed;
}

// ---- Where the parts are --------------------------------------------------------------------------

namespace
{

struct OverlayPartRects
{
    ImRect rects[3];
    int frame = -100;  // Dear ImGui frame of the record
};

} // namespace

ImGuiID OverlayPartKey(const char* part, ImGuiID owner)
{
    return ImHashStr(part, 0, owner);
}

void OverlayRecordRects(ImGuiID key, const ImRect* rects, int count)
{
    OverlayPartRects& record = GetState<OverlayPartRects>(key);
    for (int i = 0; i < count && i < 3; ++i)
        record.rects[i] = rects[i];
    record.frame = ImGui::GetFrameCount();
}

bool OverlayReadRects(ImGuiID key, ImRect* rects, int count)
{
    if (ImGui::GetCurrentContext() == nullptr)
        return false;
    const OverlayPartRects& record = GetState<OverlayPartRects>(key);
    if (record.frame + 2 < ImGui::GetFrameCount())
        return false;
    for (int i = 0; i < count && i < 3; ++i)
        rects[i] = record.rects[i];
    return true;
}

bool OverlayCloseButtonRect(ImGuiID level, ImRect* rect)
{
    ImRect read;
    if (!OverlayReadRects(OverlayPartKey("close", level), &read, 1))
        return false;
    if (rect != nullptr)
        *rect = read;
    return true;
}

bool OverlayConfirmButtonRects(ImGuiID level, ImRect* cancel, ImRect* confirm)
{
    ImRect read[2];
    if (!OverlayReadRects(OverlayPartKey("confirm", level), read, 2))
        return false;
    if (cancel != nullptr)
        *cancel = read[0];
    if (confirm != nullptr)
        *confirm = read[1];
    return true;
}

bool OverlayToastRects(ToastId toast, ImRect* body, ImRect* action, ImRect* close)
{
    ImRect read[3];
    if (!toast.IsValid() || !OverlayReadRects(OverlayPartKey("toast", toast.value), read, 3))
        return false;
    if (body != nullptr)
        *body = read[0];
    if (action != nullptr)
        *action = read[1];
    if (close != nullptr)
        *close = read[2];
    return true;
}

bool OverlayPaletteRowRect(ImGuiID level, int item, ImRect* rect)
{
    ImRect read;
    if (!OverlayReadRects(OverlayPartKey("row", level) + static_cast<ImGuiID>(item), &read, 1))
        return false;
    if (rect != nullptr)
        *rect = read;
    return true;
}

bool OverlayBarButtonRects(ImGuiID id, ImRect* revert, ImRect* apply)
{
    ImRect read[2];
    if (!OverlayReadRects(OverlayPartKey("bar", id), read, 2))
        return false;
    if (revert != nullptr)
        *revert = read[0];
    if (apply != nullptr)
        *apply = read[1];
    return true;
}

bool OverlayWatermarkRect(ImGuiID id, ImRect* rect)
{
    ImRect read;
    if (!OverlayReadRects(OverlayPartKey("watermark", id), &read, 1))
        return false;
    if (rect != nullptr)
        *rect = read;
    return true;
}

} // namespace facet::internal
