#pragma once

// Fuzzy matching: scores a typed query against candidate names and reports which characters
// matched, so a list can rank the candidates and highlight the match (DrawHighlightedText in
// text_util.h draws the ranges). Used by searchable dropdowns, autocomplete, the command
// palette, the icon picker, settings search and filter fields.
//
// A query matches a candidate when every character of the query appears in the candidate in
// the same order. Among the matches, the score prefers, from strongest to weakest:
//   - the exact name, then a name that starts with the query;
//   - characters at word starts: the first character, after a separator (space - _ . / \ :
//     and other punctuation), an upper-case letter after a lower-case one (camelCase), and a
//     digit after a letter;
//   - consecutive runs of matched characters;
//   - few and short gaps between matched characters, and an early first match;
//   - characters whose case equals the query's (a small bonus; matching ignores case).
// Matching is case-insensitive for ASCII, Latin-1 and Latin Extended-A, and letters with
// diacritics match their base letter ("e" matches "é"), so Latin names rank as expected.
// Text is UTF-8; invalid bytes count as one character each and match only themselves.
//
// Memory: nothing here allocates. Scoring uses a fixed scratch table per thread for up to
// kFuzzyMaxQuery query characters against kFuzzyMaxCandidate candidate characters. A longer
// candidate or query is matched by the fallback: one greedy left-to-right pass with the same
// bonuses, which still finds every match but may pick a lower-scoring placement.
//
// Example, filtering a list into caller storage on every key press:
//
//     std::vector<FuzzyResult> results(names.size());        // once, outside the frame loop
//     const int shown = FuzzyFilter(query, names, results);  // best first
//     for (int i = 0; i < shown; ++i)
//     {
//         TextRange ranges[kFuzzyMaxRanges];
//         const FuzzyMatch m = FuzzyScore(query, names[results[i].index], ranges);
//         DrawHighlightedText(..., std::span(ranges, std::min(m.rangeCount, kFuzzyMaxRanges)), ...);
//     }

#include <span>
#include <string_view>

#include "internal/text_util.h"

namespace facet::internal
{

// Characters of the query and of a candidate scored by the full table; longer ones use the
// greedy fallback described above.
inline constexpr int kFuzzyMaxQuery = 64;
inline constexpr int kFuzzyMaxCandidate = 256;
// Enough room for the ranges of any query of up to kFuzzyMaxQuery / 4 characters; callers
// that highlight longer queries pass a larger span.
inline constexpr int kFuzzyMaxRanges = 16;

struct FuzzyMatch
{
    bool matched = false;  // every query character was found in order
    int score = 0;         // higher ranks first; 0 for the empty query; meaningless when !matched
    int rangeCount = 0;    // matched byte ranges in the candidate, which may exceed the span given
};

// Scores `query` against `candidate`. Writes up to ranges.size() matched byte ranges of the
// candidate, sorted and merged (adjacent characters form one range). An empty query (or one
// of spaces only) matches every candidate with score 0 and no ranges. Leading and trailing
// spaces of the query are ignored; spaces inside it are matched like other characters.
FuzzyMatch FuzzyScore(std::string_view query, std::string_view candidate, std::span<TextRange> ranges = {});

struct FuzzyResult
{
    int index = 0;  // index of the candidate in the list given to FuzzyFilter
    int score = 0;
};

// Candidate `index` of a lazy list; return nullptr for an entry that should never match.
using FuzzyGetter = const char* (*)(int index, void* user);

// Scores every candidate and writes the matching ones into `out`, best score first; equal
// scores keep the list order, and the empty query keeps the whole list in order. When more
// candidates match than `out` holds, the best ones are kept. Returns the number written;
// `totalMatches` (may be null) receives the number of candidates that matched.
int FuzzyFilter(std::string_view query, std::span<const char* const> candidates, std::span<FuzzyResult> out,
                int* totalMatches = nullptr);
int FuzzyFilter(std::string_view query, int candidateCount, FuzzyGetter getter, void* user,
                std::span<FuzzyResult> out, int* totalMatches = nullptr);

} // namespace facet::internal
