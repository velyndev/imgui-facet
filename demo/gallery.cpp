#include "gallery.h"

#include <cmath>
#include <iterator>

#include <imgui.h>

#include <facet/facet.h>

#include "app_shell.h"

namespace facet::demo
{
namespace
{

// Navigation width in multiples of the font size, so it follows the UI scale.
constexpr float kNavWidthEm = 14.0f;
// Width of the UI scale combo box in multiples of the font size.
constexpr float kScaleComboWidthEm = 5.0f;

// UI scale steps offered in the title strip.
constexpr float kUiScales[] = {0.75f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f};
constexpr const char* kUiScaleLabels[] = {"75%", "100%", "125%", "150%", "175%", "200%"};
static_assert(std::size(kUiScales) == std::size(kUiScaleLabels));

constexpr const char* kDarkLabel = "Dark theme";
constexpr const char* kScaleLabel = "UI scale";

int NearestScaleIndex(float scale)
{
    int best = 0;
    for (int i = 1; i < static_cast<int>(std::size(kUiScales)); ++i)
    {
        if (std::fabs(kUiScales[i] - scale) < std::fabs(kUiScales[best] - scale))
            best = i;
    }
    return best;
}

// Applies theme and scale to Facet and to the ImGui style when they changed. Runs before
// the first window of the frame, so the new font scale takes effect at once.
void ApplyGalleryLook(GalleryState& state)
{
    const float scale = state.baseScale * state.uiScale;
    if (state.appliedScale == scale && state.appliedDark == state.dark)
        return;
    // Switched without animation: capture and selftest runs must not depend on how long a
    // transition from the previous run's theme takes.
    facet::SetTheme(state.dark ? facet::Theme::Dark() : facet::Theme::Light(), false);
    facet::SetDpiScale(state.baseScale);
    facet::SetUiScale(state.uiScale);
    ApplyStyle(scale);
    ApplyThemeToImGuiStyle(facet::T());
    state.appliedScale = scale;
    state.appliedDark = state.dark;
}

void DrawSettings(GalleryState& state, PageCtx& ctx, float comboWidth)
{
    ImGui::Checkbox(kDarkLabel, &state.dark);
    ctx.Probe("gallery.dark");

    ImGui::SameLine();
    int scaleIndex = NearestScaleIndex(state.uiScale);
    ImGui::SetNextItemWidth(comboWidth);
    if (ImGui::Combo(kScaleLabel, &scaleIndex, kUiScaleLabels, static_cast<int>(std::size(kUiScaleLabels))))
        state.uiScale = kUiScales[scaleIndex];
    ctx.Probe("gallery.scale");
}

// Title, theme and scale settings, and the window buttons. When one row is too narrow,
// the settings move to a second row so the window buttons stay reachable.
void DrawTitleStrip(GalleryState& state, PageCtx& ctx, AppShell* shell)
{
    const ImGuiStyle& style = ImGui::GetStyle();
    const char* title = "Facet demo";
    const bool maximized = shell != nullptr && shell->IsWindowMaximized();
    const char* labels[] = {"Minimize", maximized ? "Restore" : "Maximize", "Close"};

    const float comboWidth = ImGui::GetFontSize() * kScaleComboWidthEm;
    const float settingsWidth = ImGui::GetFrameHeight() + style.ItemInnerSpacing.x +
                                ImGui::CalcTextSize(kDarkLabel).x + style.ItemSpacing.x + comboWidth +
                                style.ItemInnerSpacing.x + ImGui::CalcTextSize(kScaleLabel).x;
    float buttonsWidth = 0.0f;
    for (const char* label : labels)
        buttonsWidth += ImGui::CalcTextSize(label).x + style.FramePadding.x * 2.0f;
    buttonsWidth += style.ItemSpacing.x * 2.0f;

    const float innerWidth = ImGui::GetContentRegionAvail().x - style.WindowPadding.x * 2.0f;
    const float oneRowWidth = ImGui::CalcTextSize(title).x + settingsWidth + buttonsWidth + style.ItemSpacing.x * 2.0f;
    const bool twoRows = oneRowWidth > innerWidth;
    const float rowHeight = ImGui::GetFrameHeight();
    const float stripHeight =
        (twoRows ? rowHeight * 2.0f + style.ItemSpacing.y : rowHeight) + style.WindowPadding.y * 2.0f;

    ImGui::BeginChild("##title_strip", ImVec2(0.0f, stripHeight), ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(title);

    ImGui::SameLine();
    const float rightWidth = twoRows ? buttonsWidth : settingsWidth + style.ItemSpacing.x + buttonsWidth;
    const float freeWidth = ImGui::GetContentRegionAvail().x - rightWidth;
    if (freeWidth > 0.0f)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + freeWidth);
    const float controlsScreenX = ImGui::GetCursorScreenPos().x;

    if (!twoRows)
    {
        DrawSettings(state, ctx, comboWidth);
        ImGui::SameLine();
    }
    if (ImGui::Button(labels[0]) && shell != nullptr)
        shell->Minimize();
    ImGui::SameLine();
    if (ImGui::Button(labels[1]) && shell != nullptr)
        shell->ToggleMaximize();
    ImGui::SameLine();
    if (ImGui::Button(labels[2]) && shell != nullptr)
        shell->Close();
    if (twoRows)
        DrawSettings(state, ctx, comboWidth);

    // The first row left of the controls drags the window, unless a popup is open, an item
    // is hovered, or another window (a combo list, a popup, a tooltip that takes input)
    // lies over the strip under the mouse: then clicks there belong to the UI.
    if (shell != nullptr)
    {
        const ImVec2 stripMin = ImGui::GetWindowPos();
        const float captionBottom = stripMin.y + style.WindowPadding.y + rowHeight + style.ItemSpacing.y * 0.5f;
        const ImVec2 captionMax(controlsScreenX - style.ItemSpacing.x,
                                twoRows ? captionBottom : stripMin.y + stripHeight);
        shell->SetCaptionRect(stripMin.x, stripMin.y, captionMax.x, captionMax.y);
        const bool popupOpen = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
        const bool coveredUnderMouse =
            ImGui::IsMouseHoveringRect(stripMin, captionMax, false) && !ImGui::IsWindowHovered();
        shell->SetCaptionBlocked(popupOpen || coveredUnderMouse || ImGui::IsAnyItemHovered());
    }

    ImGui::EndChild();
}

void DrawNavigation(GalleryState& state)
{
    const std::span<const PageInfo> pages = Pages();
    ImGui::BeginChild("##navigation", ImVec2(ImGui::GetFontSize() * kNavWidthEm, 0.0f),
                      ImGuiChildFlags_Borders);
    const std::string* currentGroup = nullptr;
    for (int i = 0; i < static_cast<int>(pages.size()); ++i)
    {
        const PageInfo& page = pages[static_cast<size_t>(i)];
        if (currentGroup == nullptr || *currentGroup != page.group)
        {
            ImGui::SeparatorText(page.group.c_str());
            currentGroup = &page.group;
        }
        ImGui::PushID(page.id.c_str());
        if (ImGui::Selectable(page.title.c_str(), state.selectedPage == i))
            state.selectedPage = i;
        ImGui::PopID();
    }
    if (pages.empty())
        ImGui::TextDisabled("No pages registered.");
    ImGui::EndChild();
}

void DrawContent(GalleryState& state, PageCtx& ctx)
{
    const std::span<const PageInfo> pages = Pages();
    ImGui::BeginChild("##content", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
    if (state.selectedPage >= 0 && state.selectedPage < static_cast<int>(pages.size()))
    {
        const PageInfo& page = pages[static_cast<size_t>(state.selectedPage)];
        ImGui::PushID(page.id.c_str());
        page.draw(ctx);
        ImGui::PopID();
    }
    else
    {
        ImGui::TextDisabled("Select a page on the left.");
    }
    ImGui::EndChild();
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
    ImGuiStyle& style = ImGui::GetStyle();
    style = ImGuiStyle();
    ImGui::StyleColorsDark(&style);
    style.ScaleAllSizes(scale);
    style.FontScaleDpi = scale;
}

// Kept in the demo: the library itself does not touch ImGuiStyle colors.
void ApplyThemeToImGuiStyle(const facet::Theme& theme)
{
    ImGuiStyle& style = ImGui::GetStyle();
    if (theme.dark)
        ImGui::StyleColorsDark(&style);
    else
        ImGui::StyleColorsLight(&style);

    const facet::Colors& c = theme.colors;
    const ImVec4 clear = facet::WithAlpha(c.bg, 0.0f);
    ImVec4* colors = style.Colors;

    colors[ImGuiCol_Text] = c.label;
    // The demo shows empty-state and hint text with TextDisabled; it must stay readable.
    colors[ImGuiCol_TextDisabled] = c.labelSecondary;

    colors[ImGuiCol_WindowBg] = c.bg;
    colors[ImGuiCol_ChildBg] = c.bgSecondary;
    colors[ImGuiCol_PopupBg] = c.bgElevated;
    colors[ImGuiCol_Border] = c.separator;
    colors[ImGuiCol_BorderShadow] = clear;

    colors[ImGuiCol_FrameBg] = c.fill[1];
    colors[ImGuiCol_FrameBgHovered] = c.fill[0];
    colors[ImGuiCol_FrameBgActive] = facet::Over(c.fill[0], c.fill[2]);

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
    colors[ImGuiCol_ButtonActive] = facet::Over(c.fill[0], c.fill[2]);

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

    colors[ImGuiCol_TextLink] = c.accent;
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

void ClearColor(float out[4])
{
    const ImVec4& color = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
    out[0] = color.x;
    out[1] = color.y;
    out[2] = color.z;
    out[3] = 1.0f;
}

void DrawGallery(GalleryState& state, PageCtx& ctx, AppShell* shell)
{
    ApplyGalleryLook(state);
    ctx.dark = state.dark;
    ctx.scale = state.baseScale * state.uiScale;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::Begin("##gallery", nullptr, flags);
    ImGui::PopStyleVar(2);

    DrawTitleStrip(state, ctx, shell);
    DrawNavigation(state);
    ImGui::SameLine();
    DrawContent(state, ctx);

    ImGui::End();
}

} // namespace facet::demo
