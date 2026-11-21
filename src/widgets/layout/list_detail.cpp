// BeginListDetail, the pane Begins and their Ends (layout.h): a list pane and a detail pane.
//
// In a Regular or Expanded width the panes sit side by side: the list pane listWidth units wide (at
// most the width less layout.minRowControl and the band), a band of layout.dividerBand with a
// hairline in the separator color, and the detail pane in the rest. In a Compact width only one
// pane shows, at the full size: the list while *detailOpen is false, the detail while it is true.
// The compact detail pane starts with a plain back button (chevron-left and Strings::back) that
// writes *detailOpen = false; when the detail opens while the keyboard focus was in the list, the
// focus moves to that button, so keyboard users are not left on an item that went away.
//
// Each pane is a scroll area of the scroll core: a child window with Facet's overlay bars whose
// keyboard navigation is part of the window around it, and whose content keeps its ids under the
// pane. A pane Begin returns false while its pane is hidden (the Compact rule) and while the pane is
// out of sight; its End is called only after true. The view is one group, the last item after
// EndListDetail. Each BeginListDetail keeps its own entry, so list-details nest in their panes.

#include "facet/widgets/layout.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <imgui.h>
#include <imgui_internal.h>

#include "core/core_internal.h"
#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/layout.h"
#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "facet/widgets/buttons.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/scope_stack.h"
#include "internal/scroll.h"
#include "internal/state_storage.h"
#include "widgets/layout/layout_internal.h"

namespace facet
{
namespace
{

// Whether the detail was open in the last frame, to see it open.
struct ListDetailMemory
{
    bool detailWasOpen = false;
    bool focusInList = false;  // the keyboard focus was in the list pane at its End
};

enum class ListDetailPaneKind
{
    None,
    List,
    Detail,
};

struct ListDetailEntry
{
    bool active = false;
    ImGuiWindow* window = nullptr;
    ImGuiID id = 0;
    bool* detailOpen = nullptr;
    bool compact = false;
    bool detail = false;         // *detailOpen at Begin
    ImVec2 origin = {0.0f, 0.0f};
    ImVec2 size = {0.0f, 0.0f};
    float listWidth = 0.0f;
    float band = 0.0f;
    ListDetailPaneKind open = ListDetailPaneKind::None;
    bool focusBack = false;      // move the keyboard focus to the back button
    bool navSeenBefore = false;  // the focused item was submitted before the open pane began
};

struct ListDetailStack
{
    ListDetailEntry entries[internal::kScopeStackDepth];
    int depth = 0;
    int overflow = 0;
};

ListDetailStack& Views()
{
    static ListDetailStack stack;
    return stack;
}

ListDetailEntry* TopView()
{
    ListDetailStack& stack = Views();
    if (stack.overflow > 0 || stack.depth == 0)
        return nullptr;
    ListDetailEntry& entry = stack.entries[stack.depth - 1];
    return entry.active ? &entry : nullptr;
}

ImRect ListDetailPaneRect(const ListDetailEntry& e, ListDetailPaneKind kind)
{
    if (e.compact)
        return ImRect(e.origin, ImVec2(e.origin.x + e.size.x, e.origin.y + e.size.y));
    if (kind == ListDetailPaneKind::List)
        return ImRect(e.origin, ImVec2(e.origin.x + e.listWidth, e.origin.y + e.size.y));
    return ImRect(ImVec2(e.origin.x + e.listWidth + e.band, e.origin.y), ImVec2(e.origin.x + e.size.x, e.origin.y + e.size.y));
}

bool OpenListDetailPane(ListDetailEntry& e, ListDetailPaneKind kind, const char* name)
{
    if (e.open != ListDetailPaneKind::None)
    {
        internal::ReportUsageError("BeginListPane or BeginDetailPane while the other pane is still open");
        return false;
    }
    const ImRect rect = ListDetailPaneRect(e, kind);
    if (rect.GetWidth() < 1.0f || rect.GetHeight() < 1.0f)
        return false;
    ImGui::SetCursorScreenPos(rect.Min);
    internal::ScrollAreaDesc desc;
    desc.size = rect.GetSize();
    if (!internal::BeginScrollAreaCore(ImHashStr(name, 0, e.id), desc))
        return false;
    e.open = kind;
    e.navSeenBefore = GImGui->NavIdIsAlive;
    return true;
}

bool CloseListDetailPane(ListDetailPaneKind kind)
{
    ListDetailEntry* e = TopView();
    if (e == nullptr || e->open != kind)
    {
        internal::ReportUsageError(kind == ListDetailPaneKind::List ? "EndListPane without BeginListPane"
                                                                    : "EndDetailPane without BeginDetailPane");
        return false;
    }
    if (kind == ListDetailPaneKind::List)
    {
        // Remember whether the keyboard focus is in the list, for the compact detail's back button:
        // Dear ImGui marks the focused item alive when it is submitted.
        internal::GetState<ListDetailMemory>(e->id).focusInList = !e->navSeenBefore && GImGui->NavIdIsAlive;
    }
    internal::EndScrollAreaCore();
    e->open = ListDetailPaneKind::None;
    return true;
}

} // namespace

void BeginListDetail(const char* id, bool* detailOpen, const ListDetailOptions& options)
{
    internal::PushScope("BeginListDetail");
    ListDetailStack& stack = Views();
    if (stack.depth >= internal::kScopeStackDepth)
    {
        ++stack.overflow;
        return;
    }
    ListDetailEntry& e = stack.entries[stack.depth++];
    e = ListDetailEntry{};
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return;
    if (id == nullptr)
        id = "";
    const Theme& theme = T();
    e.active = true;
    e.window = window;
    e.id = window->GetID(id);
    e.detailOpen = detailOpen;
    e.detail = detailOpen != nullptr && *detailOpen;
    const ImVec2 available = ImGui::GetContentRegionAvail();
    e.size.x = std::max(1.0f, available.x);
    e.size.y = options.height > 0.0f ? Px(options.height) : (available.y >= 1.0f ? available.y : Px(theme.metrics.panelHeight));
    e.compact = AvailableWidthClass() == WidthClass::Compact;
    e.band = Px(theme.layout.dividerBand);
    const float wanted = Px(options.listWidth > 0.0f ? options.listWidth : theme.layout.labelColumn + theme.layout.minRowControl);
    e.listWidth = std::max(1.0f, std::min(wanted, e.size.x - e.band - Px(theme.layout.minRowControl)));

    ListDetailMemory& memory = internal::GetState<ListDetailMemory>(e.id);
    e.focusBack = e.compact && e.detail && !memory.detailWasOpen && memory.focusInList && internal::KeyboardModality();
    memory.detailWasOpen = e.detail;

    ImGui::BeginGroup();
    e.origin = window->DC.CursorPos;
}

bool BeginListPane()
{
    ListDetailEntry* e = TopView();
    if (e == nullptr || (e->compact && e->detail))
        return false;
    if (!OpenListDetailPane(*e, ListDetailPaneKind::List, "##list"))
        return false;
    internal::PushScope("BeginListPane");
    return true;
}

void EndListPane()
{
    if (!internal::PopScope("BeginListPane"))
        return;
    CloseListDetailPane(ListDetailPaneKind::List);
}

bool BeginDetailPane()
{
    ListDetailEntry* e = TopView();
    if (e == nullptr || (e->compact && !e->detail))
        return false;
    if (!OpenListDetailPane(*e, ListDetailPaneKind::Detail, "##detail"))
        return false;
    internal::PushScope("BeginDetailPane");
    if (e->compact)
    {
        // The way back to the list.
        if (e->focusBack)
            ImGui::SetKeyboardFocusHere();
        // The id does not depend on the text, so a translated Strings::back keeps it.
        char label[96];
        std::snprintf(label, sizeof(label), "%s###facet.listdetail.back", GetStrings().back);
        if (Button(label, {.variant = ButtonVariant::Plain, .icon = Icon("chevron-left")}) && e->detailOpen != nullptr)
            *e->detailOpen = false;
    }
    return true;
}

void EndDetailPane()
{
    if (!internal::PopScope("BeginDetailPane"))
        return;
    CloseListDetailPane(ListDetailPaneKind::Detail);
}

void EndListDetail()
{
    if (!internal::PopScope("BeginListDetail"))
        return;
    ListDetailStack& stack = Views();
    if (stack.overflow > 0)
    {
        --stack.overflow;
        return;
    }
    if (stack.depth == 0)
        return;
    ListDetailEntry& e = stack.entries[--stack.depth];
    if (!e.active || GImGui->CurrentWindow != e.window)
        return;  // the caller ended a window inside the view; Dear ImGui reports that itself
    if (!e.compact)
    {
        const float x = std::round(e.origin.x + e.listWidth + e.band * 0.5f - HairlineThickness() * 0.5f);
        HairlineV(e.window->DrawList, x, e.origin.y, e.origin.y + e.size.y, ToU32(T().colors.separator));
    }
    ImGui::SetCursorScreenPos(e.origin);
    ImGui::Dummy(e.size);
    ImGui::EndGroup();
}

namespace internal
{

void ClearListDetailStack()
{
    ListDetailStack& stack = Views();
    stack.depth = 0;
    stack.overflow = 0;
}

} // namespace internal

} // namespace facet
