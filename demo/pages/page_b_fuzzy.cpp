// Fuzzy matching page: a search field over the built-in icon names, the ranked results with
// the matched letters highlighted and each score in a caption, an empty state with a way
// back, and a run over 10000 generated names that shows the cost of one key press.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include <imgui.h>

#include <facet/facet.h>

#include "page_helpers.h"
#include "internal/fuzzy.h"
#include "internal/text_util.h"
#include "pages/pages.h"

namespace
{

using facet::internal::FuzzyFilter;
using facet::internal::FuzzyMatch;
using facet::internal::FuzzyResult;
using facet::internal::FuzzyScore;
using facet::internal::TextRange;

// Width of the search field in multiples of the font size: room for a long query.
constexpr float kSearchWidthEm = 20.0f;
// Results listed before "and N more"; enough to see the ranking without a long page.
constexpr int kShownResults = 12;
// Candidates of the speed run.
constexpr int kGeneratedCount = 10000;

struct FuzzyPageState
{
    char query[64] = "";
    // The speed run: names generated once, results of the last run.
    bool generatedReady = false;
    bool runDone = false;
    int runMatches = 0;
    double runMilliseconds = 0.0;
};

FuzzyPageState page;

// Filled in while drawing, read by the selftest.
int shownCount = 0;
int matchCount = 0;
bool emptyStateShown = false;
std::string topName;
int topScore = 0;

std::vector<const char*>& IconNames()
{
    static std::vector<const char*> names;
    if (names.size() != static_cast<std::size_t>(facet::IconCount()))
    {
        names.clear();
        for (int i = 0; i < facet::IconCount(); ++i)
            names.push_back(facet::IconNameAt(i));
    }
    return names;
}

std::vector<FuzzyResult>& Results(std::size_t size)
{
    static std::vector<FuzzyResult> results;
    if (results.size() < size)
        results.resize(size);
    return results;
}

// Names for the speed run: two icon names and a number, so the list looks like real labels
// and is clearly generated.
struct GeneratedNames
{
    std::vector<std::string> storage;
    std::vector<const char*> names;
    std::vector<FuzzyResult> results;
};

GeneratedNames& Generated()
{
    static GeneratedNames generated;
    if (generated.names.empty())
    {
        const std::vector<const char*>& icons = IconNames();
        const std::size_t n = std::max<std::size_t>(icons.size(), 1);
        generated.storage.reserve(kGeneratedCount);
        for (int i = 0; i < kGeneratedCount; ++i)
        {
            std::string name = icons[static_cast<std::size_t>(i) % n];
            name += ' ';
            name += icons[static_cast<std::size_t>(i * 7 + 3) % n];
            name += ' ';
            name += std::to_string(i);
            generated.storage.push_back(std::move(name));
        }
        for (const std::string& name : generated.storage)
            generated.names.push_back(name.c_str());
        generated.results.resize(generated.names.size());
    }
    return generated;
}

void DrawEmptyState(const facet::Theme& theme, const facet::demo::PageCtx& ctx)
{
    emptyStateShown = true;
    const facet::Colors& c = theme.colors;
    ImGui::Dummy(ImVec2(0.0f, facet::Px(theme.spacing.lg)));
    facet::IconGlyph(facet::Icon("search"), facet::Px(theme.spacing.xxxl), facet::ToU32(c.labelSecondary));
    facet::Text(facet::TextStyle::Headline, "No icon name matches \"%s\"", page.query);
    facet::demo::Paragraph("Every letter of the query has to appear in the name, in the same order. "
                           "Try fewer letters, or the first letters of the words, such as cd for "
                           "chevron-down.");
    if (facet::Button("Clear search"))
        page.query[0] = '\0';
    ctx.Probe("fuzzy.clear");
}

void DrawResults(const facet::Theme& theme, const std::vector<FuzzyResult>& results, int count)
{
    const facet::Colors& c = theme.colors;
    const std::vector<const char*>& names = IconNames();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImFont* font = ImGui::GetFont();
    const float fontSize = ImGui::GetFontSize();
    const float iconSize = std::round(facet::Px(theme.spacing.lg));
    const float rowHeight = std::max(fontSize, iconSize) + facet::Px(theme.spacing.sm);
    const float gap = facet::Px(theme.spacing.sm);
    // The match stands out by contrast: matched letters in label on a neutral wash, the rest of
    // the name in labelSecondary (still 4.5:1). Without a query every name is plain label text.
    const bool querying = page.query[0] != '\0';
    const facet::internal::HighlightStyle highlight{
        .text = facet::ToU32(querying ? c.labelSecondary : c.label),
        .matchText = facet::ToU32(c.label),
        .matchBackground = facet::ToU32(c.fill[2]),
        .rounding = facet::Px(theme.spacing.xs),
        .padding = 0.0f,
    };
    const facet::TextStyleSpec& caption = theme.type[facet::TextStyle::Footnote];
    const float captionSize = facet::Px(caption.size);
    ImFont* captionFont = facet::WeightFont(caption.weight);
    if (captionFont == nullptr)
        captionFont = font;

    shownCount = std::min(count, kShownResults);
    for (int i = 0; i < shownCount; ++i)
    {
        const FuzzyResult& result = results[static_cast<std::size_t>(i)];
        const char* name = names[static_cast<std::size_t>(result.index)];
        TextRange ranges[facet::internal::kFuzzyMaxRanges];
        const FuzzyMatch match = FuzzyScore(page.query, name, ranges);
        const int rangeCount = std::min(match.rangeCount, facet::internal::kFuzzyMaxRanges);

        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x, rowHeight));
        if (!ImGui::IsItemVisible())
            continue;
        const float centerY = origin.y + 0.5f * rowHeight;
        facet::DrawIcon(drawList, facet::FindIcon(name), ImVec2(origin.x, std::round(centerY - 0.5f * iconSize)),
                        iconSize, facet::ToU32(c.labelSecondary));
        const float textX = origin.x + iconSize + gap;
        const float textY = std::round(centerY - 0.5f * fontSize);
        const float width = facet::internal::DrawHighlightedText(drawList, font, fontSize, ImVec2(textX, textY), name,
                                                                 nullptr, std::span(ranges, rangeCount), highlight);
        char score[32];
        if (page.query[0] != '\0')
            std::snprintf(score, sizeof(score), "score %d", result.score);
        else
            std::snprintf(score, sizeof(score), "list order");
        drawList->AddText(captionFont, captionSize,
                          ImVec2(textX + width + gap, std::round(centerY - 0.5f * captionSize)),
                          facet::ToU32(c.labelSecondary), score);
    }
    if (count > shownCount)
        facet::demo::Note("and %d more", count - shownCount);
}

void DrawSpeedRun(const facet::demo::PageCtx& ctx)
{
    facet::demo::SectionHeader("10000 candidates");
    facet::demo::Paragraph("The same query against 10000 generated names (two icon names and a number "
                           "each), filtered and sorted the way a long list would be on every key press.");
    if (facet::Button("Filter 10000 generated names"))
    {
        GeneratedNames& generated = Generated();
        const auto start = std::chrono::steady_clock::now();
        int total = 0;
        FuzzyFilter(page.query, generated.names, generated.results, &total);
        const auto end = std::chrono::steady_clock::now();
        page.runDone = true;
        page.runMatches = total;
        page.runMilliseconds = std::chrono::duration<double, std::milli>(end - start).count();
    }
    ctx.Probe("fuzzy.run");
    if (!page.runDone)
    {
        facet::demo::Note("Not run yet.");
        return;
    }
    // Timings differ from run to run, so capture runs, which must give the same image every
    // time, leave them out.
    if (ctx.capture)
        facet::demo::Note("%d of %d generated names match; the time is not shown in capture runs.",
                          page.runMatches, kGeneratedCount);
    else
        facet::demo::Note("%d of %d generated names match, filtered and sorted in %.2f ms.", page.runMatches,
                          kGeneratedCount, page.runMilliseconds);
}

} // namespace

FACET_DEMO_PAGE("fuzzy", "Fuzzy matching", "Blocks", 80, "default", "ranked", "empty")
{
    const facet::Theme& theme = facet::T();
    if (ctx.capture)
    {
        static constexpr const char* kCaptureQueries[] = {"", "cd", "zzq"};
        std::snprintf(page.query, sizeof(page.query), "%s", kCaptureQueries[ctx.captureState]);
        page.runDone = false;
    }

    facet::demo::PageHeader("Fuzzy matching",
                            "Typed letters are matched against names in order, not necessarily side by side. "
                            "Exact names rank first, then names that start with the query, then matches at word "
                            "starts and in consecutive letters. The matched letters are highlighted.");
    facet::demo::SectionHeader("Icon names");

    ImGui::SetNextItemWidth(std::min(ImGui::GetContentRegionAvail().x, ImGui::GetFontSize() * kSearchWidthEm));
    facet::TextFieldOptions queryOptions;
    queryOptions.placeholder = "Type part of an icon name, such as cd or arrow";
    queryOptions.icon = facet::Icon("search");
    facet::TextField("##query", page.query, sizeof(page.query), queryOptions);
    ctx.Probe("fuzzy.query");

    const std::vector<const char*>& names = IconNames();
    std::vector<FuzzyResult>& results = Results(names.size());
    matchCount = FuzzyFilter(page.query, names, results);
    emptyStateShown = false;
    topName = matchCount > 0 ? names[static_cast<std::size_t>(results[0].index)] : "";
    topScore = matchCount > 0 ? results[0].score : 0;

    if (page.query[0] == '\0')
        facet::demo::Note("No query: all %d names in list order. The data is built in and filtered on the frame "
                          "thread, so this list has no loading or error state.",
                          static_cast<int>(names.size()));
    else if (matchCount > 0)
        facet::demo::Note("%d of %d names match, best first. Higher scores rank higher; equal scores keep the "
                          "list order.",
                          matchCount, static_cast<int>(names.size()));
    ImGui::Spacing();

    if (matchCount == 0)
    {
        shownCount = 0;
        DrawEmptyState(theme, ctx);
    }
    else
    {
        DrawResults(theme, results, matchCount);
    }
    DrawSpeedRun(ctx);
}

FACET_DEMO_SELFTEST("fuzzy")
{
    test.Do([] { page = FuzzyPageState{}; });
    test.Wait(1);
    test.Expect("without a query every name is listed in order",
                [] { return matchCount == facet::IconCount() && topName == facet::IconNameAt(0); });

    test.Click("fuzzy.query");
    test.Type("cd");
    test.Wait(2);
    test.ExpectText("\"cd\" ranks chevron-down first", [] { return topName; }, "chevron-down");
    test.Expect("the top result has a positive score", [] { return topScore > 0; });

    test.Type("zz");
    test.Wait(2);
    test.Expect("a query without a match shows the empty state",
                [] { return emptyStateShown && matchCount == 0 && shownCount == 0; });
    test.Click("fuzzy.clear");
    test.Expect("Clear search lists every name again",
                [] { return page.query[0] == '\0' && matchCount == facet::IconCount(); });

    // The same by keyboard: from the field, Tab reaches Clear search.
    test.Click("fuzzy.query");
    test.Type("qqq");
    test.Wait(2);
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("fuzzy.clear");
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Space on Clear search empties the query", [] { return page.query[0] == '\0'; });

    test.Click("fuzzy.query");
    test.Type("arrow");
    test.Wait(2);
    test.ExpectText("\"arrow\" ranks the arrows first", [] { return topName.substr(0, 6); }, "arrow-");
    test.Click("fuzzy.run");
    test.Expect("the speed run filters 10000 generated names",
                [] { return page.runDone && page.runMatches > 0 && page.runMatches <= kGeneratedCount; });
}
