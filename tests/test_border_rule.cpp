// The border rule of DESIGN.md: every outline is one hairline. A source scan over the outline
// strokes of the library and the test app (StrokeRect, StrokeRoundRect, StrokeContinuousRect,
// StrokeArc, ImDrawList::AddRect and AddCircle, and PathStroke and AddPolyline of a closed path)
// checks that each one takes HairlineThickness() or a variable the same file assigns exactly that,
// so no outline takes a width of its own.
//
// Strokes that are not outlines keep their own weights (the focus ring, glyphs): they are listed as
// exceptions with their reason. Files that still break the rule are listed as pending until their
// family moves onto the outline helpers of src/internal/control.h; a pending file that no longer
// breaks it fails the test, so the list only shrinks. Both lists live in tests/border_rule/, one
// file per group of families.

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <doctest/doctest.h>

namespace
{

struct BorderRuleException
{
    const char* path;      // relative to the source directory, forward slashes
    const char* argument;  // the width argument as written
    const char* reason;
};

struct BorderRulePending
{
    const char* path;
};

#define FACET_BORDER_EXCEPTION(path, argument, reason) BorderRuleException{path, argument, reason},
#define FACET_BORDER_PENDING(path)
const BorderRuleException kExceptions[] = {
    // The stroke helpers themselves and the focus ring.
    FACET_BORDER_EXCEPTION("src/core/draw.cpp", "thickness", "StrokeRoundRect forwards its own parameter")
    FACET_BORDER_EXCEPTION("src/core/squircle.cpp", "thickness", "StrokeContinuousRect forwards its own parameter")
    FACET_BORDER_EXCEPTION("src/internal/focus.cpp", "width", "the focus ring: metrics.focusRingWidth, not an outline")
    FACET_BORDER_EXCEPTION("src/internal/focus.cpp", "FocusRingWidth()", "the focus ring inside rows and tiles")
    FACET_BORDER_EXCEPTION("src/image/icon_atlas.cpp", "thickness", "the crossed box glyph of a missing icon, a hairline")
#include "border_rule/controls.inc"
#include "border_rule/surfaces.inc"
#include "border_rule/data.inc"
};
#undef FACET_BORDER_EXCEPTION
#undef FACET_BORDER_PENDING

#define FACET_BORDER_EXCEPTION(path, argument, reason)
#define FACET_BORDER_PENDING(path) BorderRulePending{path},
const BorderRulePending kPending[] = {
    BorderRulePending{""},  // keeps the array valid while every list is empty
#include "border_rule/controls.inc"
#include "border_rule/surfaces.inc"
#include "border_rule/data.inc"
};
#undef FACET_BORDER_EXCEPTION
#undef FACET_BORDER_PENDING

// The stroke calls and the index of their width argument.
struct StrokeCall
{
    const char* name;  // the call as written, up to and with the opening parenthesis
    int width;         // index of the width argument; a call that leaves it out uses a 1 px default
    bool member;       // an ImDrawList member, written after "->"
    int closedFlags;   // index of the flags argument: only a closed path is an outline; -1: always
};

constexpr StrokeCall kCalls[] = {
    {"StrokeRect(", 5, false, -1},
    {"StrokeRoundRect(", 5, false, -1},
    {"StrokeContinuousRect(", 5, false, -1},
    {"StrokeArc(", 6, false, -1},
    {"AddRect(", 4, true, -1},
    {"AddCircle(", 4, true, -1},
    {"PathStroke(", 1, true, 2},
    {"AddPolyline(", 3, true, 4},
};

std::string ReadFile(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    std::stringstream text;
    text << file.rdbuf();
    return text.str();
}

bool InLineComment(const std::string& source, std::size_t at)
{
    const std::size_t lineStart = source.rfind('\n', at);
    const std::size_t from = lineStart == std::string::npos ? 0 : lineStart + 1;
    const std::size_t comment = source.find("//", from);
    return comment != std::string::npos && comment < at;
}

// The top-level arguments of the call whose opening parenthesis is at `open`, trimmed. Empty when
// the parentheses do not close.
std::vector<std::string> Arguments(const std::string& source, std::size_t open)
{
    std::vector<std::string> args;
    std::string current;
    int depth = 0;
    for (std::size_t i = open + 1; i < source.size(); ++i)
    {
        const char c = source[i];
        if (c == '"' || c == '\'')
        {
            // Skip a string or character literal.
            const char quote = c;
            current += c;
            for (++i; i < source.size() && source[i] != quote; ++i)
            {
                current += source[i];
                if (source[i] == '\\' && i + 1 < source.size())
                    current += source[++i];
            }
            current += quote;
            continue;
        }
        if (c == '(' || c == '{' || c == '[')
            ++depth;
        if (c == ')' || c == '}' || c == ']')
        {
            if (depth == 0)
            {
                args.push_back(current);
                break;
            }
            --depth;
        }
        if (c == ',' && depth == 0)
        {
            args.push_back(current);
            current.clear();
            continue;
        }
        current += c;
    }
    for (std::string& arg : args)
    {
        std::string trimmed;
        bool space = false;
        for (const char c : arg)
        {
            if (std::isspace(static_cast<unsigned char>(c)) != 0)
            {
                space = !trimmed.empty();
                continue;
            }
            if (space)
                trimmed += ' ';
            space = false;
            trimmed += c;
        }
        arg = trimmed;
    }
    return args;
}

bool IsHairlineCall(const std::string& text)
{
    return text == "HairlineThickness()" || text == "facet::HairlineThickness()" || text == "internal::HairlineThickness()" ||
           text == "facet::internal::HairlineThickness()";
}

// A width that follows the rule: HairlineThickness() itself, or one identifier (a variable or a
// member, the part after the last '.') that `source` assigns HairlineThickness() and nothing else:
// "name = HairlineThickness();" or ".name = HairlineThickness()," in an initializer.
bool IsHairline(const std::string& argument, const std::string& source)
{
    if (IsHairlineCall(argument))
        return true;
    if (argument.empty())
        return false;
    for (const char c : argument)
        if (std::isalnum(static_cast<unsigned char>(c)) == 0 && c != '_' && c != '.')
            return false;
    const std::size_t dot = argument.rfind('.');
    const std::string name = argument.substr(dot == std::string::npos ? 0 : dot + 1);
    if (name.empty() || std::isdigit(static_cast<unsigned char>(name[0])) != 0)
        return false;
    std::size_t at = 0;
    while ((at = source.find(name, at)) != std::string::npos)
    {
        const std::size_t start = at;
        at += name.size();
        if (start > 0 && (std::isalnum(static_cast<unsigned char>(source[start - 1])) != 0 || source[start - 1] == '_'))
            continue;
        std::size_t i = at;
        while (i < source.size() && source[i] == ' ')
            ++i;
        if (i >= source.size() || source[i] != '=' || (i + 1 < source.size() && source[i + 1] == '='))
            continue;
        ++i;
        while (i < source.size() && source[i] == ' ')
            ++i;
        const std::size_t end = source.find_first_of(";,}", i);
        if (end == std::string::npos)
            continue;
        std::string value = source.substr(i, end - i);
        while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0)
            value.pop_back();
        if (IsHairlineCall(value))
            return true;
    }
    return false;
}

struct Violation
{
    std::string path;
    std::string call;
    std::string argument;
    int line = 0;
};

std::vector<Violation> ScanFile(const std::string& relative, const std::string& source)
{
    std::vector<Violation> found;
    for (const StrokeCall& call : kCalls)
    {
        const std::string name = call.name;
        std::size_t at = 0;
        while ((at = source.find(name, at)) != std::string::npos)
        {
            const std::size_t start = at;
            at += name.size();
            if (call.member ? (start < 2 || source.compare(start - 2, 2, "->") != 0)
                            : (start > 0 && (std::isalnum(static_cast<unsigned char>(source[start - 1])) != 0 ||
                                             source[start - 1] == '_')))
                continue;
            if (InLineComment(source, start))
                continue;
            // A declaration or definition of a helper (a type in front) is not a call.
            const std::size_t lineStart = source.rfind('\n', start);
            const std::string before =
                source.substr(lineStart == std::string::npos ? 0 : lineStart + 1,
                              start - (lineStart == std::string::npos ? 0 : lineStart + 1));
            if (before.find("void ") != std::string::npos)
                continue;
            const std::vector<std::string> args = Arguments(source, start + name.size() - 1);
            if (call.closedFlags >= 0 && (static_cast<int>(args.size()) <= call.closedFlags ||
                                          args[static_cast<std::size_t>(call.closedFlags)].find("Closed") == std::string::npos))
                continue;  // an open path: a line or a curve, not an outline
            const std::string width =
                static_cast<int>(args.size()) > call.width ? args[static_cast<std::size_t>(call.width)] : "(default 1 px)";
            if (IsHairline(width, source))
                continue;
            bool excepted = false;
            for (const BorderRuleException& exception : kExceptions)
                excepted = excepted || (relative == exception.path && width == exception.argument);
            if (excepted)
                continue;
            Violation violation;
            violation.path = relative;
            violation.call = name;
            violation.argument = width;
            violation.line = 1 + static_cast<int>(std::count(source.begin(), source.begin() + static_cast<long long>(start), '\n'));
            found.push_back(violation);
        }
    }
    return found;
}

bool IsPending(const std::string& relative)
{
    for (const BorderRulePending& pending : kPending)
        if (relative == pending.path)
            return true;
    return false;
}

} // namespace

TEST_CASE("Border rule: every outline stroke in the library and the test app is one hairline")
{
    const std::filesystem::path root = std::filesystem::path(FACET_TEST_SOURCE_DIR);
    int scanned = 0;
    std::vector<std::string> stillPending;
    for (const char* folder : {"src", "demo"})
    {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root / folder))
        {
            const std::filesystem::path& path = entry.path();
            if (!entry.is_regular_file() || (path.extension() != ".cpp" && path.extension() != ".h"))
                continue;
            ++scanned;
            const std::string relative = std::filesystem::relative(path, root).generic_string();
            const std::vector<Violation> violations = ScanFile(relative, ReadFile(path));
            if (IsPending(relative))
            {
                // A pending file that follows the rule now must leave the list.
                INFO("pending file that follows the border rule now; remove it from tests/border_rule: " << relative);
                CHECK_FALSE(violations.empty());
                continue;
            }
            for (const Violation& violation : violations)
            {
                INFO(violation.path << ":" << violation.line << " " << violation.call << " width " << violation.argument);
                CHECK(violation.argument.empty());
            }
        }
    }
    CHECK(scanned > 100);  // the source tree was found
}

TEST_CASE("Border rule: the scan recognises hairline widths and finds other widths")
{
    const std::string defined = "const float hairline = HairlineThickness();\nlook.edge = facet::HairlineThickness();\n"
                                "const float twoHairlines = HairlineThickness() * 2.0f;\n";
    CHECK(IsHairline("HairlineThickness()", ""));
    CHECK(IsHairline("hairline", defined));
    CHECK(IsHairline("look.edge", defined));
    CHECK_FALSE(IsHairline("hairline", ""));
    CHECK_FALSE(IsHairline("twoHairlines", defined));
    CHECK_FALSE(IsHairline("hairline * 2.0f", defined));
    CHECK_FALSE(IsHairline("2.0f", defined));
    CHECK_FALSE(IsHairline("Px(T().metrics.focusRingWidth)", defined));
    const std::string source =
        "void F()\n{\n    StrokeRect(dl, a, b, c, r, HairlineThickness());\n    StrokeRect(dl, a, b, c, r, 2.0f);\n"
        "    // StrokeRect(dl, a, b, c, r, 3.0f);\n    dl->AddRect(a, b, c, r, Px(2.0f), 0);\n    dl->AddRectFilled(a, b, c);\n"
        "    StrokeRect(dl, a, b, Color(1, 2), r);\n    StrokeArc(dl, p, r, 0.0f, kTurn, c, HairlineThickness() * 2.0f, false);\n"
        "    dl->PathStroke(c, 3.0f, ImDrawFlags_Closed);\n    dl->PathStroke(c, 3.0f);\n"
        "    dl->AddPolyline(p, n, c, 2.0f, ImDrawFlags_Closed);\n    dl->AddPolyline(p, n, c, 2.0f);\n}\n";
    const std::vector<Violation> found = ScanFile("example.cpp", source);
    REQUIRE(found.size() == 6);
    // ScanFile reports by call kind in the order of kCalls.
    CHECK(found[0].argument == "2.0f");
    CHECK(found[0].line == 4);
    CHECK(found[1].argument == "(default 1 px)");
    CHECK(found[2].argument == "HairlineThickness() * 2.0f");
    CHECK(found[3].argument == "Px(2.0f)");
    CHECK(found[4].argument == "3.0f");
    CHECK(found[5].argument == "2.0f");
}
