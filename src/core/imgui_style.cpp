// Maps the Facet theme and scale onto a Dear ImGui style, so stock Dear ImGui widgets drawn
// inside a Facet app follow the same tokens.

#include <imgui.h>

#include <algorithm>

#include "core/core_internal.h"
#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/theme.h"

namespace facet
{
namespace
{

struct SyncState
{
    bool enabled = false;
    std::uint64_t themeRevision = 0;
    std::uint64_t scaleRevision = 0;
};

SyncState& Sync()
{
    static SyncState state;
    return state;
}

// Sizes in design units. Scaled as a whole afterwards with ImGuiStyle::ScaleAllSizes.
void ApplySizes(ImGuiStyle& s, const Theme& theme)
{
    const Spacing& sp = theme.spacing;
    const Radii& r = theme.radii;
    const Metrics& m = theme.metrics;
    const float body = theme.type[TextStyle::Body].size;

    s.WindowPadding = ImVec2(sp.lg, sp.lg);
    // Frame height = text height + 2 * FramePadding.y, which gives the regular control height.
    s.FramePadding = ImVec2(sp.md, std::max(0.0f, (m.controlHeight - body) * 0.5f));
    s.ItemSpacing = ImVec2(sp.sm, sp.sm);
    s.ItemInnerSpacing = ImVec2(sp.sm, sp.xs);
    s.CellPadding = ImVec2(sp.sm, sp.xs);
    s.TouchExtraPadding = ImVec2(0.0f, 0.0f);
    s.IndentSpacing = sp.xl;
    s.ColumnsMinSpacing = sp.sm;
    s.ScrollbarSize = sp.md;
    s.GrabMinSize = sp.md;
    s.SeparatorTextPadding = ImVec2(sp.xl, sp.xs);
    s.SeparatorTextAlign = ImVec2(0.0f, 0.5f);
    s.WindowMinSize = ImVec2(sp.xxxl, sp.xxxl);
    s.DisplaySafeAreaPadding = ImVec2(sp.xs, sp.xs);
    s.DisplayWindowPadding = ImVec2(sp.xxxl, sp.xxxl);

    s.WindowRounding = r.window;
    s.ChildRounding = r.card;
    s.PopupRounding = r.popover;
    s.FrameRounding = r.control;
    s.GrabRounding = r.element;
    s.TabRounding = r.element;
    // The scrollbar is a pill: rounded by half its width.
    s.ScrollbarRounding = sp.md * 0.5f;
}

void ApplyColors(ImGuiStyle& style, const Theme& theme)
{
    const Colors& c = theme.colors;
    const ImVec4 clear = WithAlpha(c.bg, 0.0f);
    ImVec4* colors = style.Colors;

    colors[ImGuiCol_Text] = c.label;
    // Stock widgets use TextDisabled for hints and empty states as well; it must stay readable.
    colors[ImGuiCol_TextDisabled] = c.labelSecondary;

    colors[ImGuiCol_WindowBg] = c.bg;
    colors[ImGuiCol_ChildBg] = clear;
    colors[ImGuiCol_PopupBg] = c.bgElevated;
    // Window and frame borders share this color in Dear ImGui. Frames get a hairline border
    // so controls keep a visible boundary on every surface.
    colors[ImGuiCol_Border] = ControlBoundaryColor(c);
    colors[ImGuiCol_BorderShadow] = clear;

    colors[ImGuiCol_FrameBg] = c.fill[1];
    colors[ImGuiCol_FrameBgHovered] = c.fill[0];
    colors[ImGuiCol_FrameBgActive] = Over(c.fill[0], c.fill[2]);

    colors[ImGuiCol_TitleBg] = c.bgSecondary;
    colors[ImGuiCol_TitleBgActive] = c.bgTertiary;
    colors[ImGuiCol_TitleBgCollapsed] = c.bgSecondary;
    colors[ImGuiCol_MenuBarBg] = c.bgSecondary;

    colors[ImGuiCol_ScrollbarBg] = clear;
    colors[ImGuiCol_ScrollbarGrab] = c.fill[0];
    colors[ImGuiCol_ScrollbarGrabHovered] = c.labelTertiary;
    colors[ImGuiCol_ScrollbarGrabActive] = c.labelSecondary;

    colors[ImGuiCol_CheckMark] = c.accent;
    colors[ImGuiCol_CheckboxSelectedBg] = c.accentSubtle;
    colors[ImGuiCol_SliderGrab] = c.accent;
    colors[ImGuiCol_SliderGrabActive] = c.accentPressed;

    colors[ImGuiCol_Button] = c.fill[1];
    colors[ImGuiCol_ButtonHovered] = c.fill[0];
    colors[ImGuiCol_ButtonActive] = Over(c.fill[0], c.fill[2]);

    colors[ImGuiCol_Header] = c.accentSubtle;
    colors[ImGuiCol_HeaderHovered] = c.fill[2];
    colors[ImGuiCol_HeaderActive] = c.fill[1];

    colors[ImGuiCol_Separator] = c.separator;
    colors[ImGuiCol_SeparatorHovered] = c.accentHover;
    colors[ImGuiCol_SeparatorActive] = c.accentPressed;

    colors[ImGuiCol_ResizeGrip] = clear;
    colors[ImGuiCol_ResizeGripHovered] = c.accentSubtle;
    colors[ImGuiCol_ResizeGripActive] = c.accent;
    colors[ImGuiCol_InputTextCursor] = c.label;

    colors[ImGuiCol_Tab] = c.fill[2];
    colors[ImGuiCol_TabHovered] = c.fill[0];
    colors[ImGuiCol_TabSelected] = c.bgTertiary;
    colors[ImGuiCol_TabSelectedOverline] = c.accent;
    colors[ImGuiCol_TabDimmed] = c.fill[3];
    colors[ImGuiCol_TabDimmedSelected] = c.fill[1];
    colors[ImGuiCol_TabDimmedSelectedOverline] = c.separator;

    colors[ImGuiCol_PlotLines] = c.accent;
    colors[ImGuiCol_PlotLinesHovered] = c.accentHover;
    colors[ImGuiCol_PlotHistogram] = c.accent;
    colors[ImGuiCol_PlotHistogramHovered] = c.accentHover;

    colors[ImGuiCol_TableHeaderBg] = c.bgTertiary;
    colors[ImGuiCol_TableBorderStrong] = c.separatorOpaque;
    colors[ImGuiCol_TableBorderLight] = c.separator;
    colors[ImGuiCol_TableRowBg] = clear;
    colors[ImGuiCol_TableRowBgAlt] = c.fill[3];

    colors[ImGuiCol_TextLink] = c.accentText;
    colors[ImGuiCol_TextSelectedBg] = c.accentSubtle;
    colors[ImGuiCol_TreeLines] = c.separator;
    colors[ImGuiCol_DragDropTarget] = c.accent;
    colors[ImGuiCol_DragDropTargetBg] = c.accentSubtle;
    colors[ImGuiCol_UnsavedMarker] = c.label;

    colors[ImGuiCol_NavCursor] = c.focusRing;
    colors[ImGuiCol_NavWindowingHighlight] = c.accent;
    colors[ImGuiCol_NavWindowingDimBg] = c.scrim;
    colors[ImGuiCol_ModalWindowDimBg] = c.scrim;
}

} // namespace

void ApplyImGuiStyle(ImGuiStyle* target)
{
    if (target == nullptr)
    {
        if (ImGui::GetCurrentContext() == nullptr)
            return;
        target = &ImGui::GetStyle();
    }
    // Never the accent of a PushAccent scope: that one belongs to a region of Facet widgets.
    const Theme& theme = internal::UnscopedTheme();

    // Start from the defaults so repeated calls never compound the scaling, then carry over
    // the settings that are not sizes and have no token. Sizes without a token take the
    // Dear ImGui default times the scale, like every other size.
    ImGuiStyle style;
    style.Alpha = target->Alpha;
    style.DisabledAlpha = target->DisabledAlpha;
    style.AntiAliasedLines = target->AntiAliasedLines;
    style.AntiAliasedLineEnds = target->AntiAliasedLineEnds;
    style.AntiAliasedFill = target->AntiAliasedFill;
    style.CurveTessellationMaxError = target->CurveTessellationMaxError;
    style.CircleTessellationMaxError = target->CircleTessellationMaxError;
    style.HoverStationaryDelay = target->HoverStationaryDelay;
    style.HoverDelayShort = target->HoverDelayShort;
    style.HoverDelayNormal = target->HoverDelayNormal;
    style.HoverFlagsForTooltipMouse = target->HoverFlagsForTooltipMouse;
    style.HoverFlagsForTooltipNav = target->HoverFlagsForTooltipNav;
    style.WindowTitleAlign = target->WindowTitleAlign;
    style.WindowMenuButtonPosition = target->WindowMenuButtonPosition;
    style.ColorButtonPosition = target->ColorButtonPosition;
    style.ButtonTextAlign = target->ButtonTextAlign;
    style.SelectableTextAlign = target->SelectableTextAlign;
    style.TableAngledHeadersAngle = target->TableAngledHeadersAngle;
    style.TableAngledHeadersTextAlign = target->TableAngledHeadersTextAlign;
    style.TreeLinesFlags = target->TreeLinesFlags;
    style.MouseCursorScale = target->MouseCursorScale;

    ApplySizes(style, theme);
    style.ScaleAllSizes(Scale());

    // Borders are hairlines: whole pixels, at least one, at every scale.
    const float hairline = HairlineThickness();
    style.WindowBorderSize = hairline;
    style.ChildBorderSize = hairline;
    style.PopupBorderSize = hairline;
    style.FrameBorderSize = hairline;
    style.TabBorderSize = 0.0f;
    style.TabBarBorderSize = hairline;
    style.SeparatorTextBorderSize = hairline;

    // Text: the Body size, scaled by ImGui through the font scale factors.
    style.FontSizeBase = theme.type[TextStyle::Body].size;
    style._NextFrameFontSizeBase = style.FontSizeBase;
    if (ManagesFontScale())
    {
        const float ui = UiScale();
        style.FontScaleMain = ui;
        style.FontScaleDpi = ui > 0.0f ? Scale() / ui : Scale();
    }
    else
    {
        // The host owns the font scale (SetManageFontScale(false)): keep its factors.
        style.FontScaleMain = target->FontScaleMain;
        style.FontScaleDpi = target->FontScaleDpi;
    }

    ApplyColors(style, theme);
    *target = style;

    SyncState& sync = Sync();
    sync.themeRevision = internal::ThemeRevision();
    sync.scaleRevision = internal::ScaleRevision();
}

void SyncImGuiStyle(bool enabled)
{
    Sync().enabled = enabled;
}

bool ImGuiStyleSynced()
{
    return Sync().enabled;
}

namespace internal
{

void SyncImGuiStyleIfNeeded()
{
    const SyncState& sync = Sync();
    if (!sync.enabled || ImGui::GetCurrentContext() == nullptr)
        return;
    if (sync.themeRevision == ThemeRevision() && sync.scaleRevision == ScaleRevision())
        return;
    ApplyImGuiStyle();
}

} // namespace internal

} // namespace facet
