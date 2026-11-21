// BeginShell and EndShell (containers.h): the floating menu window.
//
// The shell is a top-level Dear ImGui window without Dear ImGui's decorations (no title bar,
// background, border, scrollbar, own resizing or moving) that Facet draws and moves itself:
//   - the surface: glass with its shadow (effects.h), or the opaque elevated surface with the
//     elevation shadow and a hairline where blur is off or unavailable;
//   - the title bar (containers.titleBarHeight): icon, the visible label part as title (cut with an
//     ellipsis and a tooltip), the close button; its free area is the drag region;
//   - resize bands of containers.resizeBorder along every edge and in every corner, inside the window
//     rectangle, so the pointer over a band is over the shell and never over what is behind it;
//     Facet's grip marks the bottom-right corner;
//   - the content: a scroll area of the scroll core below the title bar, inset by the bands.
//
// Position and size are kept in design units (the position relative to the display's work area)
// under the shell's name and saved in the Dear ImGui ini file, so the shell reopens at the same
// place and size at any scale. Every frame the rectangle is rebuilt from those units, a drag in
// progress is applied before the window begins (so the window follows the pointer without a
// frame of lag), the size is clamped to the minimum and maximum, and with keepInDisplay the
// rectangle is clamped into the display: a smaller display or a larger scale brings the shell
// back into view.

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
#include "facet/core/effects.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/popup_host.h"
#include "internal/scope_stack.h"
#include "internal/scroll.h"
#include "internal/state_storage.h"
#include "widgets/buttons/button_look.h"
#include "widgets/containers/container_layout.h"

namespace facet
{
namespace
{

constexpr Icon kShellCloseIcon("close");

// The elevation of the shell's shadow: a floating window above the page, below dialogs.
constexpr int kShellElevation = 2;
// Shells open at once (a shell opened from inside another one's content).
constexpr int kShellDepth = 8;

// Edges a resize band moves, as bits.
enum ShellEdge : int
{
    kShellEdgeNone = 0,
    kShellEdgeLeft = 1 << 0,
    kShellEdgeRight = 1 << 1,
    kShellEdgeTop = 1 << 2,
    kShellEdgeBottom = 1 << 3,
};
constexpr int kShellMove = 1 << 4;  // the title bar drag

struct ShellState
{
    static constexpr const char* kStateName = "Shell";
    ImVec2 pos = {0.0f, 0.0f};   // units from the display's work area origin
    ImVec2 size = {0.0f, 0.0f};  // units
    bool known = false;          // read from the ini file or placed once
    bool collapsed = false;

    void WriteIni(ImGuiTextBuffer& out) const
    {
        out.appendf("pos=%.2f,%.2f\nsize=%.2f,%.2f\ncollapsed=%d\n", pos.x, pos.y, size.x, size.y, collapsed ? 1 : 0);
    }
    // Reads "a,b" into two floats; false when the value is not two finite numbers.
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
    void ReadIni(const char* key, const char* value)
    {
        ImVec2 pair;
        if (std::strcmp(key, "pos") == 0 && ReadPair(value, &pair))
        {
            pos = pair;
            known = true;
        }
        else if (std::strcmp(key, "size") == 0 && ReadPair(value, &pair) && pair.x > 0.0f && pair.y > 0.0f)
        {
            size = pair;
        }
        else if (std::strcmp(key, "collapsed") == 0)
        {
            collapsed = std::atoi(value) != 0;
        }
    }
};

// A drag of the title bar or a resize band, applied before the window begins.
struct ShellDrag
{
    int mode = 0;            // kShellMove or ShellEdge bits; 0 none
    ImGuiID item = 0;        // the band or title item that holds the mouse
    ImVec2 startMouse = {0.0f, 0.0f};
    ImRect startRect;        // pixels
};

struct ShellEntry
{
    ImGuiWindow* window = nullptr;  // the shell window
};

struct ShellStack
{
    ShellEntry entries[kShellDepth];
    int depth = 0;
    int overflow = 0;
};

ShellStack& Shells()
{
    static ShellStack stack;
    return stack;
}

bool ShellInFrame()
{
    const ImGuiContext* g = ImGui::GetCurrentContext();
    return IsInitialized() && g != nullptr && g->WithinFrameScope;
}

// The lengths of a shell at the current scale, pixels.
struct ShellMetrics
{
    float scale = 1.0f;     // pixels per unit
    float titleHeight = 0.0f;
    float band = 0.0f;      // resize band thickness
    float corner = 0.0f;    // side of a corner band
    float radius = 0.0f;
    float snap = 0.0f;
    ImVec2 minSize;
    ImVec2 maxSize;
    ImRect display;         // the main viewport's work area
};

ShellMetrics GetShellMetrics(const ShellOptions& options)
{
    const Theme& t = T();
    ShellMetrics m;
    m.scale = Px(1.0f);
    m.titleHeight = Px(t.containers.titleBarHeight);
    m.band = std::max(1.0f, Px(t.containers.resizeBorder));
    m.corner = m.band * 2.0f;
    m.radius = Px(t.radii.window);
    m.snap = Px(t.spacing.lg);
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    m.display = ImRect(viewport->WorkPos, ImVec2(viewport->WorkPos.x + viewport->WorkSize.x,
                                                 viewport->WorkPos.y + viewport->WorkSize.y));
    // The smallest shell (containers.shellMinSize), never smaller than the title bar and one
    // control row inside the content padding.
    const ImVec2 floor = ImMax(Px(t.containers.shellMinSize),
                               ImVec2(Px(t.overlay.menuMinWidth),
                                      m.titleHeight + ControlHeight(ControlSize::Regular) + Px(t.spacing.lg) * 2.0f));
    m.minSize = ImVec2(options.minSize.x > 0.0f ? Px(options.minSize.x) : floor.x,
                       options.minSize.y > 0.0f ? Px(options.minSize.y) : floor.y);
    m.maxSize = ImVec2(options.maxSize.x > 0.0f ? Px(options.maxSize.x) : m.display.GetWidth(),
                       options.maxSize.y > 0.0f ? Px(options.maxSize.y) : m.display.GetHeight());
    if (options.keepInDisplay)
        m.maxSize = ImMin(m.maxSize, m.display.GetSize());
    m.minSize = ImMin(m.minSize, ImMax(m.maxSize, ImVec2(1.0f, 1.0f)));
    m.maxSize = ImMax(m.maxSize, m.minSize);
    return m;
}

// The first rectangle of a shell without a saved one, pixels.
ImRect FirstShellRect(const ShellOptions& options, const ShellMetrics& m)
{
    const Theme& t = T();
    const ImVec2 units = options.initialSize.x > 0.0f && options.initialSize.y > 0.0f
                             ? options.initialSize
                             : t.containers.shellSize;
    const ImVec2 size = ImClamp(Px(units), m.minSize, m.maxSize);
    ImVec2 pos;
    if (options.centered)
    {
        pos = ImVec2(std::floor(m.display.GetCenter().x - size.x * 0.5f), std::floor(m.display.GetCenter().y - size.y * 0.5f));
    }
    else
    {
        const float margin = Px(t.overlay.viewportMargin);
        const bool right = options.corner == ScreenCorner::TopRight || options.corner == ScreenCorner::BottomRight;
        const bool bottom = options.corner == ScreenCorner::BottomLeft || options.corner == ScreenCorner::BottomRight;
        pos.x = right ? m.display.Max.x - margin - size.x : m.display.Min.x + margin;
        pos.y = bottom ? m.display.Max.y - margin - size.y : m.display.Min.y + margin;
    }
    return ImRect(pos, ImVec2(pos.x + size.x, pos.y + size.y));
}

// Moves the dragged rectangle to the display edges and corners within m.snap.
void SnapShellRect(ImRect& r, const ShellMetrics& m)
{
    const ImVec2 size = r.GetSize();
    if (std::fabs(r.Min.x - m.display.Min.x) <= m.snap)
        r.Min.x = m.display.Min.x;
    else if (std::fabs(r.Max.x - m.display.Max.x) <= m.snap)
        r.Min.x = m.display.Max.x - size.x;
    if (std::fabs(r.Min.y - m.display.Min.y) <= m.snap)
        r.Min.y = m.display.Min.y;
    else if (std::fabs(r.Max.y - m.display.Max.y) <= m.snap)
        r.Min.y = m.display.Max.y - size.y;
    r.Max = ImVec2(r.Min.x + size.x, r.Min.y + size.y);
}

// The rectangle while a drag is under way: moved, or resized on the dragged edges with the
// opposite edges fixed, within the size limits (and the display with keepInDisplay).
ImRect DraggedShellRect(const ShellDrag& drag, ImVec2 mouse, const ShellOptions& options, const ShellMetrics& m)
{
    const ImVec2 delta(mouse.x - drag.startMouse.x, mouse.y - drag.startMouse.y);
    ImRect r = drag.startRect;
    if (drag.mode == kShellMove)
    {
        r.Translate(delta);
        if (options.snapToEdges)
            SnapShellRect(r, m);
        return r;
    }
    const bool keep = options.keepInDisplay;
    if ((drag.mode & kShellEdgeLeft) != 0)
    {
        float x = drag.startRect.Min.x + delta.x;
        if (keep)
            x = std::max(x, m.display.Min.x);
        x = std::clamp(x, drag.startRect.Max.x - m.maxSize.x, drag.startRect.Max.x - m.minSize.x);
        r.Min.x = x;
    }
    if ((drag.mode & kShellEdgeRight) != 0)
    {
        float x = drag.startRect.Max.x + delta.x;
        if (keep)
            x = std::min(x, m.display.Max.x);
        x = std::clamp(x, drag.startRect.Min.x + m.minSize.x, drag.startRect.Min.x + m.maxSize.x);
        r.Max.x = x;
    }
    if ((drag.mode & kShellEdgeTop) != 0)
    {
        float y = drag.startRect.Min.y + delta.y;
        if (keep)
            y = std::max(y, m.display.Min.y);
        y = std::clamp(y, drag.startRect.Max.y - m.maxSize.y, drag.startRect.Max.y - m.minSize.y);
        r.Min.y = y;
    }
    if ((drag.mode & kShellEdgeBottom) != 0)
    {
        float y = drag.startRect.Max.y + delta.y;
        if (keep)
            y = std::min(y, m.display.Max.y);
        y = std::clamp(y, drag.startRect.Min.y + m.minSize.y, drag.startRect.Min.y + m.maxSize.y);
        r.Max.y = y;
    }
    return r;
}

// The size limits, then the display: the rectangle a shell is drawn with.
ImRect ClampShellRect(ImRect r, const ShellOptions& options, const ShellMetrics& m)
{
    const ImVec2 size = ImClamp(r.GetSize(), m.minSize, m.maxSize);
    r.Max = ImVec2(r.Min.x + size.x, r.Min.y + size.y);
    if (options.keepInDisplay)
    {
        const ImVec2 room(m.display.GetWidth() - size.x, m.display.GetHeight() - size.y);
        r.Min.x = std::clamp(r.Min.x, m.display.Min.x, m.display.Min.x + std::max(0.0f, room.x));
        r.Min.y = std::clamp(r.Min.y, m.display.Min.y, m.display.Min.y + std::max(0.0f, room.y));
        r.Max = ImVec2(r.Min.x + size.x, r.Min.y + size.y);
    }
    return r;
}

// Stores `r` (pixels) in units when it differs from what is stored, and marks the ini dirty.
void StoreShellRect(ShellState& state, const ImRect& r, const ShellMetrics& m)
{
    const ImVec2 pos((r.Min.x - m.display.Min.x) / m.scale, (r.Min.y - m.display.Min.y) / m.scale);
    const ImVec2 size(r.GetWidth() / m.scale, r.GetHeight() / m.scale);
    constexpr float kEpsilon = 0.01f;  // units; float noise of the conversion is not a change
    const bool changed = !state.known || std::fabs(pos.x - state.pos.x) > kEpsilon ||
                         std::fabs(pos.y - state.pos.y) > kEpsilon || std::fabs(size.x - state.size.x) > kEpsilon ||
                         std::fabs(size.y - state.size.y) > kEpsilon;
    if (!changed)
        return;
    state.pos = pos;
    state.size = size;
    state.known = true;
    internal::MarkStateDirty();
}

ImGuiMouseCursor ShellEdgeCursor(int edges)
{
    const bool horizontal = (edges & (kShellEdgeLeft | kShellEdgeRight)) != 0;
    const bool vertical = (edges & (kShellEdgeTop | kShellEdgeBottom)) != 0;
    if (horizontal && vertical)
    {
        const bool falling = edges == (kShellEdgeLeft | kShellEdgeTop) || edges == (kShellEdgeRight | kShellEdgeBottom);
        return falling ? ImGuiMouseCursor_ResizeNWSE : ImGuiMouseCursor_ResizeNESW;
    }
    return horizontal ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS;
}

// The resize bands: corners first, so a corner wins where it meets an edge. Starts a resize drag
// on a press and returns whether a band is hovered or held (for the grip's emphasis).
bool ShellResizeBands(ImGuiWindow* window, const ImRect& r, const ShellMetrics& m, ShellDrag& drag)
{
    struct Band
    {
        int edges;
        ImRect rect;
    };
    const float b = m.band;
    const float c = m.corner;
    const Band bands[] = {
        {kShellEdgeLeft | kShellEdgeTop, ImRect(r.Min, ImVec2(r.Min.x + c, r.Min.y + c))},
        {kShellEdgeRight | kShellEdgeTop, ImRect(ImVec2(r.Max.x - c, r.Min.y), ImVec2(r.Max.x, r.Min.y + c))},
        {kShellEdgeLeft | kShellEdgeBottom, ImRect(ImVec2(r.Min.x, r.Max.y - c), ImVec2(r.Min.x + c, r.Max.y))},
        {kShellEdgeRight | kShellEdgeBottom, ImRect(ImVec2(r.Max.x - c, r.Max.y - c), r.Max)},
        {kShellEdgeLeft, ImRect(ImVec2(r.Min.x, r.Min.y + c), ImVec2(r.Min.x + b, r.Max.y - c))},
        {kShellEdgeRight, ImRect(ImVec2(r.Max.x - b, r.Min.y + c), ImVec2(r.Max.x, r.Max.y - c))},
        {kShellEdgeTop, ImRect(ImVec2(r.Min.x + c, r.Min.y), ImVec2(r.Max.x - c, r.Min.y + b))},
        {kShellEdgeBottom, ImRect(ImVec2(r.Min.x + c, r.Max.y - b), ImVec2(r.Max.x - c, r.Max.y))},
    };
    internal::ItemOptions options;
    options.press = internal::PressMode::Click;
    options.navigable = false;
    options.tabStop = false;
    options.hitTarget = false;
    options.handCursor = false;
    bool any = false;
    for (const Band& band : bands)
    {
        const ImGuiID id = window->GetID(band.edges);
        const internal::ItemState item = internal::PartBehavior(id, band.rect, options);
        if (item.hovered || item.active)
        {
            ImGui::SetMouseCursor(ShellEdgeCursor(band.edges));
            any = true;
        }
        if (item.activated && !item.disabled)
        {
            drag.mode = band.edges;
            drag.item = id;
            drag.startMouse = ImGui::GetIO().MousePos;
            drag.startRect = r;
        }
    }
    return any;
}

// Facet's grip in the bottom-right corner: two short diagonal hairlines, labelSecondary while a
// band is hovered or held and labelTertiary otherwise, so resizing is discoverable without a
// heavy mark.
void DrawShellGrip(ImDrawList* drawList, const ImRect& r, const ShellMetrics& m, bool emphasized)
{
    const Colors& c = T().colors;
    const ImU32 color = ToU32(emphasized ? c.labelSecondary : c.labelTertiary);
    // Inside the rounded corner: the radius's straight run ends about 0.3 of it from the corner.
    constexpr float kGripInset = 0.3f;
    const float inset = std::floor(m.radius * kGripInset) + m.band * 0.5f;
    const ImVec2 corner(r.Max.x - inset, r.Max.y - inset);
    const float thickness = HairlineThickness() * 1.5f;
    for (int i = 1; i <= 2; ++i)
    {
        const float len = m.band * static_cast<float>(i);
        drawList->AddLine(ImVec2(corner.x - len, corner.y), ImVec2(corner.x, corner.y - len), color, thickness);
    }
}

// The surface: glass or the opaque elevated surface, with the shadow outside the window.
void DrawShellSurface(ImDrawList* drawList, const ImRect& r, const ShellMetrics& m, bool glass)
{
    const Theme& t = T();
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::PushClipRect(viewport->Pos, ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y), false);
    if (glass)
    {
        GlassSpec spec;
        spec.shadow = ElevationShadow(kShellElevation);
        DrawGlass(drawList, r.Min, r.Max, m.radius, spec);
    }
    else
    {
        DrawElevation(drawList, r.Min, r.Max, m.radius, kShellElevation);
        FillRect(drawList, r.Min, r.Max, ToU32(t.colors.ElevatedSurface(kShellElevation)), m.radius);
        StrokeRect(drawList, r.Min, r.Max, ToU32(t.colors.separator), m.radius, HairlineThickness());
    }
    ImGui::PopClipRect();
}

// The title bar: icon, title, close button and the drag region. Returns the close press.
bool ShellTitleBar(ImGuiWindow* window, const char* label, bool* open, const ShellOptions& options, const ImRect& r,
                   const ShellMetrics& m, ShellState& state, ShellDrag& drag)
{
    const Theme& t = T();
    ImDrawList* drawList = window->DrawList;
    const ImRect bar(r.Min, ImVec2(r.Max.x, r.Min.y + m.titleHeight));
    const float pad = Px(t.spacing.md);
    const internal::ControlMetrics cm = internal::GetControlMetrics(ControlSize::Regular);

    // The close button: a plain square button at the trailing end.
    bool closed = false;
    float titleRight = bar.Max.x - pad;
    if (open != nullptr)
    {
        const float side = cm.height;
        const float top = std::floor(bar.GetCenter().y - side * 0.5f);
        const ImRect rect(ImVec2(bar.Max.x - (bar.GetHeight() - side) * 0.5f - side, top),
                          ImVec2(bar.Max.x - (bar.GetHeight() - side) * 0.5f, top + side));
        const internal::ItemState item = internal::PartBehavior(window->GetID("##close"), rect);
        if (item.visible)
        {
            const internal::ButtonColors colors = internal::ButtonLook(ButtonVariant::Plain, item);
            if (colors.fill.w > 0.0f)
                FillRect(drawList, rect.Min, rect.Max, ToU32(colors.fill), cm.radius);
            const internal::ControlTextScope text(cm);
            internal::DrawControlIcon(drawList, kShellCloseIcon, std::round(rect.GetCenter().x - text.IconSize() * 0.5f),
                                      rect.GetCenter().y, text.IconSize(), ToU32(colors.content));
            internal::DrawItemFocusRing(item, cm.radius);
        }
        if (internal::BeginPopupTooltip(item.id, item.rect, item.hovered, item.focusVisible))
        {
            ImGui::TextUnformatted(GetStrings().close);
            internal::EndPopupTooltip();
        }
        closed = item.pressed;
        titleRight = rect.Min.x - Px(t.spacing.sm);
    }

    // The drag region: the free part of the bar.
    const ImRect free(bar.Min, ImVec2(std::max(bar.Min.x, titleRight), bar.Max.y));
    internal::ItemOptions dragOptions;
    dragOptions.press = internal::PressMode::Click;
    dragOptions.navigable = false;
    dragOptions.tabStop = false;
    dragOptions.hitTarget = false;
    dragOptions.handCursor = false;
    const internal::ItemState dragItem = internal::PartBehavior(window->GetID("##titlebar"), free, dragOptions);
    if (dragItem.activated && options.movable)
    {
        drag.mode = kShellMove;
        drag.item = dragItem.id;
        drag.startMouse = ImGui::GetIO().MousePos;
        drag.startRect = r;
    }
    if (options.collapsible && dragItem.hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        state.collapsed = !state.collapsed;
        drag.mode = 0;
        internal::MarkStateDirty();
    }

    // Icon and title, centered on the bar.
    const internal::ControlTextScope text(TextStyle::Headline);
    float x = bar.Min.x + pad;
    const float centerY = bar.GetCenter().y;
    if (options.icon.IsValid())
    {
        internal::DrawControlIcon(drawList, options.icon, x, centerY, text.IconSize(), ToU32(t.colors.labelSecondary));
        x += text.IconSize() + Px(t.spacing.sm);
    }
    const char* labelEnd = ImGui::FindRenderedTextEnd(label);
    bool cut = false;
    if (labelEnd != label)
    {
        const internal::FittedText shown = internal::FitText(label, labelEnd, std::max(0.0f, titleRight - x));
        cut = shown.truncated;
        drawList->AddText(ImVec2(x, CenteredTextY(bar.Min.y, bar.GetHeight())), ToU32(t.colors.label), shown.text);
    }
    internal::CutTextTooltip(window->GetID("##title"), free, dragItem.hovered && drag.mode == 0, false, label, labelEnd,
                             cut);
    return closed;
}

} // namespace

bool BeginShell(const char* label, bool* open, const ShellOptions& options)
{
    if (!ShellInFrame())
        return false;
    if (label == nullptr)
        label = "";
    if (open != nullptr && !*open)
        return false;
    ImGuiContext& g = *GImGui;
    const Theme& t = T();
    const ShellMetrics m = GetShellMetrics(options);
    // The state lives under the window's name, apart from Dear ImGui's own settings of it.
    const ImGuiID stateId = ImHashStr("##facet.shell", 0, ImHashStr(label));
    ShellState& state = options.persist ? internal::GetPersistentState<ShellState>(stateId)
                                        : internal::GetState<ShellState>(stateId);
    ShellDrag& drag = internal::GetState<ShellDrag>(stateId);

    // The rectangle: saved units, a drag in progress, the limits and the display.
    ImRect rect;
    if (!state.known || state.size.x <= 0.0f || state.size.y <= 0.0f)
        rect = FirstShellRect(options, m);
    else
        rect = ImRect(ImVec2(m.display.Min.x + state.pos.x * m.scale, m.display.Min.y + state.pos.y * m.scale),
                      ImVec2(m.display.Min.x + (state.pos.x + state.size.x) * m.scale,
                             m.display.Min.y + (state.pos.y + state.size.y) * m.scale));
    if (drag.mode != 0 && (g.ActiveId != drag.item || !ImGui::IsMouseDown(ImGuiMouseButton_Left)))
        drag.mode = 0;  // released, or another item took the mouse
    if (drag.mode != 0)
        rect = DraggedShellRect(drag, g.IO.MousePos, options, m);
    rect = ClampShellRect(rect, options, m);
    StoreShellRect(state, rect, m);
    const bool collapsed = options.collapsible && state.collapsed;
    const ImRect shown = collapsed ? ImRect(rect.Min, ImVec2(rect.Max.x, rect.Min.y + m.titleHeight)) : rect;

    ImGui::SetNextWindowPos(shown.Min, ImGuiCond_Always);
    ImGui::SetNextWindowSize(shown.GetSize(), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(1.0f, 1.0f));
    constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                                        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground |
                                        ImGuiWindowFlags_NoSavedSettings;
    const bool began = ImGui::Begin(label, nullptr, kFlags);
    ImGui::PopStyleVar(3);
    ImGuiWindow* window = g.CurrentWindow;
    if (!began || window == nullptr || window->SkipItems)
    {
        ImGui::End();
        return false;
    }

    const bool glass = options.glass && GlassAvailable();
    DrawShellSurface(window->DrawList, shown, m, glass);
    bool bandHot = false;
    if (options.resizable && !collapsed)
        bandHot = ShellResizeBands(window, shown, m, drag);
    const bool closed = ShellTitleBar(window, label, open, options, shown, m, state, drag);
    if (!collapsed)
    {
        HairlineH(window->DrawList, shown.Min.x, shown.Max.x, shown.Min.y + m.titleHeight - HairlineThickness(),
                  ToU32(t.colors.separator));
        if (options.resizable)
            DrawShellGrip(window->DrawList, shown, m, bandHot || (drag.mode & ~kShellMove) != 0);
    }

    bool closeNow = closed;
    if (open != nullptr && options.closeOnEscape && internal::ClaimEscape(window->ID, internal::EscapeLayer::Shell))
        closeNow = true;
    if (closeNow && open != nullptr)
        *open = false;
    if (collapsed || (state.collapsed && options.collapsible))
    {
        ImGui::End();
        return false;
    }

    // The content: a scroll area under the title bar, inset by the resize bands. Its padding makes
    // up the rest of spacing.lg, and is never narrower than the focus ring, so rings show whole.
    const ImVec2 contentMin(shown.Min.x + m.band, shown.Min.y + m.titleHeight);
    const ImVec2 contentMax(shown.Max.x - m.band, shown.Max.y - m.band);
    ImGui::SetCursorScreenPos(contentMin);
    internal::ScrollAreaDesc desc;
    desc.size = ImVec2(std::max(1.0f, contentMax.x - contentMin.x), std::max(1.0f, contentMax.y - contentMin.y));
    desc.padding = std::max(internal::FocusRingOutset(), Px(t.spacing.lg) - m.band);
    desc.edgeFades = !glass;
    desc.fadeSurface = t.colors.ElevatedSurface(kShellElevation);
    if (!internal::BeginScrollAreaCore("##content", desc))
    {
        ImGui::End();
        return false;
    }
    internal::PushScope("BeginShell");
    ShellStack& stack = Shells();
    if (stack.depth >= kShellDepth)
        ++stack.overflow;
    else
        stack.entries[stack.depth++].window = window;
    return true;
}

void EndShell()
{
    if (!internal::PopScope("BeginShell"))
        return;
    ShellStack& stack = Shells();
    ImGuiWindow* window = nullptr;
    if (stack.overflow > 0)
        --stack.overflow;
    else if (stack.depth > 0)
        window = stack.entries[--stack.depth].window;
    ImGuiContext& g = *GImGui;
    // The content area is a child of the shell window; a missing End inside it is the caller's
    // and Dear ImGui reports it.
    internal::EndScrollAreaCore();
    if (window != nullptr && g.CurrentWindow != window)
        return;
    ImGui::End();
}

namespace internal
{

void ClearShellStack()
{
    ShellStack& stack = Shells();
    stack.depth = 0;
    stack.overflow = 0;
}

} // namespace internal

} // namespace facet
