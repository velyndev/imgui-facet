// BeginModal and EndModal (overlays.h): a dialog over a scrim.
//
// The dialog is a Modal level of the popup host (popup_host.h), centered in the display. The host
// draws the scrim and the surface, plays the scale-in and fade-out, keeps Dear ImGui's modal rules
// (the page under it takes no input, keyboard navigation stays inside) and gives the focus back to
// the item that had it when the dialog closes, however it closes. Escape closes the topmost dialog
// only. A modal opened inside a modal's content is a child level and stacks above it.
//
// Layout: the title (Title3) and the close button on one row, then the content in a scroll area
// (overlay_parts.h), as tall as the content until it outgrows the display, then scrolling with the
// overlay bars. The close button is submitted after the content, so Tab reaches the content first
// and the focus that moves in when the dialog opens lands on the first item of the content.

#include "facet/widgets/overlays.h"

#include <algorithm>

#include <imgui.h>
#include <imgui_internal.h>

#include "core/core_internal.h"
#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/item.h"
#include "internal/popup_host.h"
#include "internal/scope_stack.h"
#include "internal/state_storage.h"
#include "widgets/overlays/overlay_parts.h"

namespace facet
{
namespace
{

constexpr int kModalDepth = 8;

struct ModalEntry
{
    ImGuiID level = 0;
    bool* open = nullptr;
    bool closeButton = false;
    bool scrolled = false;
    ImRect titleRow;
};

struct ModalStack
{
    ModalEntry entries[kModalDepth];
    int depth = 0;
};

ModalStack& Stack()
{
    static ModalStack stack;
    return stack;
}

} // namespace

namespace internal
{

void OverlayModalResetStack()
{
    Stack().depth = 0;
}

} // namespace internal

bool BeginModal(const char* label, bool* open, const ModalOptions& options)
{
    using namespace internal;
    ImGuiWindow* window = ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    // A capture shows the dialog open (ForcedItemState::Open) without input; any other pending
    // request is dropped here, so it does not reach the content.
    const ForcedItemRequest forced = TakeForcedItemState();
    const Theme& theme = T();
    const ImGuiID level = window->GetID(label);
    const ImRect work = OverlayWorkArea();

    const float padding = Px(theme.spacing.xl);
    const float width = std::min(Px(options.width > 0.0f ? options.width : theme.overlay.modalWidth), work.GetWidth());
    const float inner = std::max(1.0f, width - padding * 2.0f);
    const bool hasTitle = ImGui::FindRenderedTextEnd(label) != label;
    const float titleHeight = hasTitle || options.closeButton ? OverlayTitleHeight() : 0.0f;
    const float titleGap = titleHeight > 0.0f ? Px(theme.spacing.md) : 0.0f;

    OverlayScrollDesc scroll;
    scroll.width = inner;
    scroll.maxHeight = std::max(Px(theme.metrics.controlHeight), work.GetHeight() - padding * 2.0f - titleHeight - titleGap);
    scroll.surface = theme.colors.ElevatedSurface(3);
    const ImGuiID content = ImHashStr("##facet.modal", 0, level);
    const ImVec2 area = OverlayScrollSize(content, scroll);
    const ImVec2 size(width, area.y + padding * 2.0f + titleHeight + titleGap);

    PopupOptions popup;
    popup.kind = PopupKind::Modal;
    popup.size = size;
    popup.closeOnEscape = options.dismissible;
    const bool forcedOpen = Has(forced.state, ForcedItemState::Open);
    if (forcedOpen)
        ForcePopupLevelOpen(level, {}, popup);
    const OverlayLayerSync sync = OverlaySyncLayer(level, open, {}, popup);
    if (sync.open || forcedOpen)
        SetPopupLevelLayout(level, size, ImRect());
    // A click on the scrim dismisses a dismissible dialog; otherwise it does nothing.
    if (sync.open && options.dismissible && PopupLevelScrimClicked(level))
    {
        ClosePopupLevel(level);
        *open = false;
        return false;
    }
    if (!BeginPopupLevel(level))
        return false;

    ModalStack& stack = Stack();
    if (stack.depth >= kModalDepth)
    {
        ReportUsageError("BeginModal nested deeper than %d", kModalDepth);
        EndPopupLevel();
        return false;
    }
    ImGuiWindow* dialog = GImGui->CurrentWindow;
    const ImVec2 origin = dialog->DC.CursorPos;
    ModalEntry& entry = stack.entries[stack.depth++];
    entry = ModalEntry{};
    entry.level = level;
    entry.open = open;
    entry.closeButton = options.closeButton;
    entry.titleRow = ImRect(origin, ImVec2(origin.x + inner, origin.y + titleHeight));
    if (hasTitle)
    {
        const float closeRoom = options.closeButton ? ControlHeight(ControlSize::Small) + Px(theme.spacing.sm) : 0.0f;
        DrawOverlayTitle(level, label, origin, inner - closeRoom);
    }
    if (titleHeight > 0.0f)
        ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + titleHeight + titleGap));
    entry.scrolled = BeginOverlayScroll(content, scroll);
    PushScope("BeginModal");
    return true;
}

void EndModal()
{
    using namespace internal;
    if (!PopScope("BeginModal"))
        return;
    ModalStack& stack = Stack();
    if (stack.depth <= 0)
        return;
    const ModalEntry entry = stack.entries[--stack.depth];
    if (entry.scrolled)
        EndOverlayScroll();
    if (entry.closeButton && OverlayCloseButton(entry.level, entry.titleRow.Max.x, entry.titleRow.GetCenter().y))
    {
        ClosePopupLevel(entry.level);
        if (entry.open != nullptr)
            *entry.open = false;
    }
    EndPopupLevel();
}

} // namespace facet
