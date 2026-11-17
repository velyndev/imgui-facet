#include "gallery.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <iterator>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "app_shell.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/multi_handle.h"
#include "internal/selection_pill.h"
#include "internal/text_util.h"
#include "internal/value_drag.h"

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

// Prototype controls: the knob of a slider grows by this share while it is held, so a press
// shows on the knob under the pointer.
constexpr float kKnobPressGrowth = 0.15f;
// The selection pill of a segmented control sits this share of spacing.xs inside its segment.
constexpr float kSegmentInsetShare = 0.5f;

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
        facet::internal::TruncateText(ImGui::GetFont(), ImGui::GetFontSize(), page.title.c_str(), nullptr,
                                      row.rect.GetWidth() - rowInset * 2.0f, facet::internal::TruncateMode::End, title,
                                      sizeof(title));
        drawList->AddText(ImVec2(row.rect.Min.x + rowInset, textY), Color(c.label), title);
        facet::PopTextStyle();
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
    const facet::LabeledRowLayout row = facet::LayoutLabeledRow(ImGui::GetContentRegionAvail().x);
    const float height = row.stacked ? facet::Px(theme.type[facet::TextStyle::Footnote].size) : ImGui::GetFrameHeight();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float startX = ImGui::GetCursorPosX();
    ImGui::Dummy(ImVec2(row.stacked ? row.labelWidth : row.controlOffset, height));
    facet::PushTextStyle(facet::TextStyle::Footnote);
    const float y = row.stacked ? pos.y : facet::CenteredTextY(pos.y, height);
    // Clipped to the label column, so a long label never runs under the control.
    const ImVec2 clipMax(pos.x + row.labelWidth, pos.y + height);
    ImGui::GetWindowDrawList()->PushClipRect(pos, clipMax, true);
    ImGui::GetWindowDrawList()->AddText(ImVec2(pos.x, y), Color(theme.colors.labelSecondary), label);
    ImGui::GetWindowDrawList()->PopClipRect();
    facet::PopTextStyle();
    if (!row.stacked)
        ImGui::SameLine(startX + row.controlOffset, 0.0f);
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

// ---- Prototype controls ---------------------------------------------------------------------

void ProbeRect(PageCtx& ctx, const char* name, ImGuiID id, ImVec2 min, ImVec2 max)
{
    if (ctx.probes == nullptr)
        return;
    ImGuiContext& g = *GImGui;
    const ImGuiLastItemData backup = g.LastItemData;
    g.LastItemData.ID = id;
    g.LastItemData.Rect = ImRect(min, max);
    g.LastItemData.NavRect = ImRect(min, max);
    ctx.Probe(name);
    g.LastItemData = backup;
}

namespace
{

// The fill of a control background from the hover and press amounts.
ImVec4 ControlFill(const facet::internal::ItemState& item)
{
    const facet::Colors& c = facet::T().colors;
    if (item.disabled)
        return c.fill[3];
    const ImVec4 hovered = facet::Mix(c.fill[2], c.fill[1], item.hoverAmount);
    return facet::Mix(hovered, c.fill[0], item.pressAmount);
}

// The line a knob centre travels along inside a slider rectangle: inset by half a knob at
// both ends, so the knob stays inside.
ImRect KnobTrack(const ImRect& rect, facet::Orientation orientation, float knob)
{
    const float half = knob * 0.5f;
    if (orientation == facet::Orientation::Vertical)
    {
        const float x = rect.GetCenter().x;
        return ImRect(x, rect.Min.y + half, x, rect.Max.y - half);
    }
    const float y = rect.GetCenter().y;
    return ImRect(rect.Min.x + half, y, rect.Max.x - half, y);
}

// The slider rectangle at the cursor.
ImRect SliderRect(const ProtoSliderOptions& options)
{
    const facet::Theme& t = facet::T();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float height = facet::ControlHeight(facet::ControlSize::Regular);
    if (options.orientation == facet::Orientation::Vertical)
    {
        const float length = facet::Px(options.length > 0.0f ? options.length : t.spacing.xxxl * 5.0f);
        return ImRect(pos, ImVec2(pos.x + height, pos.y + length));
    }
    // The width precedence of the API conventions: options, SetNextItemWidth, PushItemWidth (a
    // labelled row pushes its slot), then the available width.
    const float length = std::max(height, facet::internal::ResolveItemWidth(options.length, facet::internal::TakeNextItemData(),
                                                                            facet::kFill));
    return ImRect(pos, ImVec2(pos.x + length, pos.y + height));
}

// How a track looks besides its normal state.
enum class TrackLook
{
    Normal,
    ReadOnly,  // the fill in labelSecondary: shown, not editable
    Disabled,  // everything in the disabled tokens
};

// The track line, the filled part from `fillFrom` to `fillTo` (fractions), step ticks and snap
// stops.
void DrawTrack(ImDrawList* drawList, const ImRect& track, facet::Orientation orientation, double fillFrom,
               double fillTo, TrackLook look, const facet::internal::ValueMapping& mapping)
{
    const bool disabled = look == TrackLook::Disabled;
    const facet::Theme& t = facet::T();
    const facet::Colors& c = t.colors;
    const float thickness = facet::PxSnapped(t.metrics.sliderTrackHeight);
    const float half = thickness * 0.5f;
    const bool vertical = orientation == facet::Orientation::Vertical;
    const auto lineRect = [&](float a, float b) {
        if (vertical)
            return ImRect(track.Min.x - half, std::min(a, b), track.Min.x + half, std::max(a, b));
        return ImRect(std::min(a, b), track.Min.y - half, std::max(a, b), track.Min.y + half);
    };
    const float start = vertical ? track.Max.y : track.Min.x;
    const float end = vertical ? track.Min.y : track.Max.x;
    const ImRect whole = lineRect(start - (vertical ? -half : half), end + (vertical ? -half : half));
    facet::FillRect(drawList, whole.Min, whole.Max, Color(disabled ? c.fill[3] : c.fill[0]), half);
    const float a = facet::internal::FractionToTrack(track, orientation, fillFrom);
    const float b = facet::internal::FractionToTrack(track, orientation, fillTo);
    if (a != b)
    {
        const ImRect fill = lineRect(a, b);
        const ImVec4& color = disabled ? c.labelTertiary : (look == TrackLook::ReadOnly ? c.labelSecondary : c.accent);
        facet::FillRect(drawList, fill.Min, fill.Max, Color(color), half);
    }
    // Ticks for a short list of steps and for snap stops, so a slider shows where it can stop.
    const facet::internal::ValueMapping m = facet::internal::NormalizeMapping(mapping);
    const float dot = thickness * 0.5f;
    const ImU32 tick = Color(disabled ? c.labelTertiary : c.labelSecondary);
    const auto drawTick = [&](double value) {
        const float at = facet::internal::FractionToTrack(track, orientation, facet::internal::ValueToFraction(m, value));
        const ImVec2 center = vertical ? ImVec2(track.Min.x + thickness * 2.0f, at) : ImVec2(at, track.Min.y + thickness * 2.0f);
        drawList->AddCircleFilled(center, dot, tick);
    };
    if (m.step > 0.0 && m.max > m.min && (m.max - m.min) / m.step <= 20.0)
    {
        for (double v = m.min; v <= m.max + m.step * 0.5; v += m.step)
            drawTick(v);
    }
    for (const double stop : m.stops)
    {
        if (stop >= m.min && stop <= m.max)
            drawTick(stop);
    }
}

// A knob: the background color with a 3:1 outline, a little larger while held.
void DrawKnob(ImDrawList* drawList, const ImRect& rect, const facet::internal::ItemState& item, bool disabled)
{
    const facet::Colors& c = facet::T().colors;
    const ImVec2 center = rect.GetCenter();
    const float radius = rect.GetWidth() * 0.5f * (1.0f + kKnobPressGrowth * item.pressAmount);
    const ImVec4 boundary = facet::ControlBoundaryColor(c);
    const ImVec4 outline = disabled ? c.labelTertiary : facet::Mix(boundary, c.label, item.hoverAmount * 0.5f);
    drawList->AddCircleFilled(center, radius, Color(c.bg));
    drawList->AddCircle(center, radius, Color(outline), 0, facet::HairlineThickness() * 1.5f);
    // Round, like the knob, so the ring keeps an even gap all around it.
    facet::internal::DrawFocusRing(drawList, rect, rect.GetWidth() * 0.5f, item.focusAmount, 0,
                                   facet::internal::RingShape::Round);
}

} // namespace

bool ProtoButton(PageCtx& ctx, const char* label, const char* probe, bool repeat)
{
    const facet::Theme& t = facet::T();
    const facet::Colors& c = t.colors;
    const char* end = ImGui::FindRenderedTextEnd(label);
    facet::PushTextStyle(facet::TextStyle::Body);
    const float padding = facet::Px(t.spacing.md);
    const float height = facet::ControlHeight(facet::ControlSize::Regular);
    const ImVec2 size(ImGui::CalcTextSize(label, end).x + padding * 2.0f, height);
    facet::internal::ItemOptions options;
    options.repeat = repeat;
    const facet::internal::ItemState item = facet::internal::ItemBehavior(ImGui::GetID(label), size, options);
    ctx.Probe(probe);
    if (item.visible)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const float radius = facet::Px(t.radii.control);
        facet::FillRect(drawList, item.rect.Min, item.rect.Max, Color(ControlFill(item)), radius);
        facet::StrokeRect(drawList, item.rect.Min, item.rect.Max,
                          Color(item.disabled ? c.separator : facet::ControlBoundaryColor(c)), radius,
                          facet::HairlineThickness());
        drawList->AddText(ImVec2(item.rect.Min.x + padding, facet::CenteredTextY(item.rect.Min.y, height)),
                          Color(item.disabled ? c.labelTertiary : c.label), label, end);
        facet::internal::DrawItemFocusRing(item, radius);
    }
    facet::PopTextStyle();
    return item.pressed;
}

bool ProtoToggle(PageCtx& ctx, const char* label, bool* value, const char* probe)
{
    const facet::Theme& t = facet::T();
    const facet::Colors& c = t.colors;
    const char* end = ImGui::FindRenderedTextEnd(label);
    facet::PushTextStyle(facet::TextStyle::Body);
    const float gap = facet::Px(t.spacing.md);
    const float height = facet::ControlHeight(facet::ControlSize::Regular);
    const ImVec2 track(facet::Px(t.metrics.toggleWidth), facet::Px(t.metrics.toggleHeight));
    const float textWidth = ImGui::CalcTextSize(label, end).x;
    const ImVec2 size(textWidth + (textWidth > 0.0f ? gap : 0.0f) + track.x, height);
    const ImGuiID id = ImGui::GetID(label);
    const facet::internal::ItemState item = facet::internal::ItemBehavior(id, size);
    ctx.Probe(probe);
    bool written = false;
    if (item.pressed && value != nullptr && !item.readOnly)
    {
        *value = !*value;
        written = true;
        ImGui::MarkItemEdited(id);
    }
    const bool on = value != nullptr && *value;
    const float knobT = facet::Spring(id, 0, on ? 1.0f : 0.0f, t.motion.quick);
    if (item.visible)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddText(ImVec2(item.rect.Min.x, facet::CenteredTextY(item.rect.Min.y, height)),
                          Color(item.disabled ? c.labelTertiary : c.label), label, end);
        const ImVec2 min(item.rect.Max.x - track.x, item.rect.Min.y + (height - track.y) * 0.5f);
        const ImVec2 max(item.rect.Max.x, min.y + track.y);
        const float radius = track.y * 0.5f;
        const ImVec4 off = item.disabled ? c.fill[3] : ControlFill(item);
        const ImVec4 trackColor = item.disabled ? c.fill[3] : facet::Mix(off, c.accent, knobT);
        facet::FillRect(drawList, min, max, Color(trackColor), radius);
        if (knobT < 1.0f)
            facet::StrokeRect(drawList, min, max,
                              Color(facet::WithAlpha(facet::ControlBoundaryColor(c), 1.0f - knobT)), radius,
                              facet::HairlineThickness());
        const float inset = facet::Px(t.metrics.toggleKnobInset);
        const float knob = track.y - inset * 2.0f;
        const float x = min.x + inset + knob * 0.5f + knobT * (track.x - inset * 2.0f - knob);
        const ImVec4 knobColor = item.disabled ? c.labelTertiary : facet::Mix(c.labelSecondary, c.onAccent, knobT);
        drawList->AddCircleFilled(ImVec2(x, (min.y + max.y) * 0.5f), knob * 0.5f, Color(knobColor));
        facet::internal::DrawFocusRing(drawList, ImRect(min, max), radius, item.focusAmount);
    }
    facet::PopTextStyle();
    return written;
}

bool ProtoSlider(PageCtx& ctx, const char* id, double* value, const ProtoSliderOptions& options, const char* probe)
{
    const facet::Theme& t = facet::T();
    const ImRect rect = SliderRect(options);
    const float knob = facet::Px(t.metrics.sliderKnobSize);
    const ImRect track = KnobTrack(rect, options.orientation, knob);
    facet::internal::ItemOptions itemOptions;
    itemOptions.press = facet::internal::PressMode::Click;
    itemOptions.readOnly = options.readOnly;
    const ImGuiID itemId = ImGui::GetID(id);
    const facet::internal::ItemState item = facet::internal::ItemBehavior(itemId, rect.GetSize(), itemOptions);
    ctx.Probe(probe);
    char name[96];
    std::snprintf(name, sizeof(name), "%s.track", probe);
    const bool vertical = options.orientation == facet::Orientation::Vertical;
    ProbeRect(ctx, name, itemId, vertical ? ImVec2(rect.Min.x, track.Min.y) : ImVec2(track.Min.x, rect.Min.y),
              vertical ? ImVec2(rect.Max.x, track.Max.y) : ImVec2(track.Max.x, rect.Max.y));

    facet::internal::ValueDragOptions drag;
    drag.mapping = {.min = options.min, .max = options.max, .scale = options.scale, .step = options.step,
                    .integer = options.integer, .stops = options.stops};
    drag.orientation = options.orientation;
    drag.grabRadius = knob * 0.5f;
    drag.readOnly = options.readOnly;
    double edited = value != nullptr ? *value : options.min;
    const facet::internal::ValueDragResult result = facet::internal::ValueDragBehavior(item, &edited, track, drag);
    if (result.changed && value != nullptr)
        *value = edited;

    if (item.visible)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const double fraction = facet::internal::ValueToFraction(drag.mapping, edited);
        const TrackLook look = item.disabled ? TrackLook::Disabled
                               : (options.readOnly || item.readOnly) ? TrackLook::ReadOnly
                                                                     : TrackLook::Normal;
        DrawTrack(drawList, track, options.orientation, 0.0, fraction, look, drag.mapping);
        const ImRect knobRect = facet::internal::HandleRect(track, options.orientation, drag.mapping, edited, knob);
        DrawKnob(drawList, knobRect, item, item.disabled);
    }
    return result.changed;
}

bool ProtoRange(PageCtx& ctx, const char* id, double* values, int count, const ProtoRangeOptions& options,
                const char* probe, int* removeRequested)
{
    if (removeRequested != nullptr)
        *removeRequested = -1;
    count = std::clamp(count, 0, facet::internal::kMaxHandles);
    if (values == nullptr || count == 0)
        return false;
    const facet::Theme& t = facet::T();
    const ProtoSliderOptions& slider = options.slider;
    const ImRect rect = SliderRect(slider);
    const float knob = facet::Px(t.metrics.sliderKnobSize);
    const ImRect track = KnobTrack(rect, slider.orientation, knob);
    const ImGuiID widgetId = ImGui::GetID(id);

    facet::internal::MultiHandleOptions multi;
    multi.drag.mapping = {.min = slider.min, .max = slider.max, .scale = slider.scale, .step = slider.step,
                          .integer = slider.integer, .stops = slider.stops};
    multi.drag.orientation = slider.orientation;
    multi.drag.readOnly = slider.readOnly;
    multi.constraints.minGap = options.minGap;
    multi.constraints.collision = options.push ? facet::internal::CollisionMode::Push : facet::internal::CollisionMode::Clamp;
    multi.constraints.mirror = options.mirror;
    multi.constraints.center = (slider.min + slider.max) * 0.5;
    multi.handleSize = knob;
    multi.spanDrag = options.spanDrag;
    multi.removeKeys = options.removeKeys;
    multi.keys = options.keys;

    facet::internal::ItemState handles[facet::internal::kMaxHandles];
    ImGui::BeginGroup();
    const facet::internal::MultiHandleResult result = facet::internal::MultiHandleBehavior(
        widgetId, rect, track, std::span<double>(values, static_cast<size_t>(count)),
        std::span<facet::internal::ItemState>(handles, static_cast<size_t>(count)), multi);
    char name[96];
    const bool vertical = slider.orientation == facet::Orientation::Vertical;
    std::snprintf(name, sizeof(name), "%s.track", probe);
    ProbeRect(ctx, name, result.track.id, vertical ? ImVec2(rect.Min.x, track.Min.y) : ImVec2(track.Min.x, rect.Min.y),
              vertical ? ImVec2(rect.Max.x, track.Max.y) : ImVec2(track.Max.x, rect.Max.y));
    for (int i = 0; i < count; ++i)
    {
        std::snprintf(name, sizeof(name), "%s.h%d", probe, i);
        ProbeRect(ctx, name, handles[i].id, handles[i].rect.Min, handles[i].rect.Max);
    }
    ImGui::EndGroup();
    facet::internal::FinishWidgetGroup(widgetId);
    ctx.Probe(probe);
    if (removeRequested != nullptr)
        *removeRequested = result.removeRequested;

    if (result.track.visible)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const facet::internal::ValueMapping& m = multi.drag.mapping;
        const double first = count > 1 ? facet::internal::ValueToFraction(m, values[0]) : 0.0;
        const double last = facet::internal::ValueToFraction(m, values[count - 1]);
        const TrackLook look = result.track.disabled ? TrackLook::Disabled
                               : (slider.readOnly || result.track.readOnly) ? TrackLook::ReadOnly
                                                                            : TrackLook::Normal;
        DrawTrack(drawList, track, slider.orientation, first, last, look, m);
        // The handle a press would take, or the one in use, is drawn on top.
        int top = result.activeHandle >= 0 ? result.activeHandle : result.focusedHandle;
        int order[facet::internal::kMaxHandles];
        facet::internal::HandleConstraints bounds = multi.constraints;
        bounds.min = facet::internal::NormalizeMapping(m).min;
        bounds.max = facet::internal::NormalizeMapping(m).max;
        facet::internal::HandleDrawOrder(std::span<const double>(values, static_cast<size_t>(count)), bounds, top,
                                         std::span<int>(order, static_cast<size_t>(count)));
        for (int k = 0; k < count; ++k)
        {
            const int i = order[k];
            const facet::internal::ItemState& handle = handles[i];
            const ImRect knobRect = facet::internal::HandleRect(track, slider.orientation, m, values[i], knob);
            DrawKnob(drawList, knobRect, handle, result.track.disabled);
        }
    }
    return result.changed;
}

bool ProtoSegmented(PageCtx& ctx, const char* id, int* index, std::span<const char* const> items, const char* probe,
                    float width)
{
    const facet::Theme& t = facet::T();
    const facet::Colors& c = t.colors;
    const int count = static_cast<int>(items.size());
    if (count == 0)
        return false;
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    const ImGuiID widgetId = ImGui::GetID(id);
    facet::PushTextStyle(facet::TextStyle::Body);
    const float padding = facet::Px(t.spacing.md);
    const float height = facet::ControlHeight(facet::ControlSize::Regular);
    float widest = 0.0f;
    for (const char* item : items)
        widest = std::max(widest, ImGui::CalcTextSize(item, ImGui::FindRenderedTextEnd(item)).x);
    const float total = width > 0.0f ? facet::Px(width) : (widest + padding * 2.0f) * static_cast<float>(count);
    const float segment = total / static_cast<float>(count);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImRect rect(pos, ImVec2(pos.x + total, pos.y + height));
    const float radius = facet::Px(t.radii.control);
    const float inset = facet::Px(t.spacing.xs) * kSegmentInsetShare;

    ImGui::BeginGroup();
    ImGui::ItemSize(rect.GetSize());
    ImDrawList* drawList = window->DrawList;
    facet::FillRect(drawList, rect.Min, rect.Max, Color(c.fill[3]), radius);
    facet::StrokeRect(drawList, rect.Min, rect.Max, Color(facet::ControlBoundaryColor(c)), radius,
                      facet::HairlineThickness());

    const facet::internal::ForcedItemRequest forced = facet::internal::TakeForcedItemState();
    facet::internal::SelectionPill pill;
    pill.Begin(widgetId, drawList, ImVec2(window->Pos.x - window->Scroll.x, window->Pos.y - window->Scroll.y));
    const int selected = index != nullptr ? *index : -1;
    int next = selected;
    int focusedSegment = -1;
    char name[96];
    ImGui::PushOverrideID(widgetId);
    for (int i = 0; i < count; ++i)
    {
        const ImRect segmentRect(rect.Min.x + segment * i, rect.Min.y, rect.Min.x + segment * (i + 1), rect.Max.y);
        facet::internal::ItemOptions options;
        options.part = i;
        options.forced = &forced;
        // One Tab stop: the selected segment (the first when none is).
        options.tabStop = i == (selected >= 0 && selected < count ? selected : 0);
        const ImGuiID segmentId = ImGui::GetID(i);
        const facet::internal::ItemState item = facet::internal::PartBehavior(segmentId, segmentRect, options);
        std::snprintf(name, sizeof(name), "%s.%d", probe, i);
        ctx.Probe(name);
        if (item.pressed)
            next = i;
        if (item.focused && g.NavId == segmentId && item.forced == facet::internal::ForcedItemState::None &&
            !item.disabled)
            focusedSegment = i;
        if (i == selected)
            pill.Target(ImRect(segmentRect.Min.x + inset, segmentRect.Min.y + inset, segmentRect.Max.x - inset,
                               segmentRect.Max.y - inset));
        if (item.visible)
        {
            const float feedback = i == selected ? 0.0f : std::max(item.hoverAmount, item.pressAmount);
            if (feedback > 0.0f)
            {
                const ImVec4 fill = facet::Mix(c.fill[2], c.fill[1], item.pressAmount);
                facet::FillRect(drawList, ImVec2(segmentRect.Min.x + inset, segmentRect.Min.y + inset),
                                ImVec2(segmentRect.Max.x - inset, segmentRect.Max.y - inset),
                                Color(facet::WithAlpha(fill, fill.w * feedback)), radius - inset);
            }
            const char* label = items[static_cast<size_t>(i)];
            const char* end = ImGui::FindRenderedTextEnd(label);
            facet::internal::PushPillLabelStyle(facet::TextStyle::Body, i == selected);
            const float textWidth = ImGui::CalcTextSize(label, end).x;
            drawList->AddText(ImVec2(std::round(segmentRect.GetCenter().x - textWidth * 0.5f),
                                     facet::CenteredTextY(segmentRect.Min.y, height)),
                              Color(item.disabled ? c.labelTertiary : c.label), label, end);
            facet::PopTextStyle();
            facet::internal::DrawFocusRing(drawList, segmentRect, radius, item.focusAmount);
        }
    }
    // The arrows move the selection and the focus along the segments, once per press: after
    // all segments ran, so the segment that receives the focus does not see the same press.
    if (focusedSegment >= 0)
    {
        const ImGuiID segmentId = ImGui::GetID(focusedSegment);
        ImGui::SetKeyOwner(ImGuiKey_LeftArrow, segmentId);
        ImGui::SetKeyOwner(ImGuiKey_RightArrow, segmentId);
        int step = 0;
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, ImGuiInputFlags_Repeat, segmentId))
            step = 1;
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, ImGuiInputFlags_Repeat, segmentId))
            step = -1;
        const int target = std::clamp(focusedSegment + step, 0, count - 1);
        if (step != 0 && target != focusedSegment)
        {
            next = target;
            ImGui::SetFocusID(ImGui::GetID(target), window);
            ImGui::SetNavCursorVisible(true);
        }
    }
    ImGui::PopID();
    pill.End(Color(c.accentSubtle), radius - inset);
    ImGui::EndGroup();
    facet::internal::FinishWidgetGroup(widgetId);
    ctx.Probe(probe);
    facet::PopTextStyle();

    const bool changed = index != nullptr && next != selected && next >= 0;
    if (changed)
    {
        *index = next;
        ImGui::MarkItemEdited(widgetId);
    }
    return changed;
}

} // namespace facet::demo
