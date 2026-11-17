// Demo page: Overlays. Calls every v1 widget of facet/widgets/overlays.h once, grouped by how
// the layer opens. The widgets are planned: layers the caller opens are open on this page, so
// their placeholder line shows in the page flow; layers opened by input (context menus,
// tooltips) and the toast stack show nothing yet, which the notes say. Phase 2 replaces the
// placeholders one by one on this page.

#include <imgui.h>

#include <facet/facet.h>

#include "gallery.h"
#include "pages/pages.h"

namespace
{

using facet::demo::LabeledRow;
using facet::demo::Note;
using facet::demo::Paragraph;
using facet::demo::SectionHeader;

const facet::ChoiceItem kCommands[] = {
    {.label = "Command 1", .icon = facet::Icon("plus"), .detail = "Ctrl+N"},
    {.label = "Command 2", .icon = facet::Icon("search")},
};

const facet::WatermarkField kFields[] = {{.label = "Field 1", .value = "Value 1"},
                                         {.label = "Field 2", .value = "Value 2"}};

struct OverlaysState
{
    bool modal = true;
    bool confirm = true;
    bool popover = true;
    bool drawer = true;
    bool palette = true;
    int picked = -1;
    bool toastShown = false;
    facet::ToastId toast;
    int changes = 2;
};

OverlaysState& State()
{
    static OverlaysState state;
    return state;
}

void CallerOpened(OverlaysState& s)
{
    SectionHeader("Layers the caller opens");
    Paragraph("Each of these takes the caller's open flag, which is set on this page.");
    if (facet::BeginModal("Modal", &s.modal))
    {
        Note("Modal content.");
        facet::EndModal();
    }
    facet::ConfirmDialog("Confirm", &s.confirm, "Message of the dialog.", {.destructive = true});
    if (facet::BeginPopover("popover", &s.popover, {.placement = facet::PopoverPlacement::Below}))
    {
        Note("Popover content.");
        facet::EndPopover();
    }
    if (facet::BeginDrawer("Drawer", &s.drawer, {.side = facet::DrawerSide::End}))
    {
        Note("Drawer content.");
        facet::EndDrawer();
    }
    facet::CommandPalette("Commands", &s.palette, kCommands, &s.picked);
}

void InputOpened()
{
    SectionHeader("Layers opened by input");
    Note("A context menu opens on right click or the Menu key over its item, and tooltips show "
         "on hover and keyboard focus. Both are planned and show nothing yet.");
    ImGui::TextUnformatted("Item with a context menu and a tooltip");
    if (facet::BeginContextMenu("context"))
    {
        facet::MenuItem("Item 1");
        facet::EndContextMenu();
    }
    facet::ItemTooltip("Tooltip text");
    if (facet::BeginRichTooltip())
    {
        Note("Rich tooltip content.");
        facet::EndRichTooltip();
    }
}

void Notifications(OverlaysState& s)
{
    SectionHeader("Notifications and status");
    LabeledRow("Toast");
    if (ImGui::Button("Show a toast"))
    {
        s.toast = facet::ShowToast("Toast text", {.tone = facet::Tone::Success});
        s.toastShown = true;
    }
    if (s.toastShown)
        Note("ShowToast returned %s.", s.toast.IsValid() ? "a toast id" : "an invalid id: the toast stack is planned");
    facet::BeginLoadingOverlay("loading", {.message = "Loading", .loading = true});
    Note("Content under a loading overlay.");
    facet::EndLoadingOverlay();
    facet::WatermarkOverlay("watermark", kFields);
    facet::UnsavedChangesBar("changes", s.changes);
}

} // namespace

FACET_DEMO_PAGE("overlays", "Overlays", "Widgets", 100)
{
    facet::demo::PageHeader("Overlays",
                            "Modals, confirm dialogs, popovers, context menus, tooltips, toasts, "
                            "drawers, loading overlays, the command palette, watermark overlays and "
                            "the unsaved changes bar. Every widget on this page is a placeholder "
                            "until Phase 2 implements it: it shows its name and the word planned, "
                            "and does not react to input.");
    Note("Declared in facet/widgets/overlays.h; the declarations and usage examples are in "
         "docs/widgets/overlays.md.");
    OverlaysState& s = State();
    CallerOpened(s);
    InputOpened();
    Notifications(s);
}
