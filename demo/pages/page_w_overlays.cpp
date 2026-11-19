// Demo page: Overlays. Calls every v1 widget of facet/widgets/overlays.h once, grouped by how
// the layer opens. Layers the caller opens start closed, and a checkbox per layer sets its open
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

// A labelled checkbox for a layer's open flag. In capture runs the flag follows the capture state,
// so each layer state shows that layer alone; the checkbox sits right before the layer call and is
// the popover's anchor.
void LayerCheckbox(facet::demo::PageCtx& ctx, const char* label, const char* id, const char* probe, bool* open,
                 int captureState)
{
    if (ctx.capture)
        *open = ctx.captureState == captureState;
    LabeledRow(label);
    ImGui::Checkbox(id, open);
    facet::ItemCursor();
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
    Paragraph("Each of these takes the caller's open flag; the checkbox beside its name sets it, and dismissing "
              "the finished layer clears it. A planned layer shows one placeholder line while its "
              "flag is on.");
    LayerCheckbox(ctx, "Modal", "##modal", "overlays.modal", &s.modal, kCaptureModal);
    if (facet::BeginModal("Modal", &s.modal))
    {
        Note("Modal content.");
        facet::EndModal();
    }
    RevealLayer(ctx, kCaptureModal);
    LayerCheckbox(ctx, "Confirm dialog", "##confirm", "overlays.confirm", &s.confirm, kCaptureConfirm);
    facet::ConfirmDialog("Confirm", &s.confirm, "Message of the dialog.", {.destructive = true});
    RevealLayer(ctx, kCaptureConfirm);
    LayerCheckbox(ctx, "Popover", "##popover", "overlays.popover", &s.popover, kCapturePopover);
    if (facet::BeginPopover("popover", &s.popover, {.placement = facet::PopoverPlacement::Below}))
    {
        Note("Popover content.");
        facet::EndPopover();
    }
    RevealLayer(ctx, kCapturePopover);
    LayerCheckbox(ctx, "Drawer", "##drawer", "overlays.drawer", &s.drawer, kCaptureDrawer);
    if (facet::BeginDrawer("Drawer", &s.drawer, {.side = facet::DrawerSide::End}))
    {
        Note("Drawer content.");
        facet::EndDrawer();
    }
    RevealLayer(ctx, kCaptureDrawer);
    LayerCheckbox(ctx, "Command palette", "##palette", "overlays.palette", &s.palette, kCapturePalette);
    facet::CommandPalette("Commands", &s.palette, &s.picked, kCommands);
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

void Notifications(facet::demo::PageCtx& ctx, OverlaysState& s)
{
    SectionHeader("Notifications and status");
    LabeledRow("Toast");
    if (ImGui::Button("Show a toast"))
    {
        s.toast = facet::ShowToast("Toast text", {.tone = facet::Tone::Success});
        s.toastShown = true;
    }
    facet::ItemCursor();
    ctx.Probe("overlays.toast");
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
                            "Modals, confirm dialogs, popovers, context menus, tooltips, toasts, drawers, loading "
                            "overlays, the command palette, watermark overlays and the unsaved changes bar.");
    facet::demo::PlannedFamilyNote("overlays");
    OverlaysState& s = State();
    CallerOpened(ctx, s);
    InputOpened();
    Notifications(ctx, s);
}

// Every caller-opened layer starts closed, so finished layers never open over the gallery by
// themselves, and each checkbox opens and closes its own layer only.
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
        // By keyboard: after the clicks the first Tab shows the focus on the checkbox, and
        // Space flips it.
        test.KeyPress(ImGuiKey_Tab);
        test.ExpectFocused(layer.probe);
        test.KeyPress(ImGuiKey_Space);
        test.Expect(std::string(layer.probe) + " opens with Space", [flag = layer.flag] { return State().*flag; });
        test.KeyPress(ImGuiKey_Space);
        test.Expect(std::string(layer.probe) + " closes with Space", [] { return !State().AnyLayerOpen(); });
    }

    // The toast button, by mouse and by keyboard.
    test.Do([] { State().toastShown = false; });
    test.Click("overlays.toast");
    test.Expect("the toast button calls ShowToast", [] { return State().toastShown; });
    test.Do([] { State().toastShown = false; });
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("overlays.toast");
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Space on the focused toast button calls it too", [] { return State().toastShown; });
}
