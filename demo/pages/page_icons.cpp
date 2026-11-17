// Icons page: every registered icon in a searchable grid, each at 16, 20 and 24 design
// units, with its name. The tint switches between the label color and the accent text
// color. A search without a match shows an empty state with a way back.

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string_view>

#include <imgui.h>

#include <facet/facet.h>

#include "gallery.h"
#include "pages/pages.h"

namespace
{

struct IconsState
{
    char search[64] = "";
    bool accentTint = false;
};

IconsState icons;

// Filled in while drawing, read by the selftest.
int shownCount = 0;
bool emptyStateShown = false;

// Icon sizes shown in every tile, in design units: the text-sized glyph and the two
// common toolbar sizes. They are the spacing tokens lg, xl and xxl.
float SampleSize(const facet::Spacing& spacing, int index)
{
    const float sizes[] = {spacing.lg, spacing.xl, spacing.xxl};
    return sizes[index];
}
constexpr int kSampleCount = 3;

// Width of the search field in multiples of the font size: room for a long icon name.
constexpr float kSearchWidthEm = 20.0f;

bool ContainsNoCase(std::string_view text, std::string_view query)
{
    if (query.empty())
        return true;
    if (query.size() > text.size())
        return false;
    for (std::size_t start = 0; start + query.size() <= text.size(); ++start)
    {
        bool match = true;
        for (std::size_t i = 0; i < query.size() && match; ++i)
        {
            match = std::tolower(static_cast<unsigned char>(text[start + i])) ==
                    std::tolower(static_cast<unsigned char>(query[i]));
        }
        if (match)
            return true;
    }
    return false;
}

// Trims spaces around the query so a stray space does not hide every icon.
std::string_view Query()
{
    std::string_view query(icons.search);
    while (!query.empty() && query.front() == ' ')
        query.remove_prefix(1);
    while (!query.empty() && query.back() == ' ')
        query.remove_suffix(1);
    return query;
}

void DrawEmptyState(const facet::Theme& theme, const facet::demo::PageCtx& ctx)
{
    emptyStateShown = true;
    const facet::Colors& c = theme.colors;
    const float padding = facet::Px(theme.spacing.xxl);
    ImGui::Dummy(ImVec2(0.0f, padding));
    facet::IconGlyph(facet::Icon("search"), facet::Px(theme.spacing.xxxl), facet::ToU32(c.labelSecondary));
    facet::Text(facet::TextStyle::Headline, "No icons match \"%s\"", icons.search);
    facet::demo::Paragraph("Icon names are lowercase words joined by hyphens, such as chevron-down.");
    if (ImGui::Button("Clear search"))
        icons.search[0] = '\0';
    ctx.Probe("icons.clearSearch");
}

void DrawGrid(const facet::Theme& theme, std::string_view query)
{
    const facet::Colors& c = theme.colors;
    const float padding = facet::Px(theme.spacing.sm);
    const float gap = facet::Px(theme.spacing.sm);
    const float radius = facet::Px(theme.radii.card);
    const facet::TextStyleSpec& nameStyle = theme.type[facet::TextStyle::Footnote];
    ImFont* font = ImGui::GetFont();
    const float nameSize = facet::Px(nameStyle.size);

    // Tile width: the widest name or the row of samples, whichever is larger.
    float samplesWidth = 0.0f;
    float sampleHeight = 0.0f;
    for (int i = 0; i < kSampleCount; ++i)
    {
        const float side = std::round(facet::Px(SampleSize(theme.spacing, i)));
        samplesWidth += side + (i > 0 ? gap : 0.0f);
        sampleHeight = std::max(sampleHeight, side);
    }
    float nameWidth = 0.0f;
    const int count = facet::IconCount();
    for (int i = 0; i < count; ++i)
        nameWidth = std::max(nameWidth, font->CalcTextSizeA(nameSize, FLT_MAX, 0.0f, facet::IconNameAt(i)).x);
    const ImVec2 tile(std::ceil(std::max(samplesWidth, nameWidth) + 2.0f * padding),
                      std::ceil(sampleHeight + gap + nameSize + 2.0f * padding));

    const ImU32 tint = facet::ToU32(icons.accentTint ? c.accentText : c.label);
    const ImU32 tileColor = facet::ToU32(c.fill[3]);
    const ImU32 nameColor = facet::ToU32(c.labelSecondary);
    const float rightEdge = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    shownCount = 0;
    for (int i = 0; i < count; ++i)
    {
        const char* name = facet::IconNameAt(i);
        if (!ContainsNoCase(name, query))
            continue;
        if (shownCount > 0)
        {
            ImGui::SameLine(0.0f, gap);
            if (ImGui::GetCursorScreenPos().x + tile.x > rightEdge)
                ImGui::NewLine();
        }
        ++shownCount;

        ImGui::Dummy(tile);
        if (!ImGui::IsItemVisible())
            continue;
        const ImVec2 min = ImGui::GetItemRectMin();
        facet::FillRect(drawList, min, ImVec2(min.x + tile.x, min.y + tile.y), tileColor, radius);

        // Samples share a bottom line, so the different sizes compare like text glyphs.
        float x = min.x + std::floor(0.5f * (tile.x - samplesWidth));
        const float baseline = min.y + padding + sampleHeight;
        const facet::Icon icon = facet::FindIcon(name);
        for (int s = 0; s < kSampleCount; ++s)
        {
            const float side = std::round(facet::Px(SampleSize(theme.spacing, s)));
            facet::DrawIcon(drawList, icon, ImVec2(x, baseline - side), side, tint);
            x += side + gap;
        }
        const float textWidth = font->CalcTextSizeA(nameSize, FLT_MAX, 0.0f, name).x;
        drawList->AddText(font, nameSize,
                          ImVec2(std::floor(min.x + 0.5f * (tile.x - textWidth)), baseline + gap), nameColor, name);
    }
}

} // namespace

FACET_DEMO_PAGE("icons", "Icons", "Media", 10, "default", "search", "empty")
{
    const facet::Theme& theme = facet::T();
    if (ctx.capture)
    {
        static constexpr const char* kCaptureQueries[] = {"", "arrow", "rocket"};
        std::snprintf(icons.search, sizeof(icons.search), "%s", kCaptureQueries[ctx.captureState]);
        icons.accentTint = ctx.captureState == 1;
    }

    facet::demo::PageHeader("Icons", "The built-in glyph set: 16 unit grid, 1.5 unit strokes with round ends, "
                                     "drawn in the label color unless a tint is given. Each tile shows the icon "
                                     "at 16, 20 and 24 units, rasterized at the exact pixel size of the current "
                                     "scale.");
    facet::demo::SectionHeader("Glyph set");

    // Search field with its icon in front, like a labelled control.
    ImGui::AlignTextToFramePadding();
    facet::IconGlyph(facet::Icon("search"), 0.0f, facet::ToU32(theme.colors.labelSecondary));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(std::min(ImGui::GetContentRegionAvail().x, ImGui::GetFontSize() * kSearchWidthEm));
    ImGui::InputTextWithHint("##search", "Search icons by name", icons.search, sizeof(icons.search));
    ctx.Probe("icons.search");
    // The option follows the field when it fits, otherwise it starts the next line.
    const ImGuiStyle& style = ImGui::GetStyle();
    const float checkboxWidth = ImGui::GetFrameHeight() + style.ItemInnerSpacing.x +
                                ImGui::CalcTextSize("Accent tint").x;
    ImGui::SameLine();
    if (ImGui::GetContentRegionAvail().x < checkboxWidth)
        ImGui::NewLine();
    ImGui::Checkbox("Accent tint", &icons.accentTint);
    ctx.Probe("icons.tint");

    const std::string_view query = Query();
    emptyStateShown = false;
    int matches = 0;
    for (int i = 0; i < facet::IconCount(); ++i)
        matches += ContainsNoCase(facet::IconNameAt(i), query) ? 1 : 0;
    facet::demo::Note("%d of %d icons", matches, facet::IconCount());
    ImGui::Spacing();

    if (matches == 0)
    {
        shownCount = 0;
        DrawEmptyState(theme, ctx);
        return;
    }
    DrawGrid(theme, query);
}

FACET_DEMO_SELFTEST("icons")
{
    test.Do([] { icons = IconsState{}; });
    test.Expect("without a search every icon is shown", [] { return shownCount == facet::IconCount(); });

    test.Click("icons.search");
    test.Do([] { ImGui::GetIO().AddInputCharactersUTF8("arrow"); });
    test.Wait(2);
    test.Expect("typing \"arrow\" leaves the four arrows", [] { return shownCount == 4; });

    test.Do([] { ImGui::GetIO().AddInputCharactersUTF8("xyz"); });
    test.Wait(2);
    test.Expect("a search without a match shows the empty state",
                [] { return emptyStateShown && shownCount == 0; });

    test.Click("icons.clearSearch");
    test.Expect("Clear search brings every icon back",
                [] { return icons.search[0] == '\0' && shownCount == facet::IconCount(); });

    test.Click("icons.tint");
    test.Expect("clicking Accent tint turns it on", [] { return icons.accentTint; });
    // After mouse input the first Tab shows the keyboard cursor on the focused item.
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("icons.tint");
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Space on the focused checkbox turns it off", [] { return !icons.accentTint; });
    test.KeyPress(ImGuiKey_Tab | ImGuiMod_Shift);
    test.ExpectFocused("icons.search");
}
