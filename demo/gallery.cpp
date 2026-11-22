#include "gallery.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <string>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "app_shell.h"
#include "page_helpers.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/selection_pill.h"
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

// Most groups whose headings take part in pinning; later groups scroll like rows.
constexpr int kMaxPinnedGroups = 16;

// The page list: group headings and one row per page, in a list that scrolls when it is
// taller than the window. The selected row sits on the selection pill, which springs to the
// new row when the selection changes, and a newly selected page is scrolled into view. The
// heading of the group at the top stays pinned above the rows, so a scrolled list still
// shows which group its rows belong to; the next heading pushes it out as it arrives.
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
    // The pill animates relative to the list content, so scrolling moves it with its row.
    facet::internal::SelectionPill pill;
    pill.Begin(ImGui::GetID("##selection"), drawList,
               ImVec2(ImGui::GetWindowPos().x - ImGui::GetScrollX(), ImGui::GetWindowPos().y - ImGui::GetScrollY()));

    const ImRect inner = ImGui::GetCurrentWindowRead()->InnerClipRect;
    const float listTop = ImGui::GetCurrentWindowRead()->InnerRect.Min.y;
    const float spacingY = ImGui::GetStyle().ItemSpacing.y;
    const float headingHeight = facet::MeasureText(facet::TextStyle::Footnote, "A").y;
    // A heading block: the heading line, then a small gap before the first row.
    const float headingBlock = headingHeight + spacingY + facet::Px(theme.spacing.xs) + spacingY;
    // Where the first heading sits unscrolled, and the band it is pinned in. Rows are clipped
    // below the band, so a row scrolled under a pinned heading is neither drawn nor hit.
    const float pinnedY = listTop + padding.y;
    const float bandBottom = pinnedY + headingBlock;
    ImGui::PushClipRect(ImVec2(inner.Min.x, bandBottom), inner.Max, true);

    float headingY[kMaxPinnedGroups] = {};
    const std::string* headingText[kMaxPinnedGroups] = {};
    int groups = 0;
    float revealTop = 0.0f;
    float revealBottom = -1.0f;
    float focusedTop = 0.0f;
    bool rowFocused = false;
    const std::string* currentGroup = nullptr;
    char probeName[96];
    std::vector<unsigned char>& cuts = NavTitleCuts();
    if (cuts.size() != pages.size())
        cuts.assign(pages.size(), 0);
    for (int i = 0; i < static_cast<int>(pages.size()); ++i)
    {
        const PageInfo& page = pages[static_cast<size_t>(i)];
        if (currentGroup == nullptr || *currentGroup != page.group)
        {
            if (currentGroup != nullptr)
                ImGui::Dummy(ImVec2(0.0f, facet::Px(theme.spacing.md)));
            // Headings take their place here and are drawn after the rows (pinned or not);
            // beyond the pinned groups they are drawn in place.
            if (groups < kMaxPinnedGroups)
            {
                headingY[groups] = ImGui::GetCursorScreenPos().y;
                headingText[groups] = &page.group;
                ++groups;
            }
            else
            {
                facet::PushTextStyle(facet::TextStyle::Footnote);
                drawList->AddText(ImVec2(ImGui::GetCursorScreenPos().x + rowInset, ImGui::GetCursorScreenPos().y),
                                  Color(c.labelSecondary), page.group.c_str());
                facet::PopTextStyle();
            }
            ImGui::Dummy(ImVec2(0.0f, headingHeight));
            ImGui::Dummy(ImVec2(0.0f, facet::Px(theme.spacing.xs)));
            currentGroup = &page.group;
        }

        // A row: the item behaviour block, one control height tall across the list. The
        // keyboard moves between rows (Dear ImGui navigation); Enter, Space or a click selects.
        const bool selected = state.selectedPage == i;
        ImGui::PushID(page.id.c_str());
        const facet::internal::ItemState row =
            facet::internal::ItemBehavior(ImGui::GetID("##page"), ImVec2(ImGui::GetContentRegionAvail().x, rowHeight));
        if (row.pressed)
            state.selectedPage = i;
        std::snprintf(probeName, sizeof(probeName), "nav.%s", page.id.c_str());
        ctx.Probe(probeName);
        if (row.focused)
        {
            rowFocused = true;
            focusedTop = row.rect.Min.y;
        }
        if (selected && state.revealedPage != i)
        {
            revealTop = row.rect.Min.y;
            revealBottom = row.rect.Max.y;
        }
        if (selected)
            pill.Target(row.rect);
        // Hover and press feedback on rows that are not selected; the selected one has the pill.
        const float feedback = selected ? 0.0f : std::max(row.hoverAmount, row.pressAmount);
        if (row.visible && feedback > 0.0f)
        {
            const ImVec4 fill = facet::Mix(c.fill[2], c.fill[1], row.pressAmount);
            facet::FillRect(drawList, row.rect.Min, row.rect.Max, Color(facet::WithAlpha(fill, fill.w * feedback)), radius);
        }

        // Titles that do not fit the row end in an ellipsis. Their capitals sit centered in
        // the row box. The selected title is semibold, so the selection does not show by the
        // pill's tint alone.
        facet::internal::PushPillLabelStyle(facet::TextStyle::Body, selected);
        const float textY = facet::CenteredTextY(row.rect.Min.y, rowHeight);
        char title[128];
        const facet::internal::TruncateResult cut = facet::internal::TruncateText(
            ImGui::GetFont(), ImGui::GetFontSize(), page.title.c_str(), nullptr, row.rect.GetWidth() - rowInset * 2.0f,
            facet::internal::TruncateMode::End, title, sizeof(title));
        drawList->AddText(ImVec2(row.rect.Min.x + rowInset, textY), Color(c.label), title);
        facet::PopTextStyle();
        cuts[static_cast<size_t>(i)] = cut.truncated ? 1 : 0;
        // A cut title shows in full as a tooltip: beside the row while the keyboard focus is on
        // it, at the pointer while the pointer rests on it.
        if (cut.truncated && row.visible)
        {
            if (row.focusVisible)
            {
                ImGui::SetNextWindowPos(ImVec2(row.rect.Max.x + facet::Px(theme.spacing.xs), row.rect.Min.y));
                ImGui::SetTooltip("%s", page.title.c_str());
            }
            else if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
            {
                ImGui::SetTooltip("%s", page.title.c_str());
            }
        }
        facet::internal::DrawItemFocusRing(row, radius);
        ImGui::PopID();
    }

    if (pages.empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, c.labelSecondary);
        ImGui::TextWrapped("No pages registered.");
        ImGui::PopStyleColor();
    }

    // The selection pill, under the rows and clipped below the pinned headings like them.
    pill.End(Color(c.accentSubtle), radius);
    ImGui::PopClipRect();

    // Headings. The group whose heading has reached the band stays pinned there until the
    // next heading pushes it up; the headings below it scroll with their rows.
    int pinned = -1;
    for (int g = 0; g < groups; ++g)
    {
        if (headingY[g] <= pinnedY + 0.5f)
            pinned = g;
    }
    facet::PushTextStyle(facet::TextStyle::Footnote);
    const ImU32 headingColor = Color(c.labelSecondary);
    const float textX = inner.Min.x + rowInset;
    if (pinned >= 0)
    {
        float y = pinnedY;
        if (pinned + 1 < groups)
            y = std::min(y, headingY[pinned + 1] - headingBlock);
        drawList->AddText(ImVec2(textX, y), headingColor, headingText[pinned]->c_str());
    }
    for (int g = pinned + 1; g < groups; ++g)
    {
        if (headingY[g] < inner.Max.y)
            drawList->AddText(ImVec2(textX, headingY[g]), headingColor, headingText[g]->c_str());
    }
    facet::PopTextStyle();

    // A newly selected page (a click, the keyboard, --page or a capture run) is scrolled to
    // the middle of the list when it is not fully visible below the band; a row reached with
    // the keyboard is moved out from under the band. Both take effect in the next frame.
    // Until the row is visible the request repeats, because a list laid out for the first
    // time cannot scroll yet; a list too short for a whole row gives up at once.
    const float visibleBottom = inner.Max.y;
    if (revealBottom >= 0.0f)
    {
        const bool visible = revealTop >= bandBottom - 0.5f && revealBottom <= visibleBottom + 0.5f;
        if (!visible)
            ImGui::SetScrollY(ImGui::GetScrollY() + (revealTop + revealBottom) * 0.5f - (bandBottom + visibleBottom) * 0.5f);
        if (visible || visibleBottom - bandBottom < revealBottom - revealTop)
            state.revealedPage = state.selectedPage;
    }
    else if (rowFocused && focusedTop < bandBottom)
    {
        ImGui::SetScrollY(ImGui::GetScrollY() - (bandBottom - focusedTop));
    }
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

bool NavTitleCut(const char* pageId)
{
    const std::span<const PageInfo> pages = Pages();
    const std::vector<unsigned char>& cuts = NavTitleCuts();
    for (size_t i = 0; i < pages.size() && i < cuts.size(); ++i)
    {
        if (pages[i].id == pageId)
            return cuts[i] != 0;
    }
    return false;
}

} // namespace facet::demo
