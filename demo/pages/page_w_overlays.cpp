// Demo page: Overlays. Calls every v1 widget of facet/widgets/overlays.h once, grouped by how
// the layer opens. Layers the caller opens start closed, and a switch per layer sets its open
// flag, so a finished modal or drawer never opens over the gallery by itself and never stacks on
// another one; each has a capture state that shows it alone. While the widgets are planned, an
// open layer shows one placeholder line in the page flow; layers opened by input (context menus,
// tooltips) and the toast stack show nothing yet, which the notes say. Phase 2 replaces the
// placeholders one by one on this page.

#include <string>

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

// Capture states of the page, in the order of the FACET_DEMO_PAGE registration: the default
// state with every layer closed, then one state per caller-opened layer.
enum CaptureState
{
    kCaptureDefault = 0,
    kCaptureModal = 1,
    kCaptureConfirm = 2,
    kCapturePopover = 3,
    kCaptureDrawer = 4,
    kCapturePalette = 5,
};

struct OverlaysState
{
    bool modal = false;
    bool confirm = false;
    bool popover = false;
    bool drawer = false;
    bool palette = false;
    int picked = -1;
    bool toastShown = false;
    facet::ToastId toast;
    int changes = 2;

    bool AnyLayerOpen() const { return modal || confirm || popover || drawer || palette; }
};

OverlaysState& State()
{
    static OverlaysState state;
    return state;
}

// A labelled switch for a layer's open flag. In capture runs the flag follows the capture state,
// so each layer state shows that layer alone; the switch sits right before the layer call and is
// the popover's anchor.
void LayerSwitch(facet::demo::PageCtx& ctx, const char* label, const char* id, const char* probe, bool* open,
                 int captureState)
{
    if (ctx.capture)
        *open = ctx.captureState == captureState;
    LabeledRow(label);
    ImGui::Checkbox(id, open);
    ctx.Probe(probe);
}

void RevealLayer(const facet::demo::PageCtx& ctx, int captureState)
{
    if (ctx.capture && ctx.captureState == captureState)
        ctx.RevealInCapture();
}

void CallerOpened(facet::demo::PageCtx& ctx, OverlaysState& s)
{
    SectionHeader("Layers the caller opens");
    Paragraph("Each of these takes the caller's open flag; the switches set it, and dismissing "
              "the finished layer clears it. A planned layer shows one placeholder line while its "
              "flag is on.");
    LayerSwitch(ctx, "Show the modal", "##modal", "overlays.modal", &s.modal, kCaptureModal);
    if (facet::BeginModal("Modal", &s.modal))
    {
        Note("Modal content.");
        facet::EndModal();
    }
    RevealLayer(ctx, kCaptureModal);
    LayerSwitch(ctx, "Show the confirm dialog", "##confirm", "overlays.confirm", &s.confirm, kCaptureConfirm);
    facet::ConfirmDialog("Confirm", &s.confirm, "Message of the dialog.", {.destructive = true});
    RevealLayer(ctx, kCaptureConfirm);
    LayerSwitch(ctx, "Show the popover", "##popover", "overlays.popover", &s.popover, kCapturePopover);
    if (facet::BeginPopover("popover", &s.popover, {.placement = facet::PopoverPlacement::Below}))
    {
        Note("Popover content.");
        facet::EndPopover();
    }
    RevealLayer(ctx, kCapturePopover);
    LayerSwitch(ctx, "Show the drawer", "##drawer", "overlays.drawer", &s.drawer, kCaptureDrawer);
    if (facet::BeginDrawer("Drawer", &s.drawer, {.side = facet::DrawerSide::End}))
    {
        Note("Drawer content.");
        facet::EndDrawer();
    }
    RevealLayer(ctx, kCaptureDrawer);
    LayerSwitch(ctx, "Show the command palette", "##palette", "overlays.palette", &s.palette, kCapturePalette);
    facet::CommandPalette("Commands", &s.palette, kCommands, &s.picked);
    RevealLayer(ctx, kCapturePalette);
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

FACET_DEMO_PAGE("overlays", "Overlays", "Widgets", 100, "default", "modal", "confirm", "popover", "drawer",
                "palette")
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
    CallerOpened(ctx, s);
    InputOpened();
    Notifications(s);
}

// Every caller-opened layer starts closed, so finished layers never open over the gallery by
// themselves, and each switch opens and closes its own layer only.
FACET_DEMO_SELFTEST("overlays")
{
    test.Expect("every layer starts closed", [] { return !OverlaysState{}.AnyLayerOpen(); });
    struct Layer
    {
        const char* probe;
        bool OverlaysState::*flag;
    };
    static const Layer kLayers[] = {
        {"overlays.modal", &OverlaysState::modal},     {"overlays.confirm", &OverlaysState::confirm},
        {"overlays.popover", &OverlaysState::popover}, {"overlays.drawer", &OverlaysState::drawer},
        {"overlays.palette", &OverlaysState::palette},
    };
    for (const Layer& layer : kLayers)
    {
        // Every attempt starts from the same state: all layers closed.
        test.Do([] {
            OverlaysState& s = State();
            s.modal = s.confirm = s.popover = s.drawer = s.palette = false;
        });
        test.Click(layer.probe);
        test.Expect(std::string(layer.probe) + " opens its layer alone", [flag = layer.flag] {
            OverlaysState copy = State();
            const bool opened = copy.*flag;
            copy.*flag = false;
            return opened && !copy.AnyLayerOpen();
        });
        test.Click(layer.probe);
        test.Expect(std::string(layer.probe) + " closes its layer", [] { return !State().AnyLayerOpen(); });
    }
}
