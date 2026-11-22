// The built-in strings (widgets/common.h): templates filled by FormatTokens, and every
// Strings member that a header or a doc names exists in the table.

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <regex>
#include <set>
#include <sstream>
#include <string>

#include <doctest/doctest.h>

#include <facet/facet.h>

#include "internal/text_util.h"

using facet::internal::FormatTokens;
using facet::internal::TextToken;

namespace
{

std::string ReadFile(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

// The member names of `struct Strings` in common.h.
std::set<std::string> StringMembers(const std::filesystem::path& root)
{
    const std::string header = ReadFile(root / "include/facet/widgets/common.h");
    const std::size_t begin = header.find("struct Strings");
    const std::size_t end = header.find("};", begin);
    std::set<std::string> names;
    if (begin == std::string::npos || end == std::string::npos)
        return names;
    const std::string body = header.substr(begin, end - begin);
    const std::regex member(R"(const char\* (\w+) =)");
    for (auto it = std::sregex_iterator(body.begin(), body.end(), member); it != std::sregex_iterator(); ++it)
        names.insert((*it)[1].str());
    return names;
}

} // namespace

TEST_CASE("Strings templates: named tokens are filled and everything else stays literal")
{
    const TextToken tokens[] = {{"page", 3}, {"count", 12}};
    char out[64];
    CHECK(FormatTokens(out, sizeof(out), "Page {page} of {count}", tokens) == 12);
    CHECK(std::strcmp(out, "Page 3 of 12") == 0);
    // A translation may reorder the tokens.
    FormatTokens(out, sizeof(out), "{count} pages, this is {page}", tokens);
    CHECK(std::strcmp(out, "12 pages, this is 3") == 0);
    // printf specifiers, unknown tokens and lone braces are text.
    FormatTokens(out, sizeof(out), "%s%n%d {other} { {page", tokens);
    CHECK(std::strcmp(out, "%s%n%d {other} { {page") == 0);
    // Too small a buffer cuts at a whole UTF-8 character and stays terminated.
    char small[6];
    FormatTokens(small, sizeof(small), "Seite {page} \xC3\xBC", tokens);
    CHECK(std::strcmp(small, "Seite") == 0);
    char tiny[10];  // room for the first byte of the two-byte character, which must not be written
    FormatTokens(tiny, sizeof(tiny), "Seite 3 \xC3\xBC", {});
    CHECK(std::strcmp(tiny, "Seite 3 ") == 0);
    FormatTokens(out, sizeof(out), nullptr, tokens);
    CHECK(out[0] == '\0');
}

TEST_CASE("Strings templates: the defaults fill as documented, also after SetStrings")
{
    const TextToken tokens[] = {{"page", 2}, {"count", 5}};
    char out[64];
    FormatTokens(out, sizeof(out), facet::GetStrings().pageOf, tokens);
    CHECK(std::strcmp(out, "Page 2 of 5") == 0);
    const TextToken one[] = {{"count", 1}};
    FormatTokens(out, sizeof(out), facet::GetStrings().unsavedChange, one);
    CHECK(std::strcmp(out, "1 unsaved change") == 0);
    FormatTokens(out, sizeof(out), facet::GetStrings().unsavedChanges, tokens);
    CHECK(std::strcmp(out, "5 unsaved changes") == 0);

    facet::Strings translated;
    translated.pageOf = "%s {count}: {page}";
    facet::SetStrings(translated);
    FormatTokens(out, sizeof(out), facet::GetStrings().pageOf, tokens);
    CHECK(std::strcmp(out, "%s 5: 2") == 0);
    CHECK(std::strcmp(facet::GetStrings().jumpToLatest, "Jump to latest") == 0);
    facet::SetStrings(facet::Strings{});
    // The "+N" chip of the second widget wave, and a member far down the table kept by SetStrings.
    const TextToken more[] = {{"count", 7}};
    FormatTokens(out, sizeof(out), facet::GetStrings().moreCount, more);
    CHECK(std::strcmp(out, "+7") == 0);
    translated = facet::Strings{};
    translated.playhead = "Position";
    facet::SetStrings(translated);
    CHECK(std::strcmp(facet::GetStrings().playhead, "Position") == 0);
    CHECK(std::strcmp(facet::GetStrings().loopEnd, "Loop end") == 0);
    facet::SetStrings(facet::Strings{});
}

TEST_CASE("Strings templates: a text token is copied as it is, never read as a template")
{
    const TextToken tokens[] = {{"name", 0, "Preset {count} 100%"}, {"count", 4}};
    char out[96];
    FormatTokens(out, sizeof(out), facet::GetStrings().deletePresetMessage, tokens);
    CHECK(std::strcmp(out, "\"Preset {count} 100%\" will be deleted. This cannot be undone.") == 0);
    FormatTokens(out, sizeof(out), facet::GetStrings().conflictWith, tokens);
    CHECK(std::strcmp(out, "Also bound to Preset {count} 100%") == 0);
    // An empty text writes nothing, and a text too long for the buffer is cut at a whole character.
    const TextToken empty[] = {{"name", 0, ""}};
    FormatTokens(out, sizeof(out), "[{name}]", empty);
    CHECK(std::strcmp(out, "[]") == 0);
    const TextToken umlaut[] = {{"name", 0, "ab\xC3\xBC"}};
    char small[4];
    FormatTokens(small, sizeof(small), "{name}", umlaut);
    CHECK(std::strcmp(small, "ab") == 0);
}

TEST_CASE("Strings templates: the composite templates fill as documented")
{
    char out[64];
    const TextToken results[] = {{"count", 12}};
    FormatTokens(out, sizeof(out), facet::GetStrings().resultCount, results);
    CHECK(std::strcmp(out, "12 results") == 0);
    const TextToken one[] = {{"count", 1}};
    FormatTokens(out, sizeof(out), facet::GetStrings().resultCountOne, one);
    CHECK(std::strcmp(out, "1 result") == 0);
    const TextToken scale[] = {{"percent", 125}};
    FormatTokens(out, sizeof(out), facet::GetStrings().percent, scale);
    CHECK(std::strcmp(out, "125 %") == 0);
    const TextToken step[] = {{"step", 2}, {"count", 3}};
    FormatTokens(out, sizeof(out), facet::GetStrings().stepOf, step);
    CHECK(std::strcmp(out, "Step 2 of 3") == 0);
}

TEST_CASE("Every Strings member named in a header or a doc exists")
{
    const std::filesystem::path root = FACET_TEST_SOURCE_DIR;
    const std::set<std::string> members = StringMembers(root);
    REQUIRE(members.size() > 30);
    CHECK(members.count("pageOf") == 1);

    std::set<std::filesystem::path> files;
    for (const char* dir : {"include/facet/widgets", "docs"})
    {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root / dir))
        {
            const std::string ext = entry.path().extension().string();
            if (entry.is_regular_file() && (ext == ".h" || ext == ".md"))
                files.insert(entry.path());
        }
    }
    files.insert(root / "README.md");
    REQUIRE(files.size() > 10);

    const std::regex reference(R"(Strings::(\w+))");
    int references = 0;
    for (const std::filesystem::path& file : files)
    {
        const std::string text = ReadFile(file);
        for (auto it = std::sregex_iterator(text.begin(), text.end(), reference); it != std::sregex_iterator(); ++it)
        {
            const std::string name = (*it)[1].str();
            ++references;
            INFO(file.filename().string() << " names Strings::" << name);
            CHECK(members.count(name) == 1);
        }
    }
    CHECK(references > 20);
}
