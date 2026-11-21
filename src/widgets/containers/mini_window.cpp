// BeginMiniWindow and EndMiniWindow (containers.h): a compact panel for an overlay host.
//
// The panel is a top-level window that sizes itself to its content (or to `width`), on the
// opaque elevated surface of elevation 1 with its shadow and the card radius: it floats, but
// below the menu's shell. While the overlay menu shows (overlay_controller.cpp) it has a title row
// with the icon, the title and the pin button, and the title row drags it; while the menu is hidden
// it shows only when pinned, then without the title row and, with clickThrough, without taking any
// input, so the game or app behind it keeps the mouse and keyboard. Its position is kept in design
// units under its name (saved in the ini file with persist) and clamped into the display.

#include "facet/widgets/containers.h"

#include <algorithm>
#include <cfloat>
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
#include "internal/state_storage.h"
#include "widgets/buttons/button_look.h"
#include "widgets/containers/container_layout.h"

namespace facet
{
namespace
{

// The shadow and surface level of a mini window: it floats over the scene, below the shell.
constexpr int kMiniElevation = 1;
constexpr int kMiniDepth = 4;

struct MiniWindowState
{
    static constexpr const char* kStateName = "MiniWindow";
    ImVec2 pos = {0.0f, 0.0f};  // units from the display's work area origin
    bool known = false;

    void WriteIni(ImGuiTextBuffer& out) const
    {
        if (known)
            out.appendf("pos=%.2f,%.2f\n", pos.x, pos.y);
    }
    void ReadIni(const char* key, const char* value)
    {
        if (std::strcmp(key, "pos") != 0)
            return;
        char* end = nullptr;
        const float x = std::strtof(value, &end);
        if (end == value || *end != ',')
            return;
        const char* second = end + 1;
        const float y = std::strtof(second, &end);
        if (end == second || !std::isfinite(x) || !std::isfinite(y))
            return;
        pos = ImVec2(x, y);
        known = true;
    }
};

struct MiniWindowDrag
{
    bool active = false;
    ImGuiID item = 0;
    ImVec2 startMouse = {0.0f, 0.0f};
    ImVec2 startPos = {0.0f, 0.0f};  // pixels
};

struct MiniWindowEntry
{
    ImGuiWindow* window = nullptr;
    bool* pinned = nullptr;   // the pin button goes in at the End
    ImRect pinSlot;           // where the title row kept room for it
};

struct MiniWindowStack
{
    MiniWindowEntry entries[kMiniDepth];
    int depth = 0;
    int overflow = 0;
};

MiniWindowStack& MiniWindows()
{
    static MiniWindowStack stack;
    return stack;
}

bool MiniInFrame()
{
    const ImGuiContext* g = ImGui::GetCurrentContext();
    return IsInitialized() && g != nullptr && g->WithinFrameScope;
}

ImRect MiniDisplay()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    return ImRect(viewport->WorkPos,
                  ImVec2(viewport->WorkPos.x + viewport->WorkSize.x, viewport->WorkPos.y + viewport->WorkSize.y));
}

// The pin button: a small square plain button with the pin or unpin icon at ect.
bool MiniPinButton(ImGuiWindow* window, bool pinned, const ImRect& rect)
{
    const internal::ControlMetrics m = internal::GetControlMetrics(ControlSize::Small);
    const ImGuiID id = window->GetID("##pin");
    const internal::ItemState item = internal::PartBehavior(id, rect);
    if (item.visible)
    {
        internal::ItemState look = item;
        const internal::ButtonColors colors = internal::ButtonLook(ButtonVariant::Plain, look);
        if (colors.fill.w > 0.0f)
            FillRect(window->DrawList, item.rect.Min, item.rect.Max, ToU32(colors.fill), m.radius);
        const internal::ControlTextScope text(m);
        const ImU32 content = ToU32(pinned && !item.disabled ? T().colors.accentText : colors.content);
        internal::DrawControlIcon(window->DrawList, Icon(pinned ? "unpin" : "pin"),
                                  std::round(item.rect.GetCenter().x - text.IconSize() * 0.5f), item.rect.GetCenter().y,
                                  text.IconSize(), content);
        internal::DrawItemFocusRing(item, m.radius);
    }
    if (internal::BeginPopupTooltip(item.id, item.rect, item.hovered, item.focusVisible))
    {
        ImGui::TextUnformatted(pinned ? GetStrings().unpin : GetStrings().pin);
        internal::EndPopupTooltip();
    }
    return item.pressed;
}

} // namespace

bool BeginMiniWindow(const char* label, bool* pinned, const MiniWindowOptions& options)
{
    if (!MiniInFrame())
        return false;
    if (label == nullptr)
        label = "";
    ImGuiContext& g = *GImGui;
    const Theme& t = T();
    const bool isPinned = pinned != nullptr && *pinned;
    const bool menu = internal::OverlayMenuShown();
    if (!menu && !isPinned)
        return false;
    const bool passThrough = !menu && options.clickThrough;
    const ImGuiID stateId = ImHashStr("##facet.mini", 0, ImHashStr(label));
    MiniWindowState& state = options.persist ? internal::GetPersistentState<MiniWindowState>(stateId)
                                             : internal::GetState<MiniWindowState>(stateId);
    MiniWindowDrag& drag = internal::GetState<MiniWindowDrag>(stateId);
    const float scale = Px(1.0f);
    const ImRect display = MiniDisplay();

    // The position: saved units, a drag in progress, then the display.
    ImVec2 pos = state.known ? ImVec2(display.Min.x + state.pos.x * scale, display.Min.y + state.pos.y * scale)
                             : ImVec2(display.Min.x + Px(t.overlay.viewportMargin), display.Min.y + Px(t.overlay.viewportMargin));
    if (drag.active && (g.ActiveId != drag.item || !ImGui::IsMouseDown(ImGuiMouseButton_Left) || !menu))
        drag.active = false;
    if (drag.active)
        pos = ImVec2(drag.startPos.x + g.IO.MousePos.x - drag.startMouse.x, drag.startPos.y + g.IO.MousePos.y - drag.startMouse.y);
    ImGuiWindow* last = ImGui::FindWindowByName(label);
    const ImVec2 size = last != nullptr ? last->Size : ImVec2(0.0f, 0.0f);
    pos.x = std::clamp(pos.x, display.Min.x, std::max(display.Min.x, display.Max.x - size.x));
    pos.y = std::clamp(pos.y, display.Min.y, std::max(display.Min.y, display.Max.y - size.y));
    pos = ImVec2(std::floor(pos.x), std::floor(pos.y));
    const ImVec2 units((pos.x - display.Min.x) / scale, (pos.y - display.Min.y) / scale);
    if (!state.known || std::fabs(units.x - state.pos.x) > 0.01f || std::fabs(units.y - state.pos.y) > 0.01f)
    {
        state.pos = units;
        state.known = true;
        internal::MarkStateDirty();
    }

    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    if (options.width > 0.0f)
        ImGui::SetNextWindowSizeConstraints(ImVec2(Px(options.width), 0.0f), ImVec2(Px(options.width), FLT_MAX));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Px(t.spacing.md), Px(t.spacing.md)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(1.0f, 1.0f));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground |
                             ImGuiWindowFlags_NoFocusOnAppearing;
    if (passThrough)
        flags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav;
    const bool began = ImGui::Begin(label, nullptr, flags);
    ImGui::PopStyleVar(3);
    ImGuiWindow* window = g.CurrentWindow;
    if (!began || window == nullptr || window->SkipItems)
    {
        ImGui::End();
        return false;
    }

    // The surface, its shadow outside the window.
    const ImRect r = window->Rect();
    const float radius = Px(t.radii.card);
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::PushClipRect(viewport->Pos, ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y), false);
    DrawElevation(window->DrawList, r.Min, r.Max, radius, kMiniElevation);
    FillRect(window->DrawList, r.Min, r.Max, ToU32(t.colors.ElevatedSurface(kMiniElevation)), radius);
    StrokeRect(window->DrawList, r.Min, r.Max, ToU32(t.colors.separator), radius, HairlineThickness());
    ImGui::PopClipRect();

    ImRect pinSlot;
    if (menu)
    {
        // The title row: icon and title (the drag region), then the pin button.
        const internal::ControlTextScope text(TextStyle::Headline);
        const float side = ControlHeight(ControlSize::Small);
        const char* labelEnd = ImGui::FindRenderedTextEnd(label);
        const float iconWidth = options.icon.IsValid() ? text.IconSize() + Px(t.spacing.sm) : 0.0f;
        float titleWidth = iconWidth + ImGui::CalcTextSize(label, labelEnd).x;
        if (options.width > 0.0f)
        {
            const float room = Px(options.width) - Px(t.spacing.md) * 2.0f - (pinned != nullptr ? side + Px(t.spacing.sm) : 0.0f);
            titleWidth = std::max(1.0f, room);
        }
        internal::ItemOptions dragOptions;
        dragOptions.press = internal::PressMode::Click;
        dragOptions.navigable = false;
        dragOptions.tabStop = false;
        dragOptions.hitTarget = false;
        dragOptions.handCursor = false;
        const float top = window->DC.CursorPos.y;
        dragOptions.baseline = CenteredTextY(top, side) - top;
        const internal::ItemState title = internal::ItemBehavior(window->GetID("##title"), ImVec2(std::ceil(titleWidth), side),
                                                                 dragOptions);
        if (title.activated)
        {
            drag.active = true;
            drag.item = title.id;
            drag.startMouse = g.IO.MousePos;
            drag.startPos = window->Pos;
        }
        bool cut = false;
        if (title.visible)
        {
            float x = title.rect.Min.x;
            if (options.icon.IsValid())
            {
                internal::DrawControlIcon(window->DrawList, options.icon, x, title.rect.GetCenter().y, text.IconSize(),
                                          ToU32(internal::ControlSecondaryColor(title.disabled)));
                x += iconWidth;
            }
            const internal::FittedText shown = internal::FitText(label, labelEnd, std::max(0.0f, title.rect.Max.x - x));
            cut = shown.truncated;
            window->DrawList->AddText(ImVec2(x, CenteredTextY(title.rect.Min.y, side)),
                                      ToU32(internal::ControlLabelColor(title.disabled)), shown.text);
        }
        internal::CutTextTooltip(title, label, labelEnd, cut);
        if (pinned != nullptr)
        {
            // Room for the pin button; EndMiniWindow puts it at the trailing edge of the content.
            ImGui::SameLine(0.0f, Px(t.spacing.sm));
            ImGui::Dummy(ImVec2(side, side));
            pinSlot = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        }
    }
    if (!passThrough)
        internal::AddOverlayInputRegion(window);

    internal::PushScope("BeginMiniWindow");
    MiniWindowStack& stack = MiniWindows();
    if (stack.depth >= kMiniDepth)
        ++stack.overflow;
    else
        stack.entries[stack.depth++] = {window, menu ? pinned : nullptr, pinSlot};
    return true;
}

void EndMiniWindow()
{
    if (!internal::PopScope("BeginMiniWindow"))
        return;
    MiniWindowStack& stack = MiniWindows();
    MiniWindowEntry entry;
    if (stack.overflow > 0)
        --stack.overflow;
    else if (stack.depth > 0)
        entry = stack.entries[--stack.depth];
    ImGuiWindow* window = entry.window;
    if (window != nullptr && GImGui->CurrentWindow != window)
        return;  // the caller left a window open inside; Dear ImGui reports that itself
    if (window != nullptr && entry.pinned != nullptr)
    {
        // At the trailing edge of the content measured this frame, so the panel can also shrink.
        const float side = entry.pinSlot.GetWidth();
        const float x = std::max(entry.pinSlot.Min.x, window->DC.CursorMaxPos.x - side);
        const ImRect rect(ImVec2(x, entry.pinSlot.Min.y), ImVec2(x + side, entry.pinSlot.Max.y));
        if (MiniPinButton(window, *entry.pinned, rect))
            *entry.pinned = !*entry.pinned;
    }
    ImGui::End();
}

namespace internal
{

void ClearMiniWindowStack()
{
    MiniWindowStack& stack = MiniWindows();
    stack.depth = 0;
    stack.overflow = 0;
}

} // namespace internal

} // namespace facet
