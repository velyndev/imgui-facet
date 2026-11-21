// WatermarkOverlay (overlays.h): a strip of name and value fields in a corner of the display.
//
// The strip is drawn on the main viewport's foreground draw list, over every window, and takes no
// input, so clicks and keys reach whatever is under it. Each field shows its icon and label in
// labelSecondary and its value in label (Footnote), spacing.md apart, on the elevated surface of
// level 1 with a hairline, so the text keeps AA contrast over any content. It sits
// overlay.viewportMargin from the corner of the display's work area and is cut with an ellipsis
// where the display is narrower than the strip.
//
// With movable, the strip becomes a small window while an overlay menu shows (BeginOverlay,
// containers.h): it can be dragged and snaps to the corner nearest the pointer on release, and with
// keyboard focus (Dear ImGui's window switching) the arrow keys move it to the next corner that way.
// The corner is kept under the id and saved in the ini file.

#include "facet/widgets/overlays.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/state_storage.h"
#include "widgets/containers/container_layout.h"
#include "widgets/overlays/overlay_parts.h"

namespace facet
{
namespace
{

struct WatermarkCorner
{
    static constexpr const char* kStateName = "Watermark";
    int corner = -1;  // ScreenCorner, -1 until the strip was first shown

    void WriteIni(ImGuiTextBuffer& out) const
    {
        if (corner >= 0)
            out.appendf("corner=%d\n", corner);
    }
    void ReadIni(const char* key, const char* value)
    {
        if (std::strcmp(key, "corner") == 0)
            corner = std::clamp(std::atoi(value), 0, 3);
    }
};

struct WatermarkDrag
{
    bool dragging = false;
    ImVec2 grab;  // pointer offset from the strip's top-left corner at the press
};

bool IsRight(ScreenCorner corner)
{
    return corner == ScreenCorner::TopRight || corner == ScreenCorner::BottomRight;
}

bool IsBottom(ScreenCorner corner)
{
    return corner == ScreenCorner::BottomLeft || corner == ScreenCorner::BottomRight;
}

ScreenCorner CornerOf(bool right, bool bottom)
{
    if (bottom)
        return right ? ScreenCorner::BottomRight : ScreenCorner::BottomLeft;
    return right ? ScreenCorner::TopRight : ScreenCorner::TopLeft;
}

// The measured strip: its size and where each field's parts go, from the left edge.
struct WatermarkLayout
{
    ImVec2 size;
    float padding = 0.0f;
    float iconSize = 0.0f;
};

float FieldWidth(const WatermarkField& field, float iconSize, float gap)
{
    float width = 0.0f;
    if (field.icon.IsValid())
        width += iconSize + gap * 0.5f;
    if (field.label != nullptr)
        width += ImGui::CalcTextSize(field.label).x;
    if (field.value != nullptr && field.value[0] != '\0')
        width += gap * 0.5f + ImGui::CalcTextSize(field.value).x;
    return width;
}

// Draws the strip with its top-left corner at `pos`, at most `maxWidth` pixels wide, into `drawList`.
// Expects the Footnote style pushed.
ImVec2 DrawWatermarkStrip(ImDrawList* drawList, ImVec2 pos, float maxWidth, std::span<const WatermarkField> fields,
                          bool measureOnly, float focusAmount)
{
    using namespace internal;
    const Theme& theme = T();
    const float padX = Px(theme.spacing.sm);
    const float padY = Px(theme.spacing.xs);
    const float gap = Px(theme.spacing.md);
    const float iconSize = std::round(ImGui::GetFontSize());
    float content = 0.0f;
    for (std::size_t i = 0; i < fields.size(); ++i)
        content += FieldWidth(fields[i], iconSize, gap) + (i > 0 ? gap : 0.0f);
    const float width = std::min(std::ceil(content + padX * 2.0f), std::max(1.0f, maxWidth));
    const float height = std::ceil(ImGui::GetFontSize() + padY * 2.0f);
    const ImVec2 size(width, height);
    if (measureOnly)
        return size;
    const float radius = Px(theme.radii.control);
    const ImVec2 max(pos.x + width, pos.y + height);
    FillRect(drawList, pos, max, ToU32(theme.colors.ElevatedSurface(1)), radius);
    StrokeRect(drawList, pos, max, ToU32(theme.colors.separator), radius, HairlineThickness());
    drawList->PushClipRect(ImVec2(pos.x + padX, pos.y), ImVec2(max.x - padX, max.y), true);
    float x = pos.x + padX;
    const float right = max.x - padX;
    const float y = pos.y + padY;
    const ImU32 secondary = ToU32(theme.colors.labelSecondary);
    const ImU32 primary = ToU32(theme.colors.label);
    for (std::size_t i = 0; i < fields.size() && x < right; ++i)
    {
        const WatermarkField& field = fields[i];
        if (i > 0)
            x += gap;
        if (field.icon.IsValid())
        {
            DrawControlIcon(drawList, field.icon, x, y + ImGui::GetFontSize() * 0.5f, iconSize, secondary);
            x += iconSize + gap * 0.5f;
        }
        if (field.label != nullptr)
        {
            const FittedText shown = FitText(field.label, nullptr, std::max(0.0f, right - x));
            drawList->AddText(ImVec2(x, y), secondary, shown.text);
            x += shown.width;
        }
        if (field.value != nullptr && field.value[0] != '\0' && x < right)
        {
            x += gap * 0.5f;
            const FittedText shown = FitText(field.value, nullptr, std::max(0.0f, right - x));
            drawList->AddText(ImVec2(x, y), primary, shown.text);
            x += shown.width;
        }
    }
    drawList->PopClipRect();
    if (focusAmount > 0.0f)
        DrawFocusRing(drawList, ImRect(pos, max), radius, focusAmount);
    return size;
}

} // namespace

void WatermarkOverlay(const char* id, std::span<const WatermarkField> fields, const WatermarkOptions& options)
{
    using namespace internal;
    ImGuiContext* g = ImGui::GetCurrentContext();
    if (g == nullptr || !g->WithinFrameScope || g->CurrentWindow == nullptr || fields.empty())
        return;
    if (id == nullptr)
        id = "";
    const Theme& theme = T();
    const ImGuiID stripId = ImGui::GetID(id);
    ScreenCorner corner = options.corner;
    WatermarkCorner* saved = nullptr;
    if (options.movable)
    {
        saved = &GetPersistentState<WatermarkCorner>(stripId);
        if (saved->corner < 0)
            saved->corner = static_cast<int>(options.corner);
        corner = static_cast<ScreenCorner>(saved->corner);
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float margin = Px(theme.overlay.viewportMargin);
    const ImRect work(ImVec2(viewport->WorkPos.x + margin, viewport->WorkPos.y + margin),
                      ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - margin,
                             viewport->WorkPos.y + viewport->WorkSize.y - margin));
    PushTextStyle(TextStyle::Footnote);
    const ImVec2 size = DrawWatermarkStrip(nullptr, ImVec2(0.0f, 0.0f), work.GetWidth(), fields, true, 0.0f);
    ImVec2 pos(IsRight(corner) ? work.Max.x - size.x : work.Min.x, IsBottom(corner) ? work.Max.y - size.y : work.Min.y);

    if (!options.movable || !OverlayMenuShown())
    {
        DrawWatermarkStrip(ImGui::GetForegroundDrawList(), pos, work.GetWidth(), fields, false, 0.0f);
        const ImRect strip(pos, ImVec2(pos.x + size.x, pos.y + size.y));
        OverlayRecordRects(OverlayPartKey("watermark", stripId), &strip, 1);
        PopTextStyle();
        return;
    }

    // Movable while an overlay menu shows: a window of the strip's size that drags and snaps.
    WatermarkDrag& drag = GetState<WatermarkDrag>(stripId);
    const ImGuiIO& io = ImGui::GetIO();
    if (drag.dragging)
        pos = ImVec2(io.MousePos.x - drag.grab.x, io.MousePos.y - drag.grab.y);
    char name[40];
    std::snprintf(name, sizeof(name), "##FacetWatermark%08X", stripId);
    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    const bool visible = ImGui::Begin(name, nullptr,
                                      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                                          ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoMove |
                                          ImGuiWindowFlags_NoBackground);
    ImGui::PopStyleVar(2);
    if (visible)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        ImGui::BringWindowToDisplayFront(window);
        ItemOptions itemOptions;
        itemOptions.press = PressMode::Click;
        itemOptions.hitTarget = false;
        const ItemState item = PartBehavior(window->GetID("##facet.strip"), window->Rect(), itemOptions);
        if (item.hovered || drag.dragging)
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
        if (item.activated && item.held && !item.keyboardPressed)
        {
            drag.dragging = true;
            drag.grab = ImVec2(io.MousePos.x - window->Pos.x, io.MousePos.y - window->Pos.y);
        }
        bool right = IsRight(corner);
        bool bottom = IsBottom(corner);
        bool moved = false;
        if (drag.dragging && !item.held)
        {
            // Snap to the corner of the display quarter the pointer is in.
            drag.dragging = false;
            right = io.MousePos.x >= viewport->Pos.x + viewport->Size.x * 0.5f;
            bottom = io.MousePos.y >= viewport->Pos.y + viewport->Size.y * 0.5f;
            moved = true;
        }
        if (item.focused)
        {
            const ImGuiID owner = item.id;
            const ImGuiKey keys[] = {ImGuiKey_LeftArrow, ImGuiKey_RightArrow, ImGuiKey_UpArrow, ImGuiKey_DownArrow};
            for (ImGuiKey key : keys)
                ImGui::SetKeyOwner(key, owner);
            if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, ImGuiInputFlags_None, owner))
                right = false, moved = true;
            if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, ImGuiInputFlags_None, owner))
                right = true, moved = true;
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, ImGuiInputFlags_None, owner))
                bottom = false, moved = true;
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, ImGuiInputFlags_None, owner))
                bottom = true, moved = true;
        }
        if (moved && saved != nullptr)
        {
            saved->corner = static_cast<int>(CornerOf(right, bottom));
            MarkStateDirty();
        }
        DrawWatermarkStrip(window->DrawList, window->Pos, work.GetWidth(), fields, false, item.focusAmount);
        const ImRect strip = window->Rect();
        OverlayRecordRects(OverlayPartKey("watermark", stripId), &strip, 1);
    }
    ImGui::End();
    PopTextStyle();
}

} // namespace facet
