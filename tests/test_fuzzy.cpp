// Fuzzy matching (src/internal/fuzzy.h): ranking, ranges, Latin text, the long-text fallback,
// filtering and its cost.

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include <doctest/doctest.h>

#include <facet/core/icons.h>

#include "internal/fuzzy.h"

#ifdef _DEBUG
#include <crtdbg.h>
#endif

using facet::internal::FuzzyFilter;
using facet::internal::FuzzyMatch;
using facet::internal::FuzzyResult;
using facet::internal::FuzzyScore;
using facet::internal::TextRange;

namespace
{

// Setting names as an app's options screen would list them, plus option keys in camelCase.
const char* const kSettings[] = {
    "Vertical sync",     "Frame rate limit", "Render scale",     "Texture quality", "Texture filter",
    "Shadow quality",    "Field of view",    "Mouse sensitivity", "Invert mouse Y",  "Master volume",
    "Music volume",      "Push to talk",     "Show frame time",  "Reduce motion",   "Interface scale",
    "Accent color",      "Theme",            "Language",         "Subtitles",       "Reset all settings",
    "maxFrameRate",      "minFrameTime",     "renderScale",      "textureFilter",   "showFrameGraph",
    "frameSkip",         "fullscreen",
};

std::vector<const char*> IconNames()
{
    std::vector<const char*> names;
    for (int i = 0; i < facet::IconCount(); ++i)
        names.push_back(facet::IconNameAt(i));
    return names;
}

// The rank of `name` (0 best) when `list` is filtered with `query`, or -1 when it does not match.
int RankOf(std::string_view query, const std::vector<const char*>& list, std::string_view name)
{
    std::vector<FuzzyResult> results(list.size());
    const int count = FuzzyFilter(query, list, results);
    for (int i = 0; i < count; ++i)
    {
        if (name == list[static_cast<std::size_t>(results[static_cast<std::size_t>(i)].index)])
            return i;
    }
    return -1;
}

// "a must rank above b for query q" over a list.
void CheckRanksAbove(std::string_view query, const std::vector<const char*>& list, std::string_view a,
                     std::string_view b)
{
    const int rankA = RankOf(query, list, a);
    const int rankB = RankOf(query, list, b);
    INFO("query \"", std::string(query), "\": ", std::string(a), " at ", rankA, ", ", std::string(b), " at ",
         rankB);
    // Both must match, or the order says nothing about the ranking.
    REQUIRE(rankA >= 0);
    REQUIRE(rankB >= 0);
    CHECK(rankA < rankB);
}

std::vector<const char*> SettingNames()
{
    return std::vector<const char*>(std::begin(kSettings), std::end(kSettings));
}

std::string Ranges(std::string_view query, std::string_view candidate)
{
    TextRange ranges[facet::internal::kFuzzyMaxRanges];
    const FuzzyMatch match = FuzzyScore(query, candidate, ranges);
    std::string text;
    for (int i = 0; i < std::min(match.rangeCount, facet::internal::kFuzzyMaxRanges); ++i)
    {
        char part[32];
        std::snprintf(part, sizeof(part), "%s[%d,%d)", i > 0 ? " " : "", ranges[i].begin, ranges[i].end);
        text += part;
    }
    return text;
}

} // namespace

TEST_CASE("Fuzzy matching finds the query characters in order, ignoring case")
{
    CHECK(FuzzyScore("cd", "chevron-down").matched);
    CHECK(FuzzyScore("CD", "chevron-down").matched);
    CHECK(FuzzyScore("chev", "Chevron").matched);
    CHECK_FALSE(FuzzyScore("dc", "chevron-down").matched);
    CHECK_FALSE(FuzzyScore("xyz", "chevron-down").matched);
    CHECK_FALSE(FuzzyScore("chevron-downs", "chevron-down").matched);
    CHECK_FALSE(FuzzyScore("a", "").matched);
    // A separator in the query matches any separator, so "arrow down" finds "arrow-down".
    CHECK(FuzzyScore("arrow down", "arrow-down").matched);
    CHECK(FuzzyScore("zoom_in", "zoom-in").matched);
}

TEST_CASE("An empty query matches everything with score 0 and keeps the list order")
{
    const FuzzyMatch match = FuzzyScore("", "anything");
    CHECK(match.matched);
    CHECK(match.score == 0);
    CHECK(match.rangeCount == 0);
    CHECK(FuzzyScore("   ", "anything").matched);

    const std::vector<const char*> names = IconNames();
    std::vector<FuzzyResult> results(names.size());
    int total = -1;
    const int count = FuzzyFilter("", names, results, &total);
    REQUIRE(count == static_cast<int>(names.size()));
    CHECK(total == count);
    for (int i = 0; i < count; ++i)
        CHECK(results[static_cast<std::size_t>(i)].index == i);
}

TEST_CASE("Fuzzy ranking over the built-in icon names")
{
    const std::vector<const char*> icons = IconNames();
    REQUIRE(icons.size() > 40);
    // The exact name first, then names that start with the query.
    CheckRanksAbove("eye", icons, "eye", "eye-off");
    CheckRanksAbove("star", icons, "star", "star-filled");
    CheckRanksAbove("down", icons, "download", "arrow-down");
    // A prefix beats the same letters inside a word.
    CheckRanksAbove("set", icons, "settings", "reset");
    CheckRanksAbove("in", icons, "info", "zoom-in");
    // A word start beats the same letters in the middle of a word.
    CheckRanksAbove("in", icons, "zoom-in", "pin");
    CheckRanksAbove("up", icons, "upload", "arrow-up");
    // A whole word later in the name beats a first letter followed by scattered ones.
    CheckRanksAbove("up", icons, "arrow-up", "unpin");
    // Consecutive letters beat the same letters with gaps.
    CheckRanksAbove("cl", icons, "clock", "calendar");
    CheckRanksAbove("cl", icons, "close", "collapse");
    // Two word starts beat a word start followed by a letter inside a word.
    CheckRanksAbove("cd", icons, "chevron-down", "calendar");
    CheckRanksAbove("ad", icons, "arrow-down", "calendar");
    // The whole query must be there.
    CHECK(RankOf("chevron", icons, "check") == -1);
}

TEST_CASE("Fuzzy ranking over setting names: words, camelCase and initials")
{
    const std::vector<const char*> settings = SettingNames();
    // A word after a space counts a little more than a camelCase hump.
    CheckRanksAbove("time", settings, "Show frame time", "minFrameTime");
    CheckRanksAbove("scale", settings, "Render scale", "renderScale");
    CheckRanksAbove("tf", settings, "Texture filter", "textureFilter");
    // A camelCase hump counts as a word start.
    CheckRanksAbove("fr", settings, "maxFrameRate", "textureFilter");
    CheckRanksAbove("rs", settings, "renderScale", "Reset all settings");
    CheckRanksAbove("fs", settings, "frameSkip", "fullscreen");
    // The first letters of the words.
    CheckRanksAbove("mv", settings, "Master volume", "Mouse sensitivity");
}

TEST_CASE("Shorter gaps and earlier matches score higher")
{
    CHECK(FuzzyScore("ab", "axb").score > FuzzyScore("ab", "axxxb").score);
    CHECK(FuzzyScore("b", "xxb").score > FuzzyScore("b", "xxxxxxb").score);
}

TEST_CASE("The typed case wins a tie")
{
    const FuzzyMatch lower = FuzzyScore("Theme", "theme");
    const FuzzyMatch upper = FuzzyScore("Theme", "Theme");
    REQUIRE(lower.matched);
    REQUIRE(upper.matched);
    CHECK(upper.score > lower.score);
    CHECK(FuzzyScore("theme", "theme").score > FuzzyScore("theme", "THEME").score);
}

TEST_CASE("Equal scores keep the list order")
{
    const char* const list[] = {"beta-one", "alpha-one", "beta-one", "gamma-one"};
    std::vector<FuzzyResult> results(4);
    const int count = FuzzyFilter("one", list, results);
    REQUIRE(count == 4);
    CHECK(results[0].score == results[1].score);
    for (int i = 0; i < count; ++i)
        CHECK(results[static_cast<std::size_t>(i)].index == i);
}

TEST_CASE("Matched ranges cover the matched characters, merged when adjacent")
{
    CHECK(Ranges("cd", "chevron-down") == "[0,1) [8,9)");
    CHECK(Ranges("down", "arrow-down") == "[6,10)");
    CHECK(Ranges("eye", "eye-off") == "[0,3)");
    CHECK(Ranges("ptt", "Push to talk") == "[0,1) [5,6) [8,9)");
    CHECK(Ranges("", "anything").empty());

    // Fewer slots than ranges: the count still says how many there are.
    TextRange one[1];
    const FuzzyMatch match = FuzzyScore("ptt", "Push to talk", one);
    CHECK(match.rangeCount == 3);
    CHECK(one[0].begin == 0);
    CHECK(one[0].end == 1);
}

TEST_CASE("Latin letters match across case and diacritics, with byte ranges in UTF-8")
{
    // "Résumé": R, é (2 bytes), s, u, m, é (2 bytes).
    CHECK(FuzzyScore("resume", "R\xC3\xA9sum\xC3\xA9").matched);
    CHECK(FuzzyScore("R\xC3\x89SUM\xC3\x89", "r\xC3\xA9sum\xC3\xA9").matched);  // upper-case accents
    CHECK(Ranges("re", "R\xC3\xA9sum\xC3\xA9") == "[0,3)");
    // Latin Extended-A: "Łódź" (Ł, ó and ź take two bytes each, d one).
    CHECK(FuzzyScore("lodz", "\xC5\x81\xC3\xB3""d\xC5\xBA").matched);
    CHECK(Ranges("lodz", "\xC5\x81\xC3\xB3""d\xC5\xBA") == "[0,7)");
    // Letters without a base letter match only themselves, in either case.
    CHECK(FuzzyScore("\xC3\xA6", "\xC3\x86ther").matched);  // æ finds Æ
    CHECK_FALSE(FuzzyScore("a", "\xC3\x86").matched);
    // Invalid bytes never break the scan.
    CHECK(FuzzyScore("ab", "a\xFF\xFE" "b").matched);
    CHECK(Ranges("ab", "a\xFF\xFE" "b") == "[0,1) [3,4)");
    CHECK_FALSE(FuzzyScore("ab", "a\xC3").matched);  // a cut sequence at the end
}

TEST_CASE("Long candidates and queries use the greedy fallback")
{
    // A candidate longer than the table: the match is still found, with ranges.
    std::string longName(300, 'x');
    longName.replace(280, 4, "-cat");
    REQUIRE(longName.size() > static_cast<std::size_t>(facet::internal::kFuzzyMaxCandidate));
    TextRange ranges[4];
    const FuzzyMatch match = FuzzyScore("cat", longName, ranges);
    REQUIRE(match.matched);
    CHECK(match.rangeCount == 1);
    CHECK(ranges[0].begin == 281);
    CHECK(ranges[0].end == 284);
    CHECK_FALSE(FuzzyScore("dog", longName).matched);

    // A word start in a long text still counts more than a letter inside a word.
    std::string boundary(270, 'y');
    boundary += " cat";
    std::string inside(270, 'y');
    inside += "scat";
    CHECK(FuzzyScore("cat", boundary).score > FuzzyScore("cat", inside).score);

    // A query longer than the table matches a long enough candidate.
    const std::string longQuery(80, 'z');
    std::string zs(100, 'z');
    CHECK(FuzzyScore(longQuery, zs).matched);
    CHECK_FALSE(FuzzyScore(longQuery, std::string(79, 'z')).matched);
}

TEST_CASE("Filtering into small storage keeps the best matches")
{
    const std::vector<const char*> icons = IconNames();
    std::vector<FuzzyResult> all(icons.size());
    int total = 0;
    const int count = FuzzyFilter("r", icons, all, &total);
    REQUIRE(count == total);
    REQUIRE(count > 5);

    std::vector<FuzzyResult> top(3);
    int topTotal = 0;
    const int written = FuzzyFilter("r", icons, top, &topTotal);
    CHECK(written == 3);
    CHECK(topTotal == total);
    for (int i = 0; i < 3; ++i)
    {
        CHECK(top[static_cast<std::size_t>(i)].index == all[static_cast<std::size_t>(i)].index);
        CHECK(top[static_cast<std::size_t>(i)].score == all[static_cast<std::size_t>(i)].score);
    }

    // Null entries of a lazy list never match.
    const char* const withNull[] = {"alpha", nullptr, "alps"};
    std::vector<FuzzyResult> out(3);
    CHECK(FuzzyFilter("", withNull, out) == 2);
    CHECK(FuzzyFilter("al", withNull, out) == 2);
}

TEST_CASE("Filtering 10000 candidates takes milliseconds and allocates nothing")
{
    // Generated names: two or three words from the setting names, joined by spaces.
    std::vector<std::string> storage;
    storage.reserve(10000);
    const std::vector<const char*> words = SettingNames();
    for (int i = 0; i < 10000; ++i)
    {
        std::string name = words[static_cast<std::size_t>(i) % words.size()];
        name += ' ';
        name += words[static_cast<std::size_t>(i * 7 + 3) % words.size()];
        if (i % 3 == 0)
        {
            name += ' ';
            name += std::to_string(i);
        }
        storage.push_back(std::move(name));
    }
    std::vector<const char*> names;
    names.reserve(storage.size());
    for (const std::string& name : storage)
        names.push_back(name.c_str());
    std::vector<FuzzyResult> results(names.size());

#ifdef _DEBUG
    static long allocations = 0;
    allocations = 0;
    const _CRT_ALLOC_HOOK previous = _CrtSetAllocHook(
        [](int type, void*, size_t, int, long, const unsigned char*, int) -> int {
            if (type == _HOOK_ALLOC || type == _HOOK_REALLOC)
                ++allocations;
            return 1;  // let the allocation proceed
        });
#endif
    const auto start = std::chrono::steady_clock::now();
    int matched = 0;
    for (const char* query : {"f", "fr", "fra", "fram", "frame", "frame t", "frame ti"})
        matched += FuzzyFilter(query, names, results);
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
#ifdef _DEBUG
    _CrtSetAllocHook(previous);
    CHECK(allocations == 0);
#endif
    CHECK(matched > 0);
    MESSAGE("7 queries over 10000 candidates: ", seconds * 1000.0, " ms");
#ifdef NDEBUG
    // One key press must stay well inside a frame; the bound is loose for slow machines.
    CHECK(seconds / 7.0 < 0.05);
#endif
}
