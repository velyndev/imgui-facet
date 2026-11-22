#include "gallery.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "app_shell.h"
#include "overlay_backdrop.h"
#include "page_helpers.h"
#include "theme_editor.h"

namespace facet::demo
{
namespace
{

constexpr const char* kTitle = "Facet demo";
// The title bar title and the shell title keep one id while the text could change.
constexpr const char* kTitleBarLabel = "Facet demo###gallery-title";
constexpr const char* kShellLabel = "Facet demo###gallery-shell";

// Widths in multiples of spacing tokens, so they follow the theme and the scale.
constexpr float kTitleScaleWidthInXxxl = 8.0f;   // the UI scale control in the title bar
constexpr float kEditorWidthInXxxl = 12.0f;      // first width of the theme editor pane
constexpr float kOverlayInfoWidthInXxxl = 8.0f;  // the information panel of overlay mode
// The overlay shell in units: first size, smallest and largest.
constexpr float kShellWidthInXxxl = 30.0f;
constexpr float kShellHeightInXxxl = 20.0f;
constexpr float kShellMinWidthInXxxl = 11.0f;
constexpr float kShellMinHeightInXxxl = 8.0f;
constexpr float kShellMaxWidthInXxxl = 50.0f;
constexpr float kShellMaxHeightInXxxl = 34.0f;

// The overlay toggle: a registered shortcut, so it can be rebound like any other.
constexpr const char* kOverlayShortcutName = "demo.overlay";
constexpr ImGuiKeyChord kOverlayShortcutChord = ImGuiKey_F9;

GalleryState* g_currentGallery = nullptr;

// Per page: its navigation title ended in an ellipsis in the last frame.
std::vector<unsigned char>& NavTitleCuts()
{
    static std::vector<unsigned char> cuts;
    return cuts;
}

facet::ThemeMode ModeOf(const GalleryState& state)
{
    if (state.followSystem)
        return facet::ThemeMode::System;
    return state.dark ? facet::ThemeMode::Dark : facet::ThemeMode::Light;
}

// The icon of a page in the navigation: chosen for what the page shows; the rail shows the title
// as a tooltip, so two pages may share one.
struct PageIconEntry
{
    const char* id;
    const char* icon;
};
constexpr PageIconEntry kPageIcons[] = {
    {"showcase", "monitor"},       {"tokens", "palette"},
    {"type", "info"},              {"motion", "play"},
    {"draw", "edit"},              {"effects", "layers"},
    {"icons", "grid"},             {"images", "image"},
    {"item", "mouse"},             {"focus", "eye"},
    {"rows", "menu"},              {"overflow", "more-horizontal"},
    {"value-drag", "sliders"},     {"multi-handle", "chevrons-up-down"},
    {"pill", "check"},             {"fuzzy", "search"},
    {"keys", "keyboard"},          {"chart-core", "arrow-up"},
    {"popup", "external-link"},    {"scroll", "arrow-down"},
    {"list-core", "list"},         {"drag-drop", "grip"},
    {"text-field", "edit"},        {"buttons", "plus"},
    {"selection", "check"},        {"sliders", "sliders"},
    {"inputs-text", "edit"},       {"inputs-value", "chevrons-up-down"},
    {"pickers-color", "eyedropper"}, {"pickers-date", "calendar"},
    {"lists", "list"},             {"navigation", "sidebar"},
    {"containers", "layers"},      {"layout", "grid"},
    {"overlays", "bell"},          {"display", "info"},
    {"data-table", "sort-ascending"}, {"charts", "arrow-up"},
    {"media", "image"},            {"composites-cards", "grid"},
    {"composites-settings", "settings"}, {"composites-editors", "edit"},
};

facet::Icon PageIcon(const PageInfo& page)
{
    for (const PageIconEntry& entry : kPageIcons)
        if (page.id == entry.id)
            return facet::Icon(entry.icon);
    if (page.group == "Foundations")
        return facet::Icon("palette");
    if (page.group == "Media")
        return facet::Icon("image");
    if (page.group == "Blocks")
        return facet::Icon("grid");
    return facet::Icon("sliders");
}

// Case-insensitive substring search, ASCII letters folded.
bool ContainsFolded(std::string_view text, std::string_view needle)
{
    if (needle.empty())
        return true;
    if (needle.size() > text.size())
        return false;
    for (std::size_t start = 0; start + needle.size() <= text.size(); ++start)
    {
        std::size_t i = 0;
        while (i < needle.size() &&
               std::tolower(static_cast<unsigned char>(text[start + i])) == std::tolower(static_cast<unsigned char>(needle[i])))
            ++i;
        if (i == needle.size())
            return true;
    }
    return false;
}

bool PageMatches(const PageInfo& page, std::string_view filter)
{
    return ContainsFolded(page.title, filter) || ContainsFolded(page.group, filter) || ContainsFolded(page.id, filter);
}

facet::ShortcutId OverlayShortcut()
{
    facet::ShortcutId id = facet::FindShortcut(kOverlayShortcutName);
    if (!id.IsValid())
    {
        facet::ShortcutOptions options;
        options.label = "Show or hide the overlay menu";
        options.group = "Test app";
        id = facet::RegisterShortcut(kOverlayShortcutName, kOverlayShortcutChord, options);
    }
    return id;
}

// Applies theme mode, theme edits and scale to Facet and to the ImGui style when they changed.
// Runs before the first window of the frame, so the new font scale takes effect at once.
void ApplyGalleryLook(GalleryState& state, const PageCtx& ctx)
{
    const bool deterministic = ctx.capture || ctx.probes != nullptr;
    facet::SetFollowSystemMotion(!deterministic);
    facet::SyncImGuiStyle(true);

    const bool first = state.appliedMode < 0;
    // An accent chosen outside the editor (the Tokens page swatches call SetAccent and ClearAccent)
    // becomes the editor's accent, so the editor shows it and a later edit keeps it.
    if (!first && !facet::ThemeTransitionActive())
    {
        const bool custom = facet::HasCustomAccent();
        const ImVec4 live = facet::T().colors.accent;
        const bool differs = custom == state.appliedEdits.themeDefaultAccent ||
                             (custom && (ThemeEditValuesDiffer(live.x, state.appliedEdits.accent.x) ||
                                         ThemeEditValuesDiffer(live.y, state.appliedEdits.accent.y) ||
                                         ThemeEditValuesDiffer(live.z, state.appliedEdits.accent.z)));
        if (differs)
        {
            state.edits.themeDefaultAccent = state.appliedEdits.themeDefaultAccent = !custom;
            if (custom)
                state.edits.accent = state.appliedEdits.accent = facet::WithAlpha(live, 1.0f);
        }
    }

    const facet::ThemeMode mode = ModeOf(state);
    const bool dark = mode == facet::ThemeMode::System ? facet::SystemPrefersDarkTheme() : state.dark;
    const ThemeEdits& target = state.editsHeld ? state.appliedEdits : state.edits;
    const bool modeChanged = first || state.appliedMode != static_cast<int>(mode);
    const bool editsChanged = !(target == state.appliedEdits);
    bool changed = false;
    if (first)
    {
        // A fresh gallery starts from the presets, whatever an earlier run chose.
        facet::ClearAccent();
        facet::SetTheme(facet::Theme::Light(), false);
    }
    if (first || modeChanged || dark != state.appliedDark || editsChanged)
    {
        const bool animate = !deterministic && !first;
        const bool accentChanged = first || target.themeDefaultAccent != state.appliedEdits.themeDefaultAccent ||
                                   ThemeEditValuesDiffer(target.accent.x, state.appliedEdits.accent.x) ||
                                   ThemeEditValuesDiffer(target.accent.y, state.appliedEdits.accent.y) ||
                                   ThemeEditValuesDiffer(target.accent.z, state.appliedEdits.accent.z);
        // The accent is set through SetAccent, so the library's own mode switches (System follows
        // Windows) keep it.
        if (accentChanged)
        {
            if (target.themeDefaultAccent)
                facet::ClearAccent();
            else
                facet::SetAccent(target.accent);
        }
        if (modeChanged)
            facet::SetThemeMode(mode, animate);
        facet::SetTheme(ApplyThemeEdits(ThemeEditorPreset(dark), target), animate);
        state.appliedEdits = target;
        state.appliedDark = dark;
        changed = true;
    }

    const float scale = state.baseScale * state.uiScale;
    if (state.appliedScale != scale)
    {
        facet::SetScaleMode(facet::ScaleMode::FollowDpi);
        facet::SetDpiScale(state.baseScale);
        facet::SetUiScale(state.uiScale);
        changed = true;
    }
    if (changed)
        facet::ApplyImGuiStyle();
    state.appliedMode = static_cast<int>(mode);
    state.appliedScale = scale;
}

// The controls beside the title: the theme editor button, the theme mode and the UI scale, as
// many as the width class has room for. The editor holds all of them, so a narrow window keeps
// every setting one click away.
void DrawChromeControls(GalleryState& state, PageCtx& ctx, facet::WidthClass widthClass)
{
    facet::IconButtonOptions editor;
    editor.selected = state.editorOpen;
    if (facet::IconButton(state.editorOpen ? "Close the theme editor###editor-toggle" : "Open the theme editor###editor-toggle",
                          facet::Icon("sliders"), editor))
        state.editorOpen = !state.editorOpen;
    ctx.Probe("gallery.editor");
    if (widthClass != facet::WidthClass::Compact)
    {
        ImGui::SameLine();
        facet::ThemeMode mode = ModeOf(state);
        facet::ThemeModeControlOptions options;
        options.apply = false;
        if (facet::ThemeModeControl("##theme", &mode, options))
        {
            state.followSystem = mode == facet::ThemeMode::System;
            if (!state.followSystem)
                state.dark = mode == facet::ThemeMode::Dark;
        }
        ctx.Probe("gallery.theme");
    }
    if (widthClass == facet::WidthClass::Expanded)
    {
        ImGui::SameLine();
        GalleryUiScaleControl(state, "##scale", facet::T().spacing.xxxl * kTitleScaleWidthInXxxl);
        ctx.Probe("gallery.scale");
    }
}

// The title bar of app mode: title, the chrome controls and the window buttons, with a hairline
// below. Its free area is the window's drag region (the shell asks AppFrameHitTest).
void DrawTitleBar(GalleryState& state, PageCtx& ctx, AppShell* shell)
{
    const bool maximized = shell != nullptr && shell->IsWindowMaximized();
    facet::TitleBarOptions options;
    options.windowButtons = true;
    options.maximized = maximized;
    // The width class of the whole bar: inside the bar the cursor already sits at the items.
    const facet::WidthClass widthClass = facet::AvailableWidthClass();
    facet::BeginTitleBar(kTitleBarLabel, options);
    DrawChromeControls(state, ctx, widthClass);
    facet::EndTitleBar();
    ctx.Probe("gallery.titlebar");
    const ImVec2 barMin = ImGui::GetItemRectMin();
    const ImVec2 barMax = ImGui::GetItemRectMax();
    if (ctx.probes != nullptr)
    {
        // Probes of the window buttons for the selftests, placed as containers.md describes the
        // bar: three square buttons of the control height at the trailing end, spacing.xs apart,
        // centered on the bar, with their ids under the bar's id.
        const float size = facet::ControlHeight(facet::ControlSize::Inherit);
        const float gap = facet::Px(facet::T().spacing.xs);
        const float inset = (barMax.y - barMin.y - size) * 0.5f;
        const float top = std::floor((barMin.y + barMax.y) * 0.5f - size * 0.5f);
        float x = barMax.x - inset - (size * 3.0f + gap * 2.0f);
        const ImGuiID bar = ImGui::GetID(kTitleBarLabel);
        const char* keys[3] = {"##minimize", "##maximize", "##close"};
        const char* probes[3] = {"gallery.minimize", "gallery.maximize", "gallery.close"};
        for (int i = 0; i < 3; ++i)
        {
            ProbeRect(ctx, probes[i], ImHashStr(keys[i], 0, bar), ImVec2(x, top), ImVec2(x + size, top + size));
            x += size + gap;
        }
    }

    const facet::WindowButton pressed = facet::TitleBarButtonPressed();
    if (pressed != facet::WindowButton::None)
        state.lastWindowButton = pressed;
    if (shell != nullptr)
    {
        switch (pressed)
        {
        case facet::WindowButton::Minimize:
            shell->Minimize();
            break;
        case facet::WindowButton::Maximize:
            shell->ToggleMaximize();
            break;
        case facet::WindowButton::Close:
            shell->Close();
            break;
        case facet::WindowButton::None:
            break;
        }
        // A popup, a tooltip or another window over the bar takes the click there, not the
        // window's drag region.
        const bool popupOpen = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
        const bool covered = ImGui::IsMouseHoveringRect(barMin, barMax, false) &&
                             !ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
        shell->SetCaptionBlocked(popupOpen || covered);
    }

    facet::HairlineH(ImGui::GetWindowDrawList(), barMin.x, barMax.x, barMax.y - facet::HairlineThickness(),
                     facet::ToU32(facet::T().colors.separator));
}

// The page list: a filter field (or, in the icon rail, a button that shows the labels) above a
// sidebar with one section per group and one item per page. The selected page sits on the
// selection pill; a newly selected page is scrolled into view.
void DrawNavigation(GalleryState& state, PageCtx& ctx, float height)
{
    const facet::Theme& t = facet::T();
    const std::span<const PageInfo> pages = Pages();
    const bool rail = !state.navExpanded && facet::AvailableWidthClass() != facet::WidthClass::Expanded;
    state.navRail = rail;
    const float expandedWidth = facet::Px(t.containers.sidebarWidth);
    const float gap = facet::Px(t.spacing.sm);

    ImGui::BeginGroup();
    float headerHeight = 0.0f;
    const float headerTop = ImGui::GetCursorScreenPos().y;
    if (rail)
    {
        // The rail's own column width centers the button over the icons.
        const float railWidth = facet::Px(t.containers.railWidth);
        const float button = facet::ControlHeight(facet::ControlSize::Inherit);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (railWidth - button) * 0.5f));
        if (facet::IconButton("Show the page names and the filter###nav-expand", facet::Icon("sidebar")))
            state.navExpanded = true;
        ctx.Probe("nav.expand");
    }
    else
    {
        const bool collapsible = state.navExpanded && facet::AvailableWidthClass() != facet::WidthClass::Expanded;
        const float button = collapsible ? facet::ControlHeight(facet::ControlSize::Inherit) + gap : 0.0f;
        ImGui::SetNextItemWidth(std::max(button, expandedWidth - button));
        facet::SearchFieldOptions filter;
        filter.placeholder = "Filter pages";
        facet::SearchField("##nav-filter", &state.navFilter, filter);
        ctx.Probe("nav.filter");
        if (collapsible)
        {
            ImGui::SameLine(0.0f, gap);
            facet::IconButtonOptions collapse;
            collapse.selected = true;
            if (facet::IconButton("Show icons only###nav-collapse", facet::Icon("sidebar"), collapse))
                state.navExpanded = false;
            ctx.Probe("nav.collapse");
        }
    }
    headerHeight = ImGui::GetCursorScreenPos().y - headerTop;

    facet::SidebarOptions options;
    options.mode = rail ? facet::SidebarMode::Rail : facet::SidebarMode::Expanded;
    options.height = std::max(t.list.rowHeight, (height - headerHeight) / facet::Scale());
    std::vector<unsigned char>& cuts = NavTitleCuts();
    if (cuts.size() != pages.size())
        cuts.assign(pages.size(), 0);
    const std::string_view filter = rail ? std::string_view() : std::string_view(state.navFilter);
    if (facet::BeginSidebar("##pages", options))
    {
        // Room of a title in an expanded row: the row less its padding, the icon and the gap after it.
        const float rowWidth = ImGui::GetContentRegionAvail().x;
        facet::PushTextStyle(facet::TextStyle::Body);
        const float iconSize = std::round(ImGui::GetFontSize());
        facet::PopTextStyle();
        const float titleRoom = rowWidth - gap * 3.0f - iconSize;
        int shown = 0;
        const std::string* group = nullptr;
        bool sectionOpen = false;
        char probe[96];
        for (int i = 0; i < static_cast<int>(pages.size()); ++i)
        {
            const PageInfo& page = pages[static_cast<std::size_t>(i)];
            cuts[static_cast<std::size_t>(i)] = 0;
            if (!PageMatches(page, filter))
                continue;
            if (group == nullptr || *group != page.group)
            {
                if (sectionOpen)
                    facet::EndSidebarSection();
                sectionOpen = facet::BeginSidebarSection(page.group.c_str());
                group = &page.group;
            }
            if (!sectionOpen)
                continue;
            const bool selected = state.selectedPage == i;
            facet::SidebarItemOptions item;
            item.selected = selected;
            ImGui::PushID(page.id.c_str());
            if (facet::SidebarItem(page.title.c_str(), PageIcon(page), item))
                state.selectedPage = i;
            ImGui::PopID();
            std::snprintf(probe, sizeof(probe), "nav.%s", page.id.c_str());
            ctx.Probe(probe);
            ++shown;
            if (!rail)
            {
                // The selected title is semibold, as the sidebar draws it.
                facet::PushTextStyle(facet::TextStyle::Body);
                if (selected)
                    ImGui::PushFont(facet::WeightFont(facet::FontWeight::Semibold), 0.0f);
                cuts[static_cast<std::size_t>(i)] = ImGui::CalcTextSize(page.title.c_str()).x > titleRoom ? 1 : 0;
                if (selected)
                    ImGui::PopFont();
                facet::PopTextStyle();
            }
            // A newly selected page is scrolled to the middle of the list when it is not fully in
            // view; the request repeats until it is, because a list laid out for the first time
            // cannot scroll yet. A list too short for a whole row gives up at once.
            if (selected && state.revealedPage != i)
            {
                const float top = ImGui::GetItemRectMin().y;
                const float bottom = ImGui::GetItemRectMax().y;
                const float viewTop = ImGui::GetWindowPos().y;
                const float viewBottom = viewTop + ImGui::GetWindowHeight();
                const bool visible = top >= viewTop - 0.5f && bottom <= viewBottom + 0.5f;
                if (!visible)
                    ImGui::SetScrollHereY(0.5f);
                if (visible || viewBottom - viewTop < bottom - top)
                    state.revealedPage = i;
            }
        }
        if (sectionOpen)
            facet::EndSidebarSection();
        if (shown == 0)
        {
            facet::EmptyStateOptions empty;
            empty.message = pages.empty() ? "No pages are registered." : "No page title or group contains the filter.";
            empty.icon = facet::Icon("search");
            empty.actionLabel = pages.empty() ? nullptr : "Clear the filter";
            empty.compact = true;
            if (facet::EmptyState(pages.empty() ? "No pages###nav-empty" : "No match###nav-empty", empty))
                state.navFilter.clear();
            ctx.Probe("nav.empty");
        }
        facet::EndSidebar();
    }
    ImGui::EndGroup();
}

void DrawPage(GalleryState& state, PageCtx& ctx)
{
    const facet::Theme& t = facet::T();
    const std::span<const PageInfo> pages = Pages();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, facet::Px(ImVec2(t.spacing.xxl, t.spacing.xl)));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::BeginChild("##content", ImVec2(0.0f, 0.0f), ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
    if (state.selectedPage >= 0 && state.selectedPage < static_cast<int>(pages.size()))
    {
        const PageInfo& page = pages[static_cast<std::size_t>(state.selectedPage)];
        ImGui::PushID(page.id.c_str());
        page.draw(ctx);
        ImGui::PopID();
    }
    else
    {
        Paragraph("Select a page in the navigation.");
    }
    ImGui::EndChild();
}

// Navigation, page and theme editor pane, filling the rest of the current window.
void DrawBody(GalleryState& state, PageCtx& ctx)
{
    const facet::Theme& t = facet::T();
    const float inset = facet::Px(t.spacing.sm);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float height = ImGui::GetContentRegionAvail().y;

    ImGui::SetCursorScreenPos(ImVec2(origin.x + inset, origin.y + inset));
    DrawNavigation(state, ctx, height - inset * 2.0f);
    const float navRight = ImGui::GetItemRectMax().x;

    ImGui::SetCursorScreenPos(ImVec2(navRight, origin.y));
    facet::InspectorPaneOptions pane;
    pane.paneWidth = t.spacing.xxxl * kEditorWidthInXxxl;
    pane.height = height / facet::Scale();
    facet::BeginInspectorPane(kThemeEditorTitle, &state.editorOpen, pane);
    DrawPage(state, ctx);
    if (facet::InspectorPaneNext())
        DrawThemeEditor(state, ctx);
    facet::EndInspectorPane();
}

void DrawAppMode(GalleryState& state, PageCtx& ctx, AppShell* shell)
{
    if (shell != nullptr)
        shell->SetAppFrameHitTest(true);
    facet::AppFrameOptions options;
    options.hostMaximized = shell != nullptr && shell->IsWindowMaximized();
    if (facet::BeginAppFrame("##gallery", options))
    {
        DrawTitleBar(state, ctx, shell);
        DrawBody(state, ctx);
        facet::EndAppFrame();
    }
}

// The information panel of overlay mode while the menu is hidden: the key that shows it and the
// input regions the overlay reports (none). Pinned, it lets input through to the backdrop. While
// the menu shows, the theme editor's Window section says the same, so the panel never covers the
// menu on a small display.
void DrawOverlayInfo(GalleryState& state, PageCtx& ctx, facet::ShortcutId toggle)
{
    if (state.overlayVisible)
        return;
    facet::MiniWindowOptions options;
    options.icon = facet::Icon("info");
    options.width = facet::T().spacing.xxxl * kOverlayInfoWidthInXxxl;
    if (facet::BeginMiniWindow("Overlay###overlay-info", &state.overlayInfoPinned, options))
    {
        facet::ShortcutHint("Show the menu", toggle);
        facet::ScreenRegion regions[8];
        const int count = facet::GetOverlayInputRegions(regions);
        const bool over = facet::OverlayHitTest(ImGui::GetIO().MousePos);
        Note("Input regions: %d. Pointer on the menu: %s.", count, over ? "yes" : "no");
        ctx.Probe("overlay.info");
        facet::EndMiniWindow();
    }
}

void DrawOverlayMode(GalleryState& state, PageCtx& ctx, AppShell* shell)
{
    if (shell != nullptr)
    {
        shell->SetAppFrameHitTest(false);
        shell->SetCaptionBlocked(true);
    }
    const facet::Theme& t = facet::T();
    // facet::ReduceMotion() covers the theme token and the system animation setting.
    if (!state.backdropPaused && !facet::ReduceMotion())
        state.backdropTime += static_cast<double>(ImGui::GetIO().DeltaTime);
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    DrawOverlayBackdrop(ImGui::GetBackgroundDrawList(), viewport->Pos,
                        ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y), state.backdropTime);

    const facet::ShortcutId toggle = OverlayShortcut();
    facet::OverlayOptions overlay;
    overlay.toggleShortcut = toggle;
    if (facet::BeginOverlay("##gallery-overlay", &state.overlayVisible, overlay))
    {
        facet::ShellOptions options;
        options.initialSize = ImVec2(t.spacing.xxxl * kShellWidthInXxxl, t.spacing.xxxl * kShellHeightInXxxl);
        options.minSize = ImVec2(t.spacing.xxxl * kShellMinWidthInXxxl, t.spacing.xxxl * kShellMinHeightInXxxl);
        options.maxSize = ImVec2(t.spacing.xxxl * kShellMaxWidthInXxxl, t.spacing.xxxl * kShellMaxHeightInXxxl);
        options.persist = false;
        bool open = true;
        if (facet::BeginShell(kShellLabel, &open, options))
        {
            // The shell's whole window, for the selftests that move and resize it.
            const ImGuiWindow* root = ImGui::GetCurrentWindowRead()->RootWindow;
            state.shellMin = root->Pos;
            state.shellMax = ImVec2(root->Pos.x + root->Size.x, root->Pos.y + root->Size.y);
            FloatingProbeRect(ctx, "overlay.shell", root->ID, state.shellMin, state.shellMax);
            // The chrome controls on a row of their own above the navigation and the page.
            DrawChromeControls(state, ctx, facet::AvailableWidthClass());
            DrawBody(state, ctx);
            facet::EndShell();
        }
        // The shell's close button hides the menu like the toggle key.
        if (!open)
            state.overlayVisible = false;
        facet::EndOverlay();
    }
    DrawOverlayInfo(state, ctx, toggle);
}

} // namespace

void ConfigureImGuiIo(bool deterministic)
{
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    if (deterministic)
    {
        io.ConfigInputTextCursorBlink = false;
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    }
}

void ApplyStyle(float scale)
{
    facet::SetDpiScale(scale);
    facet::ApplyImGuiStyle();
}

void ClearColor(float out[4])
{
    const ImVec4& color = facet::T().colors.bg;
    out[0] = color.x;
    out[1] = color.y;
    out[2] = color.z;
    out[3] = 1.0f;
}

void DrawGallery(GalleryState& state, PageCtx& ctx, AppShell* shell)
{
    g_currentGallery = &state;
    ApplyGalleryLook(state, ctx);
    ctx.dark = facet::T().dark;
    ctx.scale = state.baseScale * state.uiScale;
    if (state.overlayMode)
        DrawOverlayMode(state, ctx, shell);
    else
        DrawAppMode(state, ctx, shell);
}

GalleryState* CurrentGallery()
{
    return g_currentGallery;
}

bool NavTitleCut(const char* pageId)
{
    const std::span<const PageInfo> pages = Pages();
    const std::vector<unsigned char>& cuts = NavTitleCuts();
    for (std::size_t i = 0; i < pages.size() && i < cuts.size(); ++i)
    {
        if (pages[i].id == pageId)
            return cuts[i] != 0;
    }
    return false;
}

} // namespace facet::demo
