// Demo page: Overlays. One section per widget of facet/widgets/overlays.h with its important
// variants, capture states for the open looks, and a selftest that operates every widget by mouse and
// keyboard. The page settings are Facet controls. Layers the caller opens start closed and open from a
// button, so none opens over the gallery by itself; in capture runs each state opens its layer alone.
// Sample content is neutral ("Command 1", "Option A"); the watermark fields say they are placeholders.
//
// The context menu rows are facet::MenuItem, a widget of the navigation family: where it is still a
// placeholder, the rows show its placeholder and do nothing, and the selftest checks only the menu.

#include <cstdio>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "gallery.h"
#include "internal/popup_host.h"
#include "pages/pages.h"
#include "widgets/overlays/overlay_parts.h"

namespace
{

using facet::demo::DemoWidth;
using facet::demo::ForcedItemState;
using facet::demo::LabeledRow;
using facet::demo::Note;
using facet::demo::Paragraph;
using facet::demo::ProbeRect;
using facet::demo::SectionHeader;

// The capture states, in the order of the FACET_DEMO_PAGE line below.
enum CaptureState
{
    kDefault,
    kModal,
    kConfirm,
    kPopover,
    kContext,
    kTooltip,
    kDrawer,
    kLoading,
    kPalette,
    kToasts,
};

const char* const kModalLabel = "Dialog";
const char* const kNestedLabel = "Delete the item?";
const char* const kConfirmLabel = "Apply the changes?";
const char* const kDrawerLabel = "Panel";
const char* const kPaletteLabel = "Commands";
const char* const kBarId = "changes";

const facet::ChoiceItem kCommands[] = {
    {.label = "Command 1", .icon = facet::Icon("plus"), .detail = "Detail", .group = "Group 1"},
    {.label = "Command 2", .icon = facet::Icon("folder"), .group = "Group 1"},
    {.label = "Command 3", .icon = facet::Icon("search"), .group = "Group 1"},
    {.label = "Command 4 (unavailable)", .icon = facet::Icon("trash"), .group = "Group 2", .disabled = true},
    {.label = "Command 5", .icon = facet::Icon("settings"), .group = "Group 2"},
    {.label = "Command 6", .icon = facet::Icon("layers"), .group = "Group 2"},
};

const facet::WatermarkField kFields[] = {
    {.label = "Field 1", .value = "Placeholder", .icon = facet::Icon("info")},
    {.label = "Field 2", .value = "Placeholder"},
};

const char* const kPlacements[] = {"Auto", "Below", "Above", "Start", "End"};
const char* const kSides[] = {"End", "Start", "Bottom", "Top"};
// In the order of ScreenCorner.
const char* const kCorners[] = {"Bottom right", "Bottom left", "Top right", "Top left"};

struct OverlaysState
{
    // Dialogs
    bool modal = false;
    bool nested = false;
    bool modalOption = true;
    std::string modalText;
    bool confirm = false;
    bool destructive = false;
    bool hold = false;
    facet::DialogResult lastResult = facet::DialogResult::None;
    // Popover
    bool popover = false;
    int placement = 0;
    bool arrow = true;
    bool dismissible = true;
    bool popoverOption = false;
    float popoverValue = 0.5f;
    bool hoverPopover = false;
    int outsidePresses = 0;
    // Context menu
    bool contextShown = false;
    // Tooltips
    bool tooltipShown = false;
    bool richShown = false;
    bool disabledTipShown = false;
    bool interactiveShown = false;
    int tooltipClicks = 0;
    // Toasts
    facet::ToastId toast;
    facet::ToastId actionToast;
    facet::ToastId progressToast;
    float progress = 0.3f;
    int corner = 0;
    int actions = 0;
    bool captureToasts = false;
    // Drawer
    bool drawer = false;
    int side = 0;
    bool drawerModal = true;
    bool resizable = false;
    bool drawerDismissible = true;
    bool drawerOption = false;
    float extent = 0.0f;
    // Loading overlay
    bool loading = true;
    bool showProgress = false;
    float loadProgress = 0.4f;
    int loadingPresses = 0;
    std::string loadingText;
    // Command palette
    bool palette = false;
    int picked = -1;
    int picks = 0;
    bool shortcutPalette = false;
    facet::ShortcutId pickedShortcut;
    // Watermark
    bool watermark = true;
    int watermarkCorner = 1;
    // Unsaved changes bar
    int changes = 2;
    int applies = 0;
    int reverts = 0;

    bool AnyLayerOpen() const { return modal || nested || confirm || popover || drawer || palette || shortcutPalette; }
};


// A probe of a part of a floating layer (a toast, a drawer's close button, the watermark), which can
// lie outside the page's visible area: recorded against the display, not the page's clip rect.
void FloatingProbe(facet::demo::PageCtx& ctx, const char* name, const ImRect& rect)
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::PushClipRect(viewport->Pos, ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y),
                        false);
    ProbeRect(ctx, name, ImGui::GetID(name), rect.Min, rect.Max);
    ImGui::PopClipRect();
}

OverlaysState& State()
{
    static OverlaysState state;
    return state;
}

facet::ShortcutId PaletteShortcut()
{
    static const facet::ShortcutId id =
        facet::RegisterShortcut("demo.overlays.palette", ImGuiMod_Ctrl | ImGuiKey_K,
                                {.label = "Open the command palette", .group = "Overlays page"});
    return id;
}

const char* ResultText(facet::DialogResult result)
{
    switch (result)
    {
    case facet::DialogResult::Confirmed:
        return "Confirmed";
    case facet::DialogResult::Canceled:
        return "Canceled";
    case facet::DialogResult::None:
        break;
    }
    return "None yet";
}

// In capture runs each state opens its own layers only.
void FollowCapture(const facet::demo::PageCtx& ctx, OverlaysState& s)
{
    if (!ctx.capture)
        return;
    const int state = ctx.captureState;
    s.modal = state == kModal || state == kConfirm;
    s.nested = state == kConfirm;
    s.confirm = false;
    s.popover = state == kPopover;
    s.drawer = state == kDrawer;
    s.palette = state == kPalette;
    s.shortcutPalette = false;
    s.loading = state == kDefault || state == kLoading;
    s.showProgress = state == kLoading;
    // The bar covers the bottom of the page: the default state shows it, the others do not.
    s.changes = state == kDefault ? 2 : 0;
}

void DialogsSection(facet::demo::PageCtx& ctx, OverlaysState& s)
{
    SectionHeader("Modal and confirm dialog");
    Paragraph("A modal dialog over a scrim keeps the keyboard focus inside and gives it back to the button that "
              "opened it. A confirm dialog opened from inside it stacks above it; Escape closes one layer per "
              "press.");
    LabeledRow("Modal");
    if (facet::Button("Open dialog"))
        s.modal = true;
    ctx.Probe("overlays.modal.open");
    if (facet::BeginModal(kModalLabel, &s.modal))
    {
        Paragraph("Placeholder content of the dialog. Tab moves between its controls and stays inside.");
        facet::Toggle("Option A", &s.modalOption);
        ctx.Probe("overlays.modal.toggle");
        facet::TextField("Name##modal", &s.modalText, {.placeholder = "Placeholder"});
        ctx.Probe("overlays.modal.field");
        if (facet::Button("Delete item", {.variant = facet::ButtonVariant::Destructive}))
            s.nested = true;
        ctx.Probe("overlays.modal.delete");
        const facet::DialogResult nested = facet::ConfirmDialog(kNestedLabel, &s.nested, "The item cannot be restored.",
                                                                {.confirmLabel = "Delete", .destructive = true});
        if (nested != facet::DialogResult::None)
            s.lastResult = nested;
        ImRect cancel;
        ImRect confirm;
        if (facet::internal::OverlayConfirmButtonRects(ImGui::GetID(kNestedLabel), &cancel, &confirm))
        {
            FloatingProbe(ctx, "overlays.nested.cancel", cancel);
            FloatingProbe(ctx, "overlays.nested.confirm", confirm);
        }
        facet::EndModal();
    }
    ImRect close;
    if (facet::internal::OverlayCloseButtonRect(ImGui::GetID(kModalLabel), &close))
        FloatingProbe(ctx, "overlays.modal.close", close);
    if (ctx.capture && (ctx.captureState == kModal || ctx.captureState == kConfirm))
        ctx.RevealInCapture();

    LabeledRow("Confirm dialog");
    if (facet::Button("Apply changes"))
        s.confirm = true;
    ctx.Probe("overlays.confirm.open");
    LabeledRow("Destructive");
    facet::Toggle("##destructive", &s.destructive);
    ctx.Probe("overlays.confirm.destructive");
    LabeledRow("Hold to confirm");
    facet::Toggle("##hold", &s.hold);
    ctx.Probe("overlays.confirm.hold");
    const facet::DialogResult result =
        facet::ConfirmDialog(kConfirmLabel, &s.confirm, "The changes take effect at once.",
                             {.confirmLabel = "Apply", .destructive = s.destructive, .holdToConfirm = s.hold});
    if (result != facet::DialogResult::None)
        s.lastResult = result;
    ImRect cancel;
    ImRect confirm;
    if (facet::internal::OverlayConfirmButtonRects(ImGui::GetID(kConfirmLabel), &cancel, &confirm))
    {
        FloatingProbe(ctx, "overlays.confirm.cancel", cancel);
        FloatingProbe(ctx, "overlays.confirm.confirm", confirm);
    }
    Note("Last result: %s.", ResultText(s.lastResult));
}

void PopoverSection(facet::demo::PageCtx& ctx, OverlaysState& s)
{
    SectionHeader("Popover");
    Paragraph("A card next to the button that opens it. The page stays usable around it: a click outside closes "
              "it and still reaches what was clicked.");
    LabeledRow("Placement");
    ImGui::SetNextItemWidth(DemoWidth(320.0f));
    facet::SegmentedControl("##placement", &s.placement, kPlacements, {.width = facet::kFill});
    ctx.Probe("overlays.popover.placement");
    LabeledRow("Arrow");
    facet::Toggle("##arrow", &s.arrow);
    LabeledRow("Dismissible");
    facet::Toggle("##dismissible", &s.dismissible);
    ctx.Probe("overlays.popover.dismissible");
    LabeledRow("Popover");
    if (facet::Button("Filters", {.icon = facet::Icon("filter")}))
        s.popover = !s.popover;
    ctx.Probe("overlays.popover.anchor");
    if (facet::BeginPopover("filters", &s.popover,
                            {.placement = static_cast<facet::PopoverPlacement>(s.placement), .arrow = s.arrow,
                             .dismissible = s.dismissible, .width = 240.0f}))
    {
        facet::Toggle("Option A", &s.popoverOption, {.width = facet::kFill});
        ctx.Probe("overlays.popover.toggle");
        facet::Slider("Value", &s.popoverValue, 0.0f, 1.0f, {.format = "%.2f"});
        ctx.Probe("overlays.popover.slider");
        if (!s.dismissible)
        {
            if (facet::Button("Done"))
                s.popover = false;
            ctx.Probe("overlays.popover.done");
        }
        facet::EndPopover();
    }
    if (ctx.capture && ctx.captureState == kPopover)
        ctx.RevealInCapture(0.3f);
    ImGui::SameLine();
    if (facet::Button("Count a click"))
        ++s.outsidePresses;
    ctx.Probe("overlays.popover.outside");
    LabeledRow("On hover");
    facet::IconButton("Details", facet::Icon("info"));
    ctx.Probe("overlays.popover.hover");
    if (facet::BeginPopover("details", &s.hoverPopover, {.placement = facet::PopoverPlacement::End, .openOnHover = true}))
    {
        Paragraph("Opens while the pointer rests on the button.");
        facet::EndPopover();
    }
    Note("Clicks counted beside the popover: %d.", s.outsidePresses);
}

void ContextMenuSection(facet::demo::PageCtx& ctx, OverlaysState& s)
{
    SectionHeader("Context menu");
    Paragraph("Right-click the button, or focus it and press the Menu key or Shift+F10.");
    LabeledRow("Target");
    facet::Button("Context target", {.icon = facet::Icon("more-horizontal")});
    ctx.Probe("overlays.context");
    if (ctx.capture && ctx.captureState == kContext)
        ctx.ForceNextItemState(ForcedItemState::Open);
    s.contextShown = false;
    if (facet::BeginContextMenu("context"))
    {
        s.contextShown = true;
        facet::MenuItem("Option A");
        facet::MenuItem("Option B");
        facet::MenuItem("Delete", {.destructive = true});
        facet::EndContextMenu();
    }
    if (ctx.capture && ctx.captureState == kContext)
        ctx.RevealInCapture(0.3f);
}

void TooltipsSection(facet::demo::PageCtx& ctx, OverlaysState& s)
{
    SectionHeader("Tooltips");
    Paragraph("A tooltip shows after the pointer rests on its item, and on keyboard focus. It never shows for a "
              "disabled item and takes no focus.");
    LabeledRow("Text");
    facet::Button("Hover for a tooltip");
    ctx.Probe("overlays.tooltip");
    if (ctx.capture && ctx.captureState == kTooltip)
        ctx.ForceNextItemState(ForcedItemState::Hovered);
    facet::ItemTooltip("A short text tooltip that wraps at the tooltip width.");
    s.tooltipShown = facet::internal::IsPopupLevelOpen(facet::internal::PopupTooltipLevel(ImGui::GetItemID()));
    ImGui::SameLine();
    ImGui::BeginDisabled();
    facet::Button("Disabled");
    ctx.Probe("overlays.tooltip.disabled");
    facet::ItemTooltip("Never shown");
    s.disabledTipShown = facet::internal::IsPopupLevelOpen(facet::internal::PopupTooltipLevel(ImGui::GetItemID()));
    ImGui::EndDisabled();
    LabeledRow("Rich");
    facet::Button("Rich tooltip");
    ctx.Probe("overlays.tooltip.rich");
    s.richShown = false;
    if (facet::BeginRichTooltip({.maxWidth = 240.0f}))
    {
        s.richShown = true;
        facet::Text(facet::TextStyle::Headline, "Title");
        facet::Text(facet::TextStyle::Footnote, "Rich content: any widgets, such as key caps.");
        facet::KeyCap(ImGuiMod_Ctrl | ImGuiKey_K);
        facet::EndRichTooltip();
    }
    ImGui::SameLine();
    facet::Button("Interactive tooltip");
    ctx.Probe("overlays.tooltip.interactive");
    s.interactiveShown = false;
    if (facet::BeginRichTooltip({.interactive = true}))
    {
        s.interactiveShown = true;
        if (facet::Button("Count", {.size = facet::ControlSize::Small}))
            ++s.tooltipClicks;
        ctx.Probe("overlays.tooltip.count");
        facet::EndRichTooltip();
    }
    if (ctx.capture && ctx.captureState == kTooltip)
        ctx.RevealInCapture(0.3f);
    Note("Clicks inside the interactive tooltip: %d.", s.tooltipClicks);
}

void ToastsSection(facet::demo::PageCtx& ctx, OverlaysState& s)
{
    SectionHeader("Toasts");
    Paragraph("Toasts stack in a corner, three at a time; later ones wait. A toast pauses while the pointer is over "
              "it or it has focus (Ctrl+Tab reaches it), can be swiped toward the edge and closes with Escape.");
    LabeledRow("Corner");
    ImGui::SetNextItemWidth(DemoWidth(420.0f));
    if (facet::SegmentedControl("##corner", &s.corner, kCorners, {.width = facet::kFill}))
        facet::SetToastCorner(static_cast<facet::ScreenCorner>(s.corner));
    ctx.Probe("overlays.toast.corner");
    LabeledRow("Show");
    const float right = facet::demo::ContentRight();
    if (facet::Button("Plain"))
        s.toast = facet::ShowToast("Placeholder notification");
    ctx.Probe("overlays.toast.plain");
    facet::demo::SameLineIfFits(right, facet::demo::ButtonWidth("With an action"));
    if (facet::Button("With an action"))
        s.actionToast = facet::ShowToast("The item was removed",
                                         {.tone = facet::Tone::Success, .description = "Placeholder description.",
                                          .actionLabel = "Undo"});
    ctx.Probe("overlays.toast.action");
    facet::demo::SameLineIfFits(right, facet::demo::ButtonWidth("With progress"));
    if (facet::Button("With progress"))
        s.progressToast = facet::ShowToast("Working", {.tone = facet::Tone::Accent, .duration = -1.0f,
                                                       .progressBar = true, .progress = s.progress});
    ctx.Probe("overlays.toast.progress");
    LabeledRow("Progress");
    ImGui::SetNextItemWidth(DemoWidth(240.0f));
    if (facet::Slider("##progress", &s.progress, 0.0f, 1.0f, {.format = "%.2f"}) && facet::IsToastVisible(s.progressToast))
        facet::UpdateToast(s.progressToast, "Working",
                           {.tone = facet::Tone::Accent, .duration = -1.0f, .progressBar = true, .progress = s.progress});
    ctx.Probe("overlays.toast.slider");
    if (facet::ToastActionPressed(s.actionToast))
        ++s.actions;
    Note("Undo pressed %d times.", s.actions);

    // Probes of the newest toasts' parts, which facet::EndFrame drew in the last frame.
    ImRect body;
    ImRect action;
    ImRect close;
    if (facet::internal::OverlayToastRects(s.actionToast, &body, &action, &close))
    {
        FloatingProbe(ctx, "overlays.toast.action.body", body);
        FloatingProbe(ctx, "overlays.toast.action.button", action);
        FloatingProbe(ctx, "overlays.toast.action.close", close);
    }
    if (facet::internal::OverlayToastRects(s.toast, &body, &action, &close))
        FloatingProbe(ctx, "overlays.toast.plain.body", body);

    // The capture state shows three toasts; they leave when another state starts.
    if (ctx.capture)
    {
        if (ctx.captureState == kToasts && !s.captureToasts)
        {
            s.toast = facet::ShowToast("Placeholder notification", {.duration = -1.0f});
            s.actionToast = facet::ShowToast("The item was removed", {.tone = facet::Tone::Success,
                                                                      .description = "Placeholder description.",
                                                                      .actionLabel = "Undo", .duration = -1.0f});
            s.progressToast = facet::ShowToast("Working", {.tone = facet::Tone::Accent, .duration = -1.0f,
                                                           .progressBar = true, .progress = 0.6f});
            s.captureToasts = true;
        }
        if (ctx.captureState != kToasts && s.captureToasts)
        {
            facet::DismissToast(s.toast);
            facet::DismissToast(s.actionToast);
            facet::DismissToast(s.progressToast);
            s.captureToasts = false;
        }
    }
}

void DrawerSection(facet::demo::PageCtx& ctx, OverlaysState& s)
{
    SectionHeader("Drawer");
    Paragraph("A panel that slides in from an edge. A modal drawer dims the page and keeps the focus; without modal "
              "the page stays usable beside it.");
    LabeledRow("Edge");
    ImGui::SetNextItemWidth(DemoWidth(300.0f));
    facet::SegmentedControl("##side", &s.side, kSides, {.width = facet::kFill});
    ctx.Probe("overlays.drawer.side");
    LabeledRow("Modal");
    facet::Toggle("##drawerModal", &s.drawerModal);
    ctx.Probe("overlays.drawer.modal");
    LabeledRow("Resizable");
    facet::Toggle("##resizable", &s.resizable);
    LabeledRow("Dismissible");
    facet::Toggle("##drawerDismissible", &s.drawerDismissible);
    LabeledRow("Extent");
    ImGui::SetNextItemWidth(DemoWidth(240.0f));
    facet::Slider("##extent", &s.extent, 0.0f, 480.0f, {.format = s.extent > 0.0f ? "%.0f units" : "Token default"});
    ctx.Probe("overlays.drawer.extent");
    LabeledRow("Drawer");
    if (facet::Button("Open drawer"))
        s.drawer = true;
    ctx.Probe("overlays.drawer.open");
    if (facet::BeginDrawer(kDrawerLabel, &s.drawer,
                           {.side = static_cast<facet::DrawerSide>(s.side), .extent = s.extent, .modal = s.drawerModal,
                            .resizable = s.resizable, .dismissible = s.drawerDismissible}))
    {
        Paragraph("Placeholder content of the drawer.");
        facet::Toggle("Option B", &s.drawerOption);
        ctx.Probe("overlays.drawer.toggle");
        if (facet::Button("Done"))
            s.drawer = false;
        ctx.Probe("overlays.drawer.done");
        facet::EndDrawer();
    }
    ImRect close;
    if (facet::internal::OverlayCloseButtonRect(ImGui::GetID(kDrawerLabel), &close))
        FloatingProbe(ctx, "overlays.drawer.close", close);
}

void LoadingSection(facet::demo::PageCtx& ctx, OverlaysState& s)
{
    SectionHeader("Loading overlay");
    LabeledRow("Loading");
    facet::Toggle("##loading", &s.loading);
    ctx.Probe("overlays.loading.toggle");
    LabeledRow("Show progress");
    facet::Toggle("##showProgress", &s.showProgress);
    LabeledRow("Region");
    facet::BeginLoadingOverlay("region", {.progress = s.showProgress ? s.loadProgress : facet::kIndeterminate,
                                          .loading = s.loading});
    ImGui::BeginGroup();
    if (facet::Button("Count a press"))
        ++s.loadingPresses;
    ctx.Probe("overlays.loading.button");
    facet::TextField("##loadingField", &s.loadingText, {.placeholder = "Placeholder", .width = DemoWidth(280.0f)});
    facet::Text(facet::TextStyle::Footnote, "Placeholder content under the overlay.");
    ImGui::EndGroup();
    facet::EndLoadingOverlay();
    if (ctx.capture && ctx.captureState == kLoading)
        ctx.RevealInCapture();
    Note("Presses counted in the region: %d.", s.loadingPresses);
}

void PaletteSection(facet::demo::PageCtx& ctx, OverlaysState& s)
{
    SectionHeader("Command palette");
    Paragraph("A search field over a list: typing filters, Up and Down move, Enter picks, Escape closes. Ctrl+K opens "
              "it from anywhere on this page; recent picks come first.");
    LabeledRow("Palette");
    if (facet::Button("Open palette", {.icon = facet::Icon("search")}))
        s.palette = true;
    ctx.Probe("overlays.palette.open");
    ImGui::SameLine();
    if (facet::Button("Shortcut palette"))
        s.shortcutPalette = true;
    ctx.Probe("overlays.palette.shortcuts");
    if (facet::CommandPalette(kPaletteLabel, &s.palette, &s.picked, kCommands, {.openShortcut = PaletteShortcut()}))
        ++s.picks;
    for (int i = 0; i < 6; ++i)
    {
        ImRect row;
        char name[40];
        std::snprintf(name, sizeof(name), "overlays.palette.row%d", i);
        if (facet::internal::OverlayPaletteRowRect(ImGui::GetID(kPaletteLabel), i, &row))
            FloatingProbe(ctx, name, row);
    }
    (void)facet::CommandPalette("Shortcuts", &s.shortcutPalette, &s.pickedShortcut);
    facet::ShortcutInfo info;
    Note("Picked: %s. Shortcut picked: %s.", s.picked >= 0 ? kCommands[s.picked].label : "none yet",
         facet::GetShortcut(s.pickedShortcut, &info) ? info.label : "none yet");
}

void WatermarkSection(facet::demo::PageCtx& ctx, OverlaysState& s)
{
    SectionHeader("Watermark overlay");
    Paragraph("A strip of fields in a display corner over everything, letting every click through.");
    LabeledRow("Show");
    facet::Toggle("##watermark", &s.watermark);
    ctx.Probe("overlays.watermark.toggle");
    LabeledRow("Corner");
    ImGui::SetNextItemWidth(DemoWidth(420.0f));
    facet::SegmentedControl("##watermarkCorner", &s.watermarkCorner, kCorners, {.width = facet::kFill});
    ctx.Probe("overlays.watermark.corner");
    if (s.watermark)
        facet::WatermarkOverlay("watermark", kFields, {.corner = static_cast<facet::ScreenCorner>(s.watermarkCorner)});
    ImRect strip;
    if (s.watermark && facet::internal::OverlayWatermarkRect(ImGui::GetID("watermark"), &strip))
        FloatingProbe(ctx, "overlays.watermark", strip);
}

void BarSection(facet::demo::PageCtx& ctx, OverlaysState& s)
{
    SectionHeader("Unsaved changes bar");
    Paragraph("A bar at the bottom of the page while there are unsaved changes. Escape reverts while one of its "
              "buttons has focus.");
    LabeledRow("Changes");
    ImGui::SetNextItemWidth(DemoWidth(240.0f));
    facet::Slider("##changes", &s.changes, 0, 5);
    ctx.Probe("overlays.bar.slider");
    Note("Applied %d times, reverted %d times.", s.applies, s.reverts);
    // Last on the page, so it draws over the page content.
    switch (facet::UnsavedChangesBar(kBarId, s.changes))
    {
    case facet::UnsavedChangesAction::Apply:
        ++s.applies;
        s.changes = 0;
        break;
    case facet::UnsavedChangesAction::Revert:
        ++s.reverts;
        s.changes = 0;
        break;
    case facet::UnsavedChangesAction::None:
        break;
    }
    ImRect revert;
    ImRect apply;
    if (facet::internal::OverlayBarButtonRects(ImGui::GetID(kBarId), &revert, &apply))
    {
        FloatingProbe(ctx, "overlays.bar.revert", revert);
        FloatingProbe(ctx, "overlays.bar.apply", apply);
    }
}

} // namespace

FACET_DEMO_PAGE("overlays", "Overlays", "Widgets", 100, "default", "modal", "confirm", "popover", "context", "tooltip",
                "drawer", "loading", "palette", "toasts")
{
    facet::demo::PageHeader("Overlays",
                            "Modals, confirm dialogs, popovers, context menus, tooltips, toasts, drawers, loading "
                            "overlays, the command palette, watermark overlays and the unsaved changes bar.");
    facet::demo::ReadyFamilyNote("overlays");
    OverlaysState& s = State();
    FollowCapture(ctx, s);
    DialogsSection(ctx, s);
    PopoverSection(ctx, s);
    ContextMenuSection(ctx, s);
    TooltipsSection(ctx, s);
    ToastsSection(ctx, s);
    DrawerSection(ctx, s);
    LoadingSection(ctx, s);
    PaletteSection(ctx, s);
    WatermarkSection(ctx, s);
    BarSection(ctx, s);
}

FACET_DEMO_SELFTEST("overlays")
{
    using T = facet::demo::Target;
    test.Do([] {
        OverlaysState& s = State();
        s = OverlaysState{};
        s.loading = false;
        s.changes = 0;  // the bar covers the bottom of the page; its own steps show it
    });
    test.Expect("every layer starts closed", [] { return !State().AnyLayerOpen(); });

    // Modal, pointer: the button opens it and the focus moves in; Tab stays inside; Escape closes
    // it and the focus is back on the button.
    test.Click("overlays.modal.open");
    test.WaitSeconds(0.3f);
    test.Expect("the button opens the modal", [] { return State().modal; });
    test.ExpectFocused("overlays.modal.toggle");
    for (int i = 0; i < 5; ++i)
        test.KeyPress(ImGuiKey_Tab);
    test.Expect("Tab stays inside the modal", [] { return State().modal; });
    test.Click("overlays.modal.toggle");
    test.Expect("a click inside the modal works", [] { return !State().modalOption; });
    test.KeyPress(ImGuiKey_Escape);
    test.WaitSeconds(0.2f);
    test.Expect("Escape closes the modal", [] { return !State().modal; });
    test.ExpectFocused("overlays.modal.open");
    // Keyboard: Space on the focused button opens it; its close button closes it by mouse.
    test.KeyPress(ImGuiKey_Space);
    test.WaitSeconds(0.3f);
    test.Expect("Space opens the modal", [] { return State().modal; });
    test.Click("overlays.modal.close");
    test.WaitSeconds(0.2f);
    test.Expect("the close button closes the modal", [] { return !State().modal; });
    test.ExpectFocused("overlays.modal.open");

    // A confirm dialog over the modal: Escape closes one layer per press.
    test.Click("overlays.modal.open");
    test.WaitSeconds(0.3f);
    test.Click("overlays.modal.delete");
    test.WaitSeconds(0.3f);
    test.Expect("the confirm dialog opens over the modal", [] { return State().nested && State().modal; });
    test.KeyPress(ImGuiKey_Escape);
    test.WaitSeconds(0.2f);
    test.Expect("the first Escape closes the confirm dialog only",
                [] { return !State().nested && State().modal && State().lastResult == facet::DialogResult::Canceled; });
    test.ExpectFocused("overlays.modal.delete");
    test.Click("overlays.modal.delete");
    test.WaitSeconds(0.3f);
    test.Click("overlays.nested.confirm");
    test.WaitSeconds(0.2f);
    test.Expect("a click on Delete confirms", [] { return State().lastResult == facet::DialogResult::Confirmed; });
    test.KeyPress(ImGuiKey_Escape);
    test.WaitSeconds(0.2f);
    test.Expect("the next Escape closes the modal", [] { return !State().modal; });

    // Confirm dialog: Enter confirms; a destructive one starts on Cancel; the scrim cancels.
    test.Do([] { State().lastResult = facet::DialogResult::None; });
    test.Click("overlays.confirm.open");
    test.WaitSeconds(0.3f);
    test.KeyPress(ImGuiKey_Enter);
    test.WaitSeconds(0.2f);
    test.Expect("Enter confirms", [] { return State().lastResult == facet::DialogResult::Confirmed && !State().confirm; });
    test.ExpectFocused("overlays.confirm.open");
    test.Click("overlays.confirm.open");
    test.WaitSeconds(0.3f);
    test.Click("overlays.confirm.cancel");
    test.WaitSeconds(0.2f);
    test.Expect("a click on Cancel cancels", [] { return State().lastResult == facet::DialogResult::Canceled; });
    test.Click("overlays.confirm.destructive");
    test.Click("overlays.confirm.open");
    test.WaitSeconds(0.3f);
    test.KeyPress(ImGuiKey_Enter);
    test.WaitSeconds(0.2f);
    test.Expect("a destructive dialog starts on Cancel: Enter cancels",
                [] { return State().lastResult == facet::DialogResult::Canceled && !State().confirm; });
    test.Click("overlays.confirm.open");
    test.WaitSeconds(0.3f);
    test.Do([] { State().lastResult = facet::DialogResult::None; });
    test.MouseMove(T::Display(0.02f, 0.98f));
    test.MouseDown();
    test.MouseUp();
    test.WaitSeconds(0.2f);
    test.Expect("a click on the scrim cancels", [] { return State().lastResult == facet::DialogResult::Canceled; });
    test.Click("overlays.confirm.destructive");
    test.Click("overlays.confirm.hold");
    test.Click("overlays.confirm.open");
    test.WaitSeconds(0.3f);
    test.Do([] { State().lastResult = facet::DialogResult::None; });
    test.Click("overlays.confirm.confirm");
    test.Expect("a short click does not confirm a held dialog", [] { return State().lastResult == facet::DialogResult::None; });
    test.MouseMove(T::Item("overlays.confirm.confirm"));
    test.MouseDown();
    test.WaitSeconds(0.5f);
    test.MouseUp();
    test.WaitSeconds(0.2f);
    test.Expect("holding the confirm button confirms", [] { return State().lastResult == facet::DialogResult::Confirmed; });
    test.Click("overlays.confirm.hold");

    // Popover: opens from its button with the focus inside, Escape gives the focus back, a click
    // outside closes it and reaches the button it lands on.
    test.Click("overlays.popover.anchor");
    test.WaitSeconds(0.3f);
    test.Expect("the button opens the popover", [] { return State().popover; });
    test.ExpectFocused("overlays.popover.toggle");
    test.Click("overlays.popover.toggle");
    test.Expect("a control in the popover works", [] { return State().popoverOption; });
    test.KeyPress(ImGuiKey_Escape);
    test.WaitSeconds(0.2f);
    test.Expect("Escape closes the popover", [] { return !State().popover; });
    test.ExpectFocused("overlays.popover.anchor");
    test.KeyPress(ImGuiKey_Space);
    test.WaitSeconds(0.3f);
    test.Expect("Space opens the popover", [] { return State().popover; });
    test.Click("overlays.popover.outside");
    test.Expect("a click outside closes the popover and reaches the page",
                [] { return !State().popover && State().outsidePresses == 1; });
    test.Click("overlays.popover.anchor");
    test.WaitSeconds(0.3f);
    test.Click("overlays.popover.anchor");
    test.Expect("a second click on the button closes the popover", [] { return !State().popover; });
    test.Hover(T::Item("overlays.popover.hover"), 0.6f);
    test.Expect("resting on the details button opens its popover", [] { return State().hoverPopover; });
    test.MouseMove(T::Display(0.02f, 0.98f));
    test.Wait(3);
    test.Expect("leaving it closes the hover popover", [] { return !State().hoverPopover; });

    // Context menu: a right click and the Menu key open it; Escape closes it and the focus stays.
    test.Click("overlays.context", ImGuiMouseButton_Right);
    test.Wait(2);
    test.Expect("a right click opens the context menu", [] { return State().contextShown; });
    test.KeyPress(ImGuiKey_Escape);
    test.Wait(2);
    test.Expect("Escape closes the context menu", [] { return !State().contextShown; });
    test.Click("overlays.context");
    test.KeyPress(ImGuiKey_Menu);
    test.Wait(2);
    test.Expect("the Menu key opens the context menu", [] { return State().contextShown; });
    test.KeyPress(ImGuiKey_Escape);
    test.ExpectFocused("overlays.context");
    test.KeyPress(ImGuiKey_F10 | ImGuiMod_Shift);
    test.Wait(2);
    test.Expect("Shift+F10 opens the context menu", [] { return State().contextShown; });
    test.KeyPress(ImGuiKey_Escape);

    // Tooltips: hover and keyboard focus show them, a disabled item never does, an interactive one
    // takes a click.
    test.Hover(T::Item("overlays.tooltip"), 0.8f);
    test.Expect("resting on the button shows its tooltip", [] { return State().tooltipShown; });
    test.Hover(T::Item("overlays.tooltip.disabled"), 0.8f);
    test.Expect("a disabled item shows no tooltip", [] { return !State().disabledTipShown; });
    test.Hover(T::Item("overlays.tooltip.rich"), 0.8f);
    test.Expect("the rich tooltip shows", [] { return State().richShown; });
    test.MouseMove(T::Display(0.02f, 0.98f));
    test.Click("overlays.tooltip");
    test.MouseMove(T::Display(0.02f, 0.98f));
    test.KeyPress(ImGuiKey_Tab);
    test.WaitSeconds(0.8f);
    test.Expect("keyboard focus shows the tooltip", [] { return State().tooltipShown; });
    test.ExpectFocused("overlays.tooltip");
    test.Click("overlays.tooltip.rich");  // the pointer again: the focused tooltip goes
    test.Hover(T::Item("overlays.tooltip.interactive"), 0.8f);
    test.Expect("the interactive tooltip shows", [] { return State().interactiveShown; });
    test.MouseMove(T::Item("overlays.tooltip.count", 0.2f, 0.2f));
    test.MouseMove(T::Item("overlays.tooltip.count"));
    test.MouseDown();
    test.MouseUp();
    test.Expect("a click inside the interactive tooltip works", [] { return State().tooltipClicks == 1; });
    test.MouseMove(T::Display(0.02f, 0.98f));

    // Toasts: a button shows one; its action reports; the close button and Escape dismiss.
    test.Click("overlays.toast.action");
    test.WaitSeconds(0.4f);
    test.Expect("the button shows a toast", [] { return facet::IsToastVisible(State().actionToast); });
    test.Click("overlays.toast.action.button");
    test.Wait(2);
    test.Expect("the toast's action reports once", [] { return State().actions == 1; });
    test.Expect("the toast leaves after its action", [] { return !facet::IsToastVisible(State().actionToast); });
    test.Click("overlays.toast.action");
    test.WaitSeconds(0.4f);
    test.Click("overlays.toast.action.close");
    test.Wait(2);
    test.Expect("the close button dismisses the toast", [] { return !facet::IsToastVisible(State().actionToast); });
    test.Click("overlays.toast.plain");
    test.WaitSeconds(0.4f);
    test.Hover(T::Item("overlays.toast.plain.body", 0.2f, 0.5f), 6.0f);
    test.Expect("a hovered toast stays past its time", [] { return facet::IsToastVisible(State().toast); });
    test.Drag({T::Item("overlays.toast.plain.body", 0.1f, 0.85f), T::Item("overlays.toast.plain.body", 0.9f, 0.85f)}, 0.1f);
    test.WaitSeconds(0.3f);
    test.Expect("a swipe toward the edge dismisses the toast", [] { return !facet::IsToastVisible(State().toast); });
    test.Click("overlays.toast.plain");
    test.WaitSeconds(0.4f);
    test.KeyPress(ImGuiKey_Tab | ImGuiMod_Ctrl);
    test.Wait(2);
    test.KeyPress(ImGuiKey_Escape);
    test.Wait(2);
    test.Expect("Ctrl+Tab reaches the toast and Escape dismisses it", [] { return !facet::IsToastVisible(State().toast); });

    // Drawer: opens with the focus inside, Escape closes it and gives the focus back; non-modal, the
    // page stays usable beside it.
    test.Click("overlays.drawer.open");
    test.WaitSeconds(0.3f);
    test.Expect("the button opens the drawer", [] { return State().drawer; });
    test.ExpectFocused("overlays.drawer.toggle");
    test.KeyPress(ImGuiKey_Escape);
    test.WaitSeconds(0.2f);
    test.Expect("Escape closes the drawer", [] { return !State().drawer; });
    test.ExpectFocused("overlays.drawer.open");
    test.KeyPress(ImGuiKey_Space);
    test.WaitSeconds(0.3f);
    test.Click("overlays.drawer.close");
    test.WaitSeconds(0.2f);
    test.Expect("the close button closes the drawer", [] { return !State().drawer; });
    test.Click("overlays.drawer.modal");
    test.Do([] {
        // On the leading edge and narrow, so the page shows beside it at every scale.
        State().extent = 160.0f;
        State().side = 1;
    });
    test.Click("overlays.drawer.open");
    test.WaitSeconds(0.3f);
    test.Click("overlays.loading.toggle");
    test.Expect("beside a non-modal drawer the page takes clicks", [] { return State().drawer && State().loading; });
    test.Click("overlays.drawer.done");
    test.Expect("the drawer's own button closes it", [] { return !State().drawer; });
    test.Click("overlays.drawer.modal");
    test.Do([] {
        State().extent = 0.0f;
        State().side = 0;
    });

    // Loading overlay: the region takes no click while loading, and works again after.
    test.Click("overlays.loading.button");
    test.Expect("a loading region takes no click", [] { return State().loadingPresses == 0; });
    test.Click("overlays.loading.toggle");
    test.WaitSeconds(0.3f);
    test.Click("overlays.loading.button");
    test.Expect("the region works when it stops loading", [] { return State().loadingPresses == 1; });
    test.Click("overlays.loading.toggle");
    test.Expect("a click turns loading on", [] { return State().loading; });
    test.KeyPress(ImGuiKey_Tab);  // shows the focus on the toggle
    test.ExpectFocused("overlays.loading.toggle");
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Space turns loading off", [] { return !State().loading; });

    // Command palette: typing filters and Enter picks; Ctrl+K opens it; a click on a row picks;
    // Escape closes without a pick.
    test.Click("overlays.palette.open");
    test.WaitSeconds(0.3f);
    test.Type("command 5");
    test.Wait(2);
    test.KeyPress(ImGuiKey_Enter);
    test.WaitSeconds(0.2f);
    test.Expect("typing and Enter pick an entry", [] { return State().picked == 4 && !State().palette; });
    test.ExpectFocused("overlays.palette.open");
    test.KeyPress(ImGuiKey_K | ImGuiMod_Ctrl);
    test.WaitSeconds(0.3f);
    test.Expect("Ctrl+K opens the palette", [] { return State().palette; });
    test.Click("overlays.palette.row4");  // the recent pick, first in the list
    test.WaitSeconds(0.2f);
    test.Expect("a click on a row picks it", [] { return State().picks == 2 && State().picked == 4 && !State().palette; });
    test.KeyPress(ImGuiKey_K | ImGuiMod_Ctrl);
    test.WaitSeconds(0.3f);
    test.KeyPress(ImGuiKey_Escape);
    test.WaitSeconds(0.2f);
    test.Expect("Escape closes the palette without a pick", [] { return !State().palette && State().picks == 2; });

    // Unsaved changes bar: Apply and Revert by mouse, Escape on its focused button reverts.
    test.Do([] { State().changes = 2; });
    test.WaitSeconds(0.4f);
    test.Click("overlays.bar.apply");
    test.Expect("Apply applies", [] { return State().applies == 1 && State().changes == 0; });
    test.Do([] { State().changes = 3; });
    test.WaitSeconds(0.4f);
    test.Click("overlays.bar.revert");

    test.Expect("Revert reverts", [] { return State().reverts == 1; });
    test.Do([] { State().changes = 1; });
    test.WaitSeconds(0.4f);
    test.Click("overlays.bar.slider");
    test.Do([] { State().changes = 1; });
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Tab);  // from the slider to the bar's first button
    test.KeyPress(ImGuiKey_Escape);
    test.Wait(2);
    test.Expect("Escape on the bar reverts", [] { return State().reverts == 2; });

    // Watermark: shown, and its toggle hides it.
    test.WaitSeconds(0.4f);  // the bar has slid away
    test.ExpectVisible("overlays.watermark");
    test.Click("overlays.watermark.toggle");
    test.Expect("the toggle hides the watermark", [] { return !State().watermark; });
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Space shows it again", [] { return State().watermark; });
    test.Do([] { State() = OverlaysState{}; });
}
