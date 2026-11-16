// Type page: every text style in every weight, plus the text helpers: truncation at the end
// and in the middle, wrapping, and match highlighting.

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iterator>

#include <imgui.h>

#include <facet/facet.h>

#include "gallery.h"
#include "internal/text_util.h"
#include "pages/pages.h"

namespace
{

using facet::demo::Note;
using facet::demo::Paragraph;
using facet::demo::SectionHeader;

struct StyleName
{
    facet::TextStyle style;
    const char* name;
};

constexpr StyleName kStyles[] = {
    {facet::TextStyle::LargeTitle, "LargeTitle"}, {facet::TextStyle::Title1, "Title1"},
    {facet::TextStyle::Title2, "Title2"},         {facet::TextStyle::Title3, "Title3"},
    {facet::TextStyle::Headline, "Headline"},     {facet::TextStyle::Body, "Body"},
    {facet::TextStyle::Callout, "Callout"},       {facet::TextStyle::Footnote, "Footnote"},
    {facet::TextStyle::Caption, "Caption"},
};
static_assert(std::size(kStyles) == facet::kTextStyleCount);

constexpr const char* kWeightNames[] = {"Regular", "Semibold", "Bold"};
static_assert(std::size(kWeightNames) == facet::kFontWeightCount);

constexpr const char* kSample = "Quick brown fox 0123";
// A sample path for the truncation demo; it does not refer to a real file.
constexpr const char* kPlaceholderPath = "C:/Example/Projects/Placeholder/assets/icons/chevron-right.svg";
constexpr const char* kParagraph =
    "Wrapping breaks a paragraph at spaces so that no line is wider than the column. "
    "A word longer than a whole line is split inside the word. Line breaks in the text\n"
    "start a new line.";

constexpr int kMaxLines = 32;
constexpr int kMaxMatches = 16;
// Widths of the truncation column the slider offers, as a fraction of the content width.
constexpr float kMinWidthFraction = 0.15f;
constexpr float kMaxWidthFraction = 1.0f;

struct TypeState
{
    float widthFraction = 0.25f;
    char query[64] = "title";
};

TypeState type;

ImU32 U32(const ImVec4& color)
{
    return facet::ToU32(color);
}

bool UsesDisplay(facet::TextStyle style)
{
    return style == facet::TextStyle::LargeTitle || style == facet::TextStyle::Title1;
}

void DrawStyleTable()
{
    const facet::Theme& theme = facet::T();
    const ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerH;
    if (!ImGui::BeginTable("##styles", 4, flags))
        return;
    ImGui::TableSetupColumn("Style", ImGuiTableColumnFlags_WidthStretch, 1.0f);
    for (const char* weight : kWeightNames)
        ImGui::TableSetupColumn(weight, ImGuiTableColumnFlags_WidthStretch, 2.0f);
    ImGui::TableHeadersRow();
    for (const StyleName& entry : kStyles)
    {
        const facet::TextStyleSpec& spec = theme.type[entry.style];
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        facet::Text(facet::TextStyle::Headline, "%s", entry.name);
        Note("%.0f, %s%s", static_cast<double>(spec.size), kWeightNames[static_cast<int>(spec.weight)],
             UsesDisplay(entry.style) ? ", display" : "");
        for (int weight = 0; weight < facet::kFontWeightCount; ++weight)
        {
            ImGui::TableNextColumn();
            ImGui::PushFont(facet::WeightFont(static_cast<facet::FontWeight>(weight), UsesDisplay(entry.style)),
                            spec.size);
            ImGui::TextWrapped("%s", kSample);
            ImGui::PopFont();
        }
    }
    ImGui::EndTable();
}

// A frame of the chosen width with the truncated path in it, for one mode.
void DrawTruncated(const char* label, facet::internal::TruncateMode mode, float width)
{
    const facet::Theme& theme = facet::T();
    const facet::Colors& c = theme.colors;
    Note("%s", label);
    facet::PushTextStyle(facet::TextStyle::Body);
    const ImVec2 padding = facet::Px(ImVec2(theme.spacing.sm, theme.spacing.xs));
    const float height = ImGui::GetFontSize() + padding.y * 2.0f;
    ImGui::Dummy(ImVec2(width, height));
    const ImVec2 a = ImGui::GetItemRectMin();
    const ImVec2 b = ImGui::GetItemRectMax();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    facet::FillRect(drawList, a, b, U32(c.fill[3]), facet::Px(theme.radii.element));
    facet::StrokeRect(drawList, a, b, U32(c.separator), facet::Px(theme.radii.element), facet::HairlineThickness());
    char out[128];
    const auto result = facet::internal::TruncateText(ImGui::GetFont(), ImGui::GetFontSize(), kPlaceholderPath,
                                                      nullptr, width - padding.x * 2.0f, mode, out, sizeof(out));
    drawList->AddText(ImVec2(a.x + padding.x, a.y + padding.y), U32(c.label), out);
    facet::PopTextStyle();
    if (result.truncated && ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", kPlaceholderPath);
}

void DrawWrapped(float width)
{
    const facet::Theme& theme = facet::T();
    facet::PushTextStyle(facet::TextStyle::Body);
    facet::internal::WrapLine lines[kMaxLines];
    const int count =
        facet::internal::WrapText(ImGui::GetFont(), ImGui::GetFontSize(), kParagraph, nullptr, width, lines);
    const int shown = count < kMaxLines ? count : kMaxLines;
    const float lineHeight = ImGui::GetTextLineHeightWithSpacing();
    ImGui::Dummy(ImVec2(width, lineHeight * static_cast<float>(shown)));
    const ImVec2 origin = ImGui::GetItemRectMin();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    // The column edge, so the wrap width is visible.
    facet::HairlineV(drawList, origin.x + width, origin.y, origin.y + lineHeight * static_cast<float>(shown),
                     U32(theme.colors.separatorOpaque));
    for (int i = 0; i < shown; ++i)
        drawList->AddText(ImVec2(origin.x, origin.y + lineHeight * static_cast<float>(i)), U32(theme.colors.label),
                          lines[i].begin, lines[i].end);
    facet::PopTextStyle();
    Note("%d lines", count);
}

void DrawHighlight(facet::demo::PageCtx& ctx)
{
    const facet::Theme& theme = facet::T();
    const facet::Colors& c = theme.colors;
    const float right = facet::demo::ContentRight();
    ImGui::SetNextItemWidth(std::min(facet::Px(theme.spacing.xxxl * 6.0f), ImGui::GetContentRegionAvail().x));
    ImGui::InputTextWithHint("##query", "Filter style names", type.query, sizeof(type.query));
    ctx.Probe("type.query");
    facet::demo::SameLineIfFits(right, facet::demo::ButtonWidth("Clear"));
    if (ImGui::Button("Clear"))
        type.query[0] = '\0';
    facet::ItemCursor();
    ctx.Probe("type.clear");

    if (type.query[0] == '\0')
    {
        Paragraph("Type in the field to highlight matches in the style names.");
        return;
    }
    facet::internal::HighlightStyle style;
    style.text = U32(c.labelSecondary);
    style.matchText = U32(c.label);
    style.matchBackground = U32(c.accentSubtle);
    style.rounding = facet::Px(theme.radii.element) * 0.5f;
    style.padding = facet::Px(theme.spacing.xs) * 0.5f;
    int total = 0;
    facet::PushTextStyle(facet::TextStyle::Body);
    for (const StyleName& entry : kStyles)
    {
        facet::internal::TextRange ranges[kMaxMatches];
        const int found = facet::internal::FindMatches(entry.name, nullptr, type.query, ranges);
        if (found == 0)
            continue;
        total += found;
        const int shown = found < kMaxMatches ? found : kMaxMatches;
        ImGui::Dummy(ImVec2(ImGui::CalcTextSize(entry.name).x, ImGui::GetFontSize()));
        facet::internal::DrawHighlightedText(ImGui::GetWindowDrawList(), ImGui::GetFont(), ImGui::GetFontSize(),
                                             ImGui::GetItemRectMin(), entry.name, nullptr,
                                             std::span<const facet::internal::TextRange>(ranges, static_cast<size_t>(shown)),
                                             style);
    }
    facet::PopTextStyle();
    if (total == 0)
        Paragraph("No style name contains this text.");
    else
        Note("%d match%s", total, total == 1 ? "" : "es");
}

} // namespace

FACET_DEMO_PAGE("type", "Type", "Foundations", 1, "default", "narrow")
{
    // Capture state "narrow" squeezes the truncation and wrapping column.
    if (ctx.capture)
        type.widthFraction = ctx.captureState == 1 ? kMinWidthFraction : TypeState{}.widthFraction;

    facet::demo::PageHeader("Type", "Each text style pushes the font of its weight at its size token. Sizes are "
                                    "design units; the UI scale multiplies them.");
    Note("Fonts in use: %s", facet::FontOriginName(facet::ActiveFontOrigin()));

    SectionHeader("Styles and weights");
    DrawStyleTable();

    SectionHeader("Truncation");
    Note("Column width");
    ImGui::SetNextItemWidth(std::min(facet::Px(facet::T().spacing.xxxl * 6.0f), ImGui::GetContentRegionAvail().x));
    ImGui::SliderFloat("##width", &type.widthFraction, kMinWidthFraction, kMaxWidthFraction, "%.2f");
    facet::ItemCursor();
    ctx.Probe("type.width");
    const float width = ImGui::GetContentRegionAvail().x * type.widthFraction;
    DrawTruncated("End: for labels and titles", facet::internal::TruncateMode::End, width);
    DrawTruncated("Middle: for paths and file names (placeholder path)", facet::internal::TruncateMode::Middle, width);
    if (ctx.captureState == 1)
        ctx.RevealInCapture(0.3f);

    SectionHeader("Wrapping");
    DrawWrapped(width);

    SectionHeader("Match highlighting");
    DrawHighlight(ctx);
}

FACET_DEMO_SELFTEST("type")
{
    test.Do([] { type = TypeState{}; });
    test.Click("type.clear");
    test.Expect("Clear empties the filter", [] { return type.query[0] == '\0'; });
    test.ExpectFocused("type.clear");
    // Shift+Tab moves at once, also right after a click.
    test.KeyPress(ImGuiKey_Tab | ImGuiMod_Shift);
    test.ExpectFocused("type.query");

    test.Do([] { std::snprintf(type.query, sizeof(type.query), "%s", "body"); });
    test.Click("type.width");
    test.Expect("clicking the middle of the slider sets a middle width", [] {
        const float middle = (kMinWidthFraction + kMaxWidthFraction) * 0.5f;
        return type.widthFraction > middle - 0.1f && type.widthFraction < middle + 0.1f;
    });
    test.Expect("the width stays inside its range",
                [] { return type.widthFraction >= kMinWidthFraction && type.widthFraction <= kMaxWidthFraction; });
}
