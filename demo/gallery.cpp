#include "gallery.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <iterator>
#include <string>

#include <imgui.h>

#include <facet/facet.h>

#include "app_shell.h"
#include "internal/text_util.h"

namespace facet::demo
{
namespace
{

// UI scale steps offered in the title strip.
constexpr float kUiScales[] = {0.75f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f};
constexpr const char* kUiScaleLabels[] = {"75%", "100%", "125%", "150%", "175%", "200%"};
static_assert(std::size(kUiScales) == std::size(kUiScaleLabels));

// Theme choices in the title strip, in facet::ThemeMode order.
constexpr const char* kThemeLabels[] = {"Light", "Dark", "System"};

constexpr const char* kTitle = "Facet demo";
constexpr const char* kThemeLabel = "Theme";
constexpr const char* kScaleLabel = "UI scale";

// Widths in multiples of spacing tokens, so they follow the theme and the scale.
constexpr float kNavWidthInXxxl = 7.0f;     // navigation column
constexpr float kNavMinWidthInXxxl = 4.0f;  // navigation column in narrow windows
// Largest share of the window the navigation takes, so narrow windows keep room for the page.
constexpr float kNavMaxShare = 0.3f;
constexpr float kComboWidthInXxxl = 3.0f;   // theme and scale lists
// Settings rows of the pages: the label column, and the narrowest control kept beside it.
constexpr float kLabelColumnInXxxl = 3.5f;
constexpr float kMinRowControlInXxxl = 4.0f;

// Sub keys of the animated values of the navigation.
constexpr int kPillY = 0;

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

ImU32 Color(const ImVec4& color)
{
    return facet::ToU32(color);
}

facet::ThemeMode ModeOf(const GalleryState& state)
{
    if (state.followSystem)
        return facet::ThemeMode::System;
    return state.dark ? facet::ThemeMode::Dark : facet::ThemeMode::Light;
}

// Applies theme mode and scale to Facet and to the ImGui style when they changed. Runs before
// the first window of the frame, so the new font scale takes effect at once.
void ApplyGalleryLook(GalleryState& state, const PageCtx& ctx)
{
    const bool deterministic = ctx.capture || ctx.probes != nullptr;
    facet::SetFollowSystemMotion(!deterministic);
    facet::SyncImGuiStyle(true);

    const facet::ThemeMode mode = ModeOf(state);
    const float scale = state.baseScale * state.uiScale;
    bool changed = false;
    if (state.appliedMode < 0)
    {
        // A fresh gallery starts from the presets, whatever an earlier run chose: a mode switch
        // alone keeps the motion, radius and spacing settings of the current theme.
        facet::ClearAccent();
        facet::SetTheme(facet::Theme::Light(), false);
        facet::SetThemeMode(mode, false);
        changed = true;
    }
    else if (state.appliedMode != static_cast<int>(mode))
    {
        facet::SetThemeMode(mode, !deterministic);
        changed = true;
    }
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

// A label in front of its control, centered on the frame height.
void LeadingLabel(const char* text)
{
    ImGui::AlignTextToFramePadding();
    ImGui::PushStyleColor(ImGuiCol_Text, facet::T().colors.labelSecondary);
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
}

void DrawSettings(GalleryState& state, PageCtx& ctx, float comboWidth)
{
    const float gap = facet::Px(facet::T().spacing.lg);

    LeadingLabel(kThemeLabel);
    const int themeIndex = static_cast<int>(ModeOf(state));
    ImGui::SetNextItemWidth(comboWidth);
    const bool open = ImGui::BeginCombo("##theme", kThemeLabels[themeIndex]);
    facet::ItemCursor();
    ctx.Probe("gallery.theme");
    if (open)
    {
        char probe[32];
        for (int i = 0; i < static_cast<int>(std::size(kThemeLabels)); ++i)
        {
            const bool selected = i == themeIndex;
            if (ImGui::Selectable(kThemeLabels[i], selected))
            {
                state.followSystem = i == static_cast<int>(facet::ThemeMode::System);
                if (!state.followSystem)
                    state.dark = i == static_cast<int>(facet::ThemeMode::Dark);
            }
            if (selected)
                ImGui::SetItemDefaultFocus();
            std::snprintf(probe, sizeof(probe), "gallery.theme.%d", i);
            ctx.Probe(probe);
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine(0.0f, gap);
    LeadingLabel(kScaleLabel);
    int scaleIndex = NearestScaleIndex(state.uiScale);
    ImGui::SetNextItemWidth(comboWidth);
    if (ImGui::Combo("##scale", &scaleIndex, kUiScaleLabels, static_cast<int>(std::size(kUiScaleLabels))))
        state.uiScale = kUiScales[scaleIndex];
    facet::ItemCursor();
    ctx.Probe("gallery.scale");
}

float SettingsWidth(float comboWidth)
{
    const ImGuiStyle& style = ImGui::GetStyle();
    const float gap = facet::Px(facet::T().spacing.lg);
    return comboWidth * 2.0f + style.ItemInnerSpacing.x * 2.0f + ImGui::CalcTextSize(kThemeLabel).x +
           ImGui::CalcTextSize(kScaleLabel).x + gap;
}
// Title, theme and scale settings, and the window buttons, on the window background with a
// hairline below. When one row is too narrow, the settings move to a second row so the window
// buttons stay reachable.
float DrawTitleStrip(GalleryState& state, PageCtx& ctx, AppShell* shell)
{
    const facet::Theme& theme = facet::T();
    const ImGuiStyle& style = ImGui::GetStyle();
    const bool maximized = shell != nullptr && shell->IsWindowMaximized();
    const char* labels[] = {"Minimize", maximized ? "Restore" : "Maximize", "Close"};
    const ImVec2 padding = facet::Px(ImVec2(theme.spacing.lg, theme.spacing.sm));

    const float comboWidth = facet::Px(theme.spacing.xxxl * kComboWidthInXxxl);
    const float settingsWidth = SettingsWidth(comboWidth);
    float buttonsWidth = 0.0f;
    for (const char* label : labels)
        buttonsWidth += ImGui::CalcTextSize(label).x + style.FramePadding.x * 2.0f;
    buttonsWidth += style.ItemSpacing.x * 2.0f;

    const float titleWidth = facet::MeasureText(facet::TextStyle::Headline, kTitle).x;
    const float innerWidth = ImGui::GetContentRegionAvail().x - padding.x * 2.0f;
    const float gap = facet::Px(theme.spacing.xl);
    const bool twoRows = titleWidth + settingsWidth + buttonsWidth + gap * 2.0f > innerWidth;
    const float rowHeight = ImGui::GetFrameHeight();
    const float stripHeight = (twoRows ? rowHeight * 2.0f + style.ItemSpacing.y : rowHeight) + padding.y * 2.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, padding);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
    ImGui::BeginChild("##title_strip", ImVec2(0.0f, stripHeight), ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar(2);

    // Title, centered on the first row.
    const float titleHeight = facet::MeasureText(facet::TextStyle::Headline, kTitle).y;
    const ImVec2 rowStart = ImGui::GetCursorScreenPos();
    ImGui::SetCursorScreenPos(ImVec2(rowStart.x, rowStart.y + (rowHeight - titleHeight) * 0.5f));
    facet::Text(facet::TextStyle::Headline, "%s", kTitle);
    ImGui::SetCursorScreenPos(rowStart);
    ImGui::Dummy(ImVec2(titleWidth, rowHeight));

    ImGui::SameLine();
    const float rightWidth = twoRows ? buttonsWidth : settingsWidth + gap + buttonsWidth;
    const float freeWidth = ImGui::GetContentRegionAvail().x - rightWidth;
    if (freeWidth > 0.0f)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + freeWidth);
    const float controlsScreenX = ImGui::GetCursorScreenPos().x;

    if (!twoRows)
    {
        DrawSettings(state, ctx, comboWidth);
        ImGui::SameLine(0.0f, gap);
    }
    if (ImGui::Button(labels[0]) && shell != nullptr)
        shell->Minimize();
    facet::ItemCursor();
    ctx.Probe("gallery.minimize");
    ImGui::SameLine();
    // A fixed id: the label switches between Maximize and Restore, the item stays.
    char maximizeLabel[32];
    std::snprintf(maximizeLabel, sizeof(maximizeLabel), "%s###maximize", labels[1]);
    if (ImGui::Button(maximizeLabel) && shell != nullptr)
        shell->ToggleMaximize();
    facet::ItemCursor();
    ctx.Probe("gallery.maximize");
    ImGui::SameLine();
    if (ImGui::Button(labels[2]) && shell != nullptr)
        shell->Close();
    facet::ItemCursor();
    ctx.Probe("gallery.close");
    if (twoRows)
        DrawSettings(state, ctx, comboWidth);

    // The first row left of the controls drags the window, unless a popup is open, an item
    // is hovered, or another window (a combo list, a popup, a tooltip that takes input)
    // lies over the strip under the mouse: then clicks there belong to the UI.
    const ImVec2 stripMin = ImGui::GetWindowPos();
    if (shell != nullptr)
    {
        const float captionBottom = stripMin.y + padding.y + rowHeight + style.ItemSpacing.y * 0.5f;
        const ImVec2 captionMax(controlsScreenX - style.ItemSpacing.x,
                                twoRows ? captionBottom : stripMin.y + stripHeight);
        shell->SetCaptionRect(stripMin.x, stripMin.y, captionMax.x, captionMax.y);
        const bool popupOpen = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
        const bool coveredUnderMouse =
            ImGui::IsMouseHoveringRect(stripMin, captionMax, false) && !ImGui::IsWindowHovered();
        shell->SetCaptionBlocked(popupOpen || coveredUnderMouse || ImGui::IsAnyItemHovered());
    }
    ImGui::EndChild();

    // Hairline under the strip, across the window.
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const float y = stripMin.y + stripHeight - facet::HairlineThickness();
    facet::HairlineH(drawList, ImGui::GetWindowPos().x, ImGui::GetWindowPos().x + ImGui::GetWindowWidth(), y,
                     Color(theme.colors.separator));
    return stripHeight;
}

// The page list: group headings and one row per page. The selected row sits on the
// selection pill, which springs to the new row when the selection changes.
void DrawNavigation(GalleryState& state, PageCtx& ctx)
{
    const facet::Theme& theme = facet::T();
    const facet::Colors& c = theme.colors;
    const std::span<const PageInfo> pages = Pages();
    const float width = std::min(facet::Px(theme.spacing.xxxl * kNavWidthInXxxl),
                                 std::max(facet::Px(theme.spacing.xxxl * kNavMinWidthInXxxl),
                                          ImGui::GetContentRegionAvail().x * kNavMaxShare));
    const ImVec2 padding = facet::Px(ImVec2(theme.spacing.sm, theme.spacing.md));
    const float rowHeight = facet::Px(theme.metrics.controlHeight);
    const float rowInset = facet::Px(theme.spacing.md);
    const float radius = facet::Px(theme.radii.control);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, c.bgSecondary);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, padding);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, facet::Px(theme.spacing.xs * 0.5f)));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
    ImGui::BeginChild("##navigation", ImVec2(width, 0.0f), ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor();

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    // Kept between frames so its channel buffers are reused instead of allocated every frame.
    static ImDrawListSplitter splitter;
    splitter.Split(drawList, 2);
    splitter.SetCurrentChannel(drawList, 1);

    // Rows draw their own hover and selection, so the stock highlight is turned off.
    const ImVec4 clear = facet::WithAlpha(c.bg, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_Header, clear);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, clear);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, clear);

    const float windowTop = ImGui::GetWindowPos().y;
    float selectedY = -1.0f;
    float rowLeft = 0.0f;
    float rowRight = 0.0f;
    const std::string* currentGroup = nullptr;
    char probeName[96];
    for (int i = 0; i < static_cast<int>(pages.size()); ++i)
    {
        const PageInfo& page = pages[static_cast<size_t>(i)];
        if (currentGroup == nullptr || *currentGroup != page.group)
        {
            if (currentGroup != nullptr)
                ImGui::Dummy(ImVec2(0.0f, facet::Px(theme.spacing.md)));
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + rowInset);
            ImGui::PushStyleColor(ImGuiCol_Text, c.labelSecondary);
            facet::Text(facet::TextStyle::Footnote, "%s", page.group.c_str());
            ImGui::PopStyleColor();
            ImGui::Dummy(ImVec2(0.0f, facet::Px(theme.spacing.xs)));
            currentGroup = &page.group;
        }

        const bool selected = state.selectedPage == i;
        ImGui::PushID(page.id.c_str());
        const ImVec2 rowMin = ImGui::GetCursorScreenPos();
        if (ImGui::Selectable("##page", selected, ImGuiSelectableFlags_None, ImVec2(0.0f, rowHeight)))
            state.selectedPage = i;
        facet::ItemCursor();
        std::snprintf(probeName, sizeof(probeName), "nav.%s", page.id.c_str());
        ctx.Probe(probeName);
        // The item rectangle includes half the item spacing above and below, so rows can be
        // hit without gaps. The visible row (hover fill, selection pill and title) is the
        // rowHeight box centered in it.
        const ImVec2 itemMin = ImGui::GetItemRectMin();
        const ImVec2 itemMax = ImGui::GetItemRectMax();
        rowLeft = itemMin.x;
        rowRight = itemMax.x;
        const float rowTop = facet::Snap(itemMin.y + (itemMax.y - itemMin.y - rowHeight) * 0.5f);
        if (ImGui::IsItemHovered() && !selected)
        {
            const ImU32 hover = Color(ImGui::IsItemActive() ? c.fill[1] : c.fill[2]);
            facet::FillRect(drawList, ImVec2(rowLeft, rowTop), ImVec2(rowRight, rowTop + rowHeight), hover, radius);
        }
        if (selected)
            selectedY = rowTop - windowTop + ImGui::GetScrollY();

        // Titles that do not fit the row end in an ellipsis. Their capitals sit centered in
        // the row box.
        facet::PushTextStyle(facet::TextStyle::Body);
        const float textY = facet::CenteredTextY(rowTop, rowHeight);
        char title[128];
        facet::internal::TruncateText(ImGui::GetFont(), ImGui::GetFontSize(), page.title.c_str(), nullptr,
                                      itemMax.x - rowMin.x - rowInset * 2.0f, facet::internal::TruncateMode::End,
                                      title, sizeof(title));
        drawList->AddText(ImVec2(rowMin.x + rowInset, textY), Color(c.label), title);
        facet::PopTextStyle();
        ImGui::PopID();
    }
    ImGui::PopStyleColor(3);

    if (pages.empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, c.labelSecondary);
        ImGui::TextWrapped("No pages registered.");
        ImGui::PopStyleColor();
    }

    // The selection pill, under the rows. Its position is kept in content coordinates so
    // scrolling the list does not animate it.
    splitter.SetCurrentChannel(drawList, 0);
    if (selectedY >= 0.0f)
    {
        const float y = facet::Spring(ImGui::GetID("##selection"), kPillY, selectedY, theme.motion.standard);
        const float top = windowTop - ImGui::GetScrollY() + y;
        facet::FillRect(drawList, ImVec2(rowLeft, top), ImVec2(rowRight, top + rowHeight), Color(c.accentSubtle),
                        radius);
    }
    splitter.Merge(drawList);
    ImGui::EndChild();
}

void DrawContent(GalleryState& state, PageCtx& ctx)
{
    const facet::Theme& theme = facet::T();
    const std::span<const PageInfo> pages = Pages();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, facet::Px(ImVec2(theme.spacing.xxl, theme.spacing.xl)));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
    ImGui::BeginChild("##content", ImVec2(0.0f, 0.0f), ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PopStyleVar(2);
    if (state.selectedPage >= 0 && state.selectedPage < static_cast<int>(pages.size()))
    {
        const PageInfo& page = pages[static_cast<size_t>(state.selectedPage)];
        ImGui::PushID(page.id.c_str());
        page.draw(ctx);
        ImGui::PopID();
    }
    else
    {
        Paragraph("Select a page on the left.");
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
    ApplyGalleryLook(state, ctx);
    ctx.dark = facet::T().dark;
    ctx.scale = state.baseScale * state.uiScale;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::Begin("##gallery", nullptr, flags);
    ImGui::PopStyleVar(3);

    // The three areas are placed explicitly, edge to edge.
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float stripHeight = DrawTitleStrip(state, ctx, shell);
    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + stripHeight));
    DrawNavigation(state, ctx);
    const float navRight = ImGui::GetItemRectMax().x;
    // Hairline between the navigation and the content.
    const float top = origin.y + stripHeight;
    const float bottom = ImGui::GetWindowPos().y + ImGui::GetWindowHeight();
    facet::HairlineV(ImGui::GetWindowDrawList(), navRight, top, bottom, Color(facet::T().colors.separator));
    ImGui::SetCursorScreenPos(ImVec2(navRight + facet::HairlineThickness(), top));
    DrawContent(state, ctx);

    ImGui::End();
}

void PageHeader(const char* title, const char* summary)
{
    facet::Text(facet::TextStyle::Title1, "%s", title);
    ImGui::Dummy(ImVec2(0.0f, facet::Px(facet::T().spacing.xs)));
    Paragraph(summary);
}

void SectionHeader(const char* title)
{
    const facet::Theme& theme = facet::T();
    ImGui::Dummy(ImVec2(0.0f, facet::Px(theme.spacing.xl)));
    facet::Text(facet::TextStyle::Title3, "%s", title);
    ImGui::Dummy(ImVec2(0.0f, facet::Px(theme.spacing.xs)));
}

void Paragraph(const char* text)
{
    facet::PushTextStyle(facet::TextStyle::Body);
    ImGui::PushStyleColor(ImGuiCol_Text, facet::T().colors.labelSecondary);
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
    facet::PopTextStyle();
}

float ContentRight()
{
    return ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
}

bool SameLineIfFits(float right, float nextWidth, float spacing)
{
    if (spacing < 0.0f)
        spacing = ImGui::GetStyle().ItemSpacing.x;
    if (ImGui::GetItemRectMax().x + spacing + nextWidth > right)
        return false;
    ImGui::SameLine(0.0f, spacing);
    return true;
}

float ButtonWidth(const char* label)
{
    return ImGui::CalcTextSize(label, nullptr, true).x + ImGui::GetStyle().FramePadding.x * 2.0f;
}

float CheckboxWidth(const char* label)
{
    return ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x + ImGui::CalcTextSize(label, nullptr, true).x;
}

void LabeledRow(const char* label)
{
    const facet::Theme& theme = facet::T();
    const float column = facet::Px(theme.spacing.xxxl * kLabelColumnInXxxl);
    const float minControl = facet::Px(theme.spacing.xxxl * kMinRowControlInXxxl);
    const bool beside = ImGui::GetContentRegionAvail().x >= column + minControl;
    const float height = beside ? ImGui::GetFrameHeight() : facet::Px(theme.type[facet::TextStyle::Footnote].size);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float startX = ImGui::GetCursorPosX();
    ImGui::Dummy(ImVec2(beside ? column : ImGui::GetContentRegionAvail().x, height));
    facet::PushTextStyle(facet::TextStyle::Footnote);
    const float y = beside ? facet::CenteredTextY(pos.y, height) : pos.y;
    // Clipped to the column, so a long label never runs under the control.
    const ImVec2 clipMax(beside ? pos.x + column - facet::Px(theme.spacing.sm) : pos.x + ImGui::GetContentRegionAvail().x,
                         pos.y + height);
    ImGui::GetWindowDrawList()->PushClipRect(pos, clipMax, true);
    ImGui::GetWindowDrawList()->AddText(ImVec2(pos.x, y), Color(theme.colors.labelSecondary), label);
    ImGui::GetWindowDrawList()->PopClipRect();
    facet::PopTextStyle();
    if (beside)
        ImGui::SameLine(startX + column, 0.0f);
}

void Note(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    ImGui::PushStyleColor(ImGuiCol_Text, facet::T().colors.labelSecondary);
    facet::PushTextStyle(facet::TextStyle::Footnote);
    ImGui::TextWrappedV(fmt, args);
    facet::PopTextStyle();
    ImGui::PopStyleColor();
    va_end(args);
}

} // namespace facet::demo
