// Fuzzy matching (fuzzy.h).
//
// The score of one placement of the query in a candidate is the sum over the matched
// characters of a base score, the position bonus of the character and a case bonus, minus the
// gap penalties between matched characters and a small penalty for a late first match. A character that continues a run of consecutive
// matches gets at least the bonus of the run's first character, so a whole word matched at
// its start outranks the same letters spread over several words. A dynamic program over
// (query character, candidate character) finds the best placement; the table also records
// where each cell came from, which gives the matched ranges.

#include "internal/fuzzy.h"

#include <algorithm>
#include <climits>
#include <cstdint>

namespace facet::internal
{
namespace
{

// --- Scores ----------------------------------------------------------------------------------
// One matched character.
constexpr int kScoreMatch = 16;
// Position bonuses: the first character of the candidate, a character after a separator, and
// an upper-case letter after a lower-case one or a digit after a letter.
constexpr int kBonusFirst = 10;
constexpr int kBonusBoundary = 8;
constexpr int kBonusCamel = 7;
// Smallest bonus of a character that directly follows the previous matched one.
constexpr int kBonusConsecutive = 4;
// The matched character has exactly the case typed.
constexpr int kBonusCase = 1;
// A gap of n unmatched characters between two matched ones costs kGapStart + (n - 1) * kGapExtend.
constexpr int kGapStart = -3;
constexpr int kGapExtend = -1;
// Unmatched characters before the first match cost one point each, up to this many.
constexpr int kLeadingCap = 4;
// The candidate equals the query (ignoring case and diacritics). A candidate that only starts
// with the query needs no extra bonus: its first character takes kBonusFirst without a leading
// penalty and every further character continues that run, so it gains at least two points per
// character over any placement that starts later, more than the case bonus can make up.
constexpr int kBonusExact = 80;

constexpr int kNoScore = INT_MIN / 4;

// --- Characters -------------------------------------------------------------------------------

enum class CharClass : std::uint8_t
{
    Separator,
    Lower,
    Upper,
    Digit,
    Other,  // letters without case and symbols that are not separators
};

struct Char
{
    std::uint32_t original = 0;
    std::uint32_t folded = 0;  // lower case, base letter, separators as ' '
    int byteBegin = 0;
    int byteEnd = 0;
    CharClass cls = CharClass::Other;
};

// Base letters of U+0100 to U+017F (Latin Extended-A); '*' keeps the lower-case character.
constexpr char kExtendedABase[] = "aaaaaaccccccccddddeeeeeeeeeegggggggghhhhiiiiiiiiii**jjkkkllllllllllnnnnnnn**oooooo**"
                                  "rrrrrrsssssssstttttt"
                                  "uuuuuuuuuuuuwwyyyzzzzzzs";
static_assert(sizeof(kExtendedABase) - 1 == 128, "one entry per character of Latin Extended-A");

// Base letters of U+00E0 to U+00FF (lower-case Latin-1 letters); '*' keeps the character.
constexpr char kLatin1Base[] = "aaaaaa*ceeeeiiii*nooooo*ouuuuy*y";
static_assert(sizeof(kLatin1Base) - 1 == 32, "one entry per character from U+00E0 to U+00FF");

bool IsAsciiSeparator(std::uint32_t c)
{
    return c == ' ' || c == '-' || c == '_' || c == '.' || c == '/' || c == '\\' || c == ':' || c == '\t';
}

bool IsUpperExtendedA(std::uint32_t c)
{
    if (c == 0x130)
        return true;
    if (c == 0x131 || c == 0x138 || c == 0x149 || c == 0x17F)
        return false;
    if (c == 0x178)
        return true;
    if ((c >= 0x139 && c <= 0x148) || (c >= 0x179 && c <= 0x17E))
        return (c & 1u) == 1u;
    return (c & 1u) == 0u;
}

CharClass Classify(std::uint32_t c)
{
    if (c >= 'a' && c <= 'z')
        return CharClass::Lower;
    if (c >= 'A' && c <= 'Z')
        return CharClass::Upper;
    if (c >= '0' && c <= '9')
        return CharClass::Digit;
    if (c < 0x80)
        return CharClass::Separator;  // spaces, punctuation and control characters
    if (c >= 0xC0 && c <= 0xDE && c != 0xD7)
        return CharClass::Upper;
    if (c >= 0xDF && c <= 0xFF && c != 0xF7)
        return CharClass::Lower;
    if (c >= 0x100 && c <= 0x17F)
        return IsUpperExtendedA(c) ? CharClass::Upper : CharClass::Lower;
    if (c >= 0xA0 && c <= 0xBF)
        return CharClass::Separator;
    return CharClass::Other;
}

std::uint32_t Fold(std::uint32_t c)
{
    if (c >= 'A' && c <= 'Z')
        return c + ('a' - 'A');
    if (c < 0x80)
        return IsAsciiSeparator(c) ? ' ' : c;
    if (c >= 0xC0 && c <= 0xDE && c != 0xD7)
        c += 0x20;
    if (c >= 0xE0 && c <= 0xFF)
    {
        const char base = kLatin1Base[c - 0xE0];
        return base == '*' ? c : static_cast<std::uint32_t>(base);
    }
    if (c >= 0x100 && c <= 0x17F)
    {
        const char base = kExtendedABase[c - 0x100];
        if (base != '*')
            return static_cast<std::uint32_t>(base);
        return IsUpperExtendedA(c) ? c + 1 : c;
    }
    return c;
}

// Decodes one UTF-8 character at `pos`; an invalid or cut sequence is one character of one
// byte with a value no valid character has (0x110000 + byte), so it matches only itself.
std::uint32_t Decode(std::string_view text, std::size_t pos, int* length)
{
    const auto byte = [&](std::size_t i) { return static_cast<std::uint8_t>(text[i]); };
    const std::uint8_t b0 = byte(pos);
    *length = 1;
    if (b0 < 0x80)
        return b0;
    int extra = 0;
    std::uint32_t c = 0;
    if ((b0 & 0xE0u) == 0xC0u)
    {
        extra = 1;
        c = b0 & 0x1Fu;
    }
    else if ((b0 & 0xF0u) == 0xE0u)
    {
        extra = 2;
        c = b0 & 0x0Fu;
    }
    else if ((b0 & 0xF8u) == 0xF0u)
    {
        extra = 3;
        c = b0 & 0x07u;
    }
    else
    {
        return 0x110000u + b0;
    }
    if (pos + static_cast<std::size_t>(extra) >= text.size())
        return 0x110000u + b0;
    for (int i = 1; i <= extra; ++i)
    {
        const std::uint8_t b = byte(pos + static_cast<std::size_t>(i));
        if ((b & 0xC0u) != 0x80u)
            return 0x110000u + b0;
        c = (c << 6) | (b & 0x3Fu);
    }
    *length = 1 + extra;
    return c;
}

// Decodes up to `capacity` characters into `out`; returns the number of characters in the
// whole text (which may exceed capacity).
int DecodeText(std::string_view text, Char* out, int capacity)
{
    int count = 0;
    std::size_t pos = 0;
    while (pos < text.size())
    {
        int length = 1;
        const std::uint32_t c = Decode(text, pos, &length);
        if (count < capacity)
        {
            Char& ch = out[count];
            ch.original = c;
            ch.folded = Fold(c);
            ch.byteBegin = static_cast<int>(pos);
            ch.byteEnd = static_cast<int>(pos) + length;
            ch.cls = Classify(c);
        }
        ++count;
        pos += static_cast<std::size_t>(length);
    }
    return count;
}

int PositionBonus(const Char* text, int j)
{
    if (j == 0)
        return kBonusFirst;
    const CharClass prev = text[j - 1].cls;
    const CharClass cur = text[j].cls;
    if (cur == CharClass::Separator)
        return 0;
    if (prev == CharClass::Separator)
        return kBonusBoundary;
    if (prev == CharClass::Lower && cur == CharClass::Upper)
        return kBonusCamel;
    if ((prev == CharClass::Lower || prev == CharClass::Upper) && cur == CharClass::Digit)
        return kBonusCamel;
    return 0;
}

int CaseBonus(const Char& q, const Char& c)
{
    const bool hasCase = q.cls == CharClass::Lower || q.cls == CharClass::Upper;
    return (hasCase && q.original == c.original) ? kBonusCase : 0;
}

// Scratch of one thread: the decoded query and candidate, and the score table with its
// back links. Sized for kFuzzyMaxQuery by kFuzzyMaxCandidate.
struct Scratch
{
    Char query[kFuzzyMaxQuery];
    Char text[kFuzzyMaxCandidate];
    int bonus[kFuzzyMaxCandidate];
    int score[kFuzzyMaxQuery][kFuzzyMaxCandidate];
    std::int16_t from[kFuzzyMaxQuery][kFuzzyMaxCandidate];   // matched column of the previous row
    std::int8_t run[kFuzzyMaxQuery][kFuzzyMaxCandidate];     // bonus of the run the cell continues
    int positions[kFuzzyMaxQuery];
};

Scratch& ThreadScratch()
{
    static thread_local Scratch scratch;
    return scratch;
}

// Gain of matching query character i at candidate column j, given the run bonus it continues.
int CellGain(const Scratch& s, int i, int j, int runBonus)
{
    int bonus = s.bonus[j];
    if (runBonus >= 0)
        bonus = std::max({bonus, runBonus, kBonusConsecutive});
    return kScoreMatch + bonus + CaseBonus(s.query[i], s.text[j]);
}

int LeadingPenalty(int j)
{
    return -std::min(j, kLeadingCap);
}

// Fills s.positions with the best placement and returns its score, or kNoScore.
int ScoreTable(Scratch& s, int m, int n)
{
    // Columns where each query character can match at all: after the earliest placement of the
    // characters before it, and before the latest placement of the ones after it.
    int lo[kFuzzyMaxQuery];
    int hi[kFuzzyMaxQuery];
    int j = 0;
    for (int i = 0; i < m; ++i)
    {
        while (j < n && s.text[j].folded != s.query[i].folded)
            ++j;
        if (j >= n)
            return kNoScore;
        lo[i] = j++;
    }
    j = n - 1;
    for (int i = m - 1; i >= 0; --i)
    {
        while (j >= 0 && s.text[j].folded != s.query[i].folded)
            --j;
        hi[i] = j--;
    }

    for (int c = 0; c < n; ++c)
        s.bonus[c] = PositionBonus(s.text, c);

    for (int i = 0; i < m; ++i)
    {
        // Best predecessor for a gap: max over k <= c - 2 of score[i-1][k] + gap penalty.
        int gapBest = kNoScore;
        int gapFrom = -1;
        // Row i scans from where a gap after row i - 1 can begin, so every predecessor counts.
        const int first = i > 0 ? std::min(lo[i], lo[i - 1] + 2) : lo[0];
        for (int c = first; c <= hi[i]; ++c)
        {
            if (i > 0)
            {
                if (gapBest != kNoScore)
                    gapBest += kGapExtend;
                const int k = c - 2;
                if (k >= lo[i - 1] && k <= hi[i - 1] && s.score[i - 1][k] != kNoScore)
                {
                    const int opened = s.score[i - 1][k] + kGapStart;
                    if (opened > gapBest)
                    {
                        gapBest = opened;
                        gapFrom = k;
                    }
                }
            }
            if (c < lo[i])
                continue;
            s.score[i][c] = kNoScore;
            if (s.text[c].folded != s.query[i].folded)
                continue;
            if (i == 0)
            {
                s.score[0][c] = CellGain(s, 0, c, -1) + LeadingPenalty(c);
                s.from[0][c] = -1;
                s.run[0][c] = static_cast<std::int8_t>(s.bonus[c]);
                continue;
            }
            int best = kNoScore;
            int bestFrom = -1;
            int bestRun = s.bonus[c];
            const int prev = c - 1;
            if (prev >= lo[i - 1] && prev <= hi[i - 1] && s.score[i - 1][prev] != kNoScore)
            {
                const int runBonus = s.run[i - 1][prev];
                best = s.score[i - 1][prev] + CellGain(s, i, c, runBonus);
                bestFrom = prev;
                bestRun = std::max(runBonus, s.bonus[c]);
            }
            if (gapBest != kNoScore)
            {
                const int viaGap = gapBest + CellGain(s, i, c, -1);
                if (viaGap > best)
                {
                    best = viaGap;
                    bestFrom = gapFrom;
                    bestRun = s.bonus[c];
                }
            }
            s.score[i][c] = best;
            s.from[i][c] = static_cast<std::int16_t>(bestFrom);
            s.run[i][c] = static_cast<std::int8_t>(bestRun);
        }
    }

    int best = kNoScore;
    int end = -1;
    for (int c = lo[m - 1]; c <= hi[m - 1]; ++c)
    {
        if (s.score[m - 1][c] > best)
        {
            best = s.score[m - 1][c];
            end = c;
        }
    }
    if (end < 0)
        return kNoScore;
    for (int i = m - 1; i >= 0; --i)
    {
        s.positions[i] = end;
        end = s.from[i][end];
    }
    return best;
}

// The fallback for long texts: each query character takes the next matching character,
// scored with the same rules. `query` and `text` are decoded on the fly.
int ScoreGreedy(std::string_view query, std::string_view text, std::span<TextRange> ranges, int* rangeCount)
{
    std::size_t tpos = 0;
    int score = 0;
    int prevIndex = -2;  // character index of the previous match
    int runBonus = -1;
    int index = 0;       // character index of tpos
    int queryIndex = 0;
    Char prevChar;
    prevChar.cls = CharClass::Separator;
    bool havePrev = false;
    *rangeCount = 0;
    for (std::size_t qpos = 0; qpos < query.size();)
    {
        int qlen = 1;
        Char q;
        q.original = Decode(query, qpos, &qlen);
        q.folded = Fold(q.original);
        q.cls = Classify(q.original);
        qpos += static_cast<std::size_t>(qlen);
        bool found = false;
        while (tpos < text.size())
        {
            int tlen = 1;
            Char c;
            c.original = Decode(text, tpos, &tlen);
            c.folded = Fold(c.original);
            c.cls = Classify(c.original);
            c.byteBegin = static_cast<int>(tpos);
            c.byteEnd = static_cast<int>(tpos) + tlen;
            Char pair[2] = {prevChar, c};
            const int bonus = PositionBonus(pair, havePrev ? 1 : 0);
            prevChar = c;
            havePrev = true;
            tpos += static_cast<std::size_t>(tlen);
            const int charIndex = index++;
            if (c.folded != q.folded)
                continue;
            int gain = bonus;
            const bool consecutive = charIndex == prevIndex + 1;
            if (consecutive && runBonus >= 0)
                gain = std::max({bonus, runBonus, kBonusConsecutive});
            score += kScoreMatch + gain + CaseBonus(q, c);
            if (queryIndex == 0)
                score += LeadingPenalty(charIndex);
            else if (!consecutive)
                score += kGapStart + (charIndex - prevIndex - 2) * kGapExtend;
            runBonus = consecutive ? std::max(runBonus, bonus) : bonus;
            if (consecutive)
            {
                if (*rangeCount <= static_cast<int>(ranges.size()))
                    ranges[static_cast<std::size_t>(*rangeCount - 1)].end = c.byteEnd;
            }
            else
            {
                if (*rangeCount < static_cast<int>(ranges.size()))
                    ranges[static_cast<std::size_t>(*rangeCount)] = {c.byteBegin, c.byteEnd};
                ++*rangeCount;
            }
            prevIndex = charIndex;
            found = true;
            break;
        }
        if (!found)
            return kNoScore;
        ++queryIndex;
    }
    return score;
}

std::string_view TrimSpaces(std::string_view text)
{
    while (!text.empty() && text.front() == ' ')
        text.remove_prefix(1);
    while (!text.empty() && text.back() == ' ')
        text.remove_suffix(1);
    return text;
}

// The candidate equals the query, compared folded. Both are decoded.
int ExactBonus(const Scratch& s, int m, int n)
{
    if (n != m)
        return 0;
    for (int i = 0; i < m; ++i)
    {
        if (s.text[i].folded != s.query[i].folded)
            return 0;
    }
    return kBonusExact;
}

} // namespace

FuzzyMatch FuzzyScore(std::string_view query, std::string_view candidate, std::span<TextRange> ranges)
{
    FuzzyMatch result;
    query = TrimSpaces(query);
    if (query.empty())
    {
        result.matched = true;
        return result;
    }
    if (candidate.empty())
        return result;

    Scratch& s = ThreadScratch();
    const int m = DecodeText(query, s.query, kFuzzyMaxQuery);
    const int n = DecodeText(candidate, s.text, kFuzzyMaxCandidate);
    if (m > n)
        return result;
    if (m > kFuzzyMaxQuery || n > kFuzzyMaxCandidate)
    {
        int rangeCount = 0;
        const int score = ScoreGreedy(query, candidate, ranges, &rangeCount);
        if (score == kNoScore)
            return result;
        result.matched = true;
        result.score = score;
        result.rangeCount = rangeCount;
        return result;
    }

    const int score = ScoreTable(s, m, n);
    if (score == kNoScore)
        return result;
    result.matched = true;
    result.score = score + ExactBonus(s, m, n);

    // Byte ranges of the matched characters, adjacent characters merged.
    int count = 0;
    for (int i = 0; i < m; ++i)
    {
        const Char& c = s.text[s.positions[i]];
        const bool joins = i > 0 && s.positions[i] == s.positions[i - 1] + 1;
        if (joins)
        {
            if (count <= static_cast<int>(ranges.size()))
                ranges[static_cast<std::size_t>(count - 1)].end = c.byteEnd;
            continue;
        }
        if (count < static_cast<int>(ranges.size()))
            ranges[static_cast<std::size_t>(count)] = {c.byteBegin, c.byteEnd};
        ++count;
    }
    result.rangeCount = count;
    return result;
}

namespace
{

struct SpanSource
{
    std::span<const char* const> items;
};

const char* SpanGetter(int index, void* user)
{
    const auto* source = static_cast<const SpanSource*>(user);
    return source->items[static_cast<std::size_t>(index)];
}

// Ranks a before b: higher score, then lower index.
bool RanksBefore(const FuzzyResult& a, const FuzzyResult& b)
{
    return a.score != b.score ? a.score > b.score : a.index < b.index;
}

} // namespace

int FuzzyFilter(std::string_view query, std::span<const char* const> candidates, std::span<FuzzyResult> out,
                int* totalMatches)
{
    SpanSource source{candidates};
    return FuzzyFilter(query, static_cast<int>(candidates.size()), &SpanGetter, &source, out, totalMatches);
}

int FuzzyFilter(std::string_view query, int candidateCount, FuzzyGetter getter, void* user,
                std::span<FuzzyResult> out, int* totalMatches)
{
    int total = 0;
    int written = 0;
    const int capacity = static_cast<int>(out.size());
    int worst = -1;  // index into out of the entry that ranks last, once out is full
    if (getter == nullptr || candidateCount < 0)
        candidateCount = 0;
    for (int index = 0; index < candidateCount; ++index)
    {
        const char* text = getter(index, user);
        if (text == nullptr)
            continue;
        const FuzzyMatch match = FuzzyScore(query, text);
        if (!match.matched)
            continue;
        ++total;
        const FuzzyResult entry{index, match.score};
        if (written < capacity)
        {
            out[static_cast<std::size_t>(written++)] = entry;
            continue;
        }
        if (capacity == 0)
            continue;
        if (worst < 0)
        {
            worst = 0;
            for (int k = 1; k < capacity; ++k)
                if (RanksBefore(out[static_cast<std::size_t>(worst)], out[static_cast<std::size_t>(k)]))
                    worst = k;
        }
        // A later candidate with an equal score ranks after every kept one.
        if (entry.score > out[static_cast<std::size_t>(worst)].score)
        {
            out[static_cast<std::size_t>(worst)] = entry;
            worst = -1;
        }
    }
    std::sort(out.begin(), out.begin() + written, RanksBefore);
    if (totalMatches != nullptr)
        *totalMatches = total;
    return written;
}

} // namespace facet::internal
