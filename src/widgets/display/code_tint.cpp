// The syntax tint of CodeView (see code_tint.h).

#include "widgets/display/code_tint.h"

#include <cstring>
#include <string_view>

namespace facet::internal
{
namespace
{

constexpr std::string_view kCppKeywords[] = {
    "alignas",   "alignof",      "auto",          "bool",       "break",     "case",         "catch",
    "char",      "char8_t",      "char16_t",      "char32_t",   "class",     "concept",      "const",
    "consteval", "constexpr",    "constinit",     "const_cast", "continue",  "co_await",     "co_return",
    "co_yield",  "decltype",     "default",       "delete",     "do",        "double",       "dynamic_cast",
    "else",      "enum",         "explicit",      "export",     "extern",    "false",        "final",
    "float",     "for",          "friend",        "goto",       "if",        "inline",       "int",
    "long",      "mutable",      "namespace",     "new",        "noexcept",  "nullptr",      "operator",
    "override",  "private",      "protected",     "public",     "register",  "reinterpret_cast", "requires",
    "return",    "short",        "signed",        "sizeof",     "static",    "static_assert", "static_cast",
    "struct",    "switch",       "template",      "this",       "thread_local", "throw",     "true",
    "try",       "typedef",      "typeid",        "typename",   "union",     "unsigned",     "using",
    "virtual",   "void",         "volatile",      "wchar_t",    "while",
};

bool IsIdentStart(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || (static_cast<unsigned char>(c) >= 0x80);
}

bool IsIdentChar(char c)
{
    return IsIdentStart(c) || (c >= '0' && c <= '9');
}

bool IsDigit(char c)
{
    return c >= '0' && c <= '9';
}

bool IsSpace(char c)
{
    return c == ' ' || c == '\t' || c == '\r';
}

// The first character of the line that is not a space, or lineEnd.
const char* FirstNonSpace(const char* lineBegin, const char* lineEnd)
{
    const char* p = lineBegin;
    while (p < lineEnd && IsSpace(*p))
        ++p;
    return p;
}

const char* SkipString(const char* p, const char* lineEnd)
{
    const char quote = *p++;
    while (p < lineEnd)
    {
        if (*p == '\\' && p + 1 < lineEnd)
        {
            p += 2;
            continue;
        }
        if (*p++ == quote)
            break;
    }
    return p;
}

const char* SkipNumber(const char* p, const char* lineEnd)
{
    while (p < lineEnd && (IsIdentChar(*p) || *p == '.' || *p == '\'' ||
                           ((*p == '+' || *p == '-') && (p[-1] == 'e' || p[-1] == 'E' || p[-1] == 'p' || p[-1] == 'P'))))
        ++p;
    return p;
}

bool IsKeyword(std::string_view word, std::span<const std::string_view> words)
{
    for (const std::string_view keyword : words)
        if (keyword == word)
            return true;
    return false;
}

CodeTintRun Cpp(const char* lineBegin, const char* p, const char* lineEnd, bool* inBlock)
{
    if (*inBlock)
    {
        const char* close = p;
        while (close + 1 < lineEnd && !(close[0] == '*' && close[1] == '/'))
            ++close;
        if (close + 1 < lineEnd)
        {
            *inBlock = false;
            return {p, close + 2, CodeTintKind::Comment};
        }
        return {p, lineEnd, CodeTintKind::Comment};
    }
    const char c = *p;
    if (IsSpace(c))
    {
        const char* end = p;
        while (end < lineEnd && IsSpace(*end))
            ++end;
        return {p, end, CodeTintKind::Plain};
    }
    if (c == '#' && p == FirstNonSpace(lineBegin, lineEnd))
    {
        // The preprocessor line up to a comment.
        const char* end = p + 1;
        while (end < lineEnd && !(end + 1 < lineEnd && end[0] == '/' && (end[1] == '/' || end[1] == '*')))
            ++end;
        return {p, end, CodeTintKind::Keyword};
    }
    if (c == '/' && p + 1 < lineEnd && p[1] == '/')
        return {p, lineEnd, CodeTintKind::Comment};
    if (c == '/' && p + 1 < lineEnd && p[1] == '*')
    {
        *inBlock = true;
        CodeTintRun run = Cpp(lineBegin, p + 2, lineEnd, inBlock);
        run.begin = p;
        return run;
    }
    if (c == '"' || c == '\'')
        return {p, SkipString(p, lineEnd), CodeTintKind::String};
    if (IsDigit(c) || (c == '.' && p + 1 < lineEnd && IsDigit(p[1])))
        return {p, SkipNumber(p + 1, lineEnd), CodeTintKind::Number};
    if (IsIdentStart(c))
    {
        const char* end = p + 1;
        while (end < lineEnd && IsIdentChar(*end))
            ++end;
        const std::string_view word(p, static_cast<std::size_t>(end - p));
        return {p, end, IsKeyword(word, kCppKeywords) ? CodeTintKind::Keyword : CodeTintKind::Plain};
    }
    return {p, p + 1, CodeTintKind::Plain};
}

CodeTintRun Json(const char* p, const char* lineEnd)
{
    static constexpr std::string_view kLiterals[] = {"true", "false", "null"};
    const char c = *p;
    if (c == '"')
    {
        const char* end = SkipString(p, lineEnd);
        // A string followed by a colon is a key: plain, so keys and values read apart.
        const char* after = end;
        while (after < lineEnd && IsSpace(*after))
            ++after;
        return {p, end, after < lineEnd && *after == ':' ? CodeTintKind::Plain : CodeTintKind::String};
    }
    if (IsDigit(c) || (c == '-' && p + 1 < lineEnd && IsDigit(p[1])))
        return {p, SkipNumber(p + 1, lineEnd), CodeTintKind::Number};
    if (IsIdentStart(c))
    {
        const char* end = p + 1;
        while (end < lineEnd && IsIdentChar(*end))
            ++end;
        const std::string_view word(p, static_cast<std::size_t>(end - p));
        return {p, end, IsKeyword(word, kLiterals) ? CodeTintKind::Keyword : CodeTintKind::Plain};
    }
    const char* end = p + 1;
    while (end < lineEnd && !(*end == '"' || IsDigit(*end) || *end == '-' || IsIdentStart(*end)))
        ++end;
    return {p, end, CodeTintKind::Plain};
}

CodeTintRun Ini(const char* lineBegin, const char* p, const char* lineEnd)
{
    const char* first = FirstNonSpace(lineBegin, lineEnd);
    if (first < lineEnd && (*first == ';' || *first == '#'))
        return {p, lineEnd, CodeTintKind::Comment};
    if (first < lineEnd && *first == '[')
        return {p, lineEnd, CodeTintKind::Keyword};
    const char* equals = static_cast<const char*>(std::memchr(first, '=', static_cast<std::size_t>(lineEnd - first)));
    if (equals == nullptr || p <= equals)
    {
        // The key up to the '=' is plain; so is the '=' itself.
        const char* end = equals != nullptr && p < equals ? equals : (equals != nullptr ? equals + 1 : lineEnd);
        return {p, end, CodeTintKind::Plain};
    }
    // The value: a quoted string, a number, or plain text up to a comment.
    const char* start = p;
    while (start < lineEnd && IsSpace(*start))
        ++start;
    if (start > p)
        return {p, start, CodeTintKind::Plain};
    if (*p == ';' || *p == '#')
        return {p, lineEnd, CodeTintKind::Comment};
    if (*p == '"' || *p == '\'')
        return {p, SkipString(p, lineEnd), CodeTintKind::String};
    if (IsDigit(*p) || (*p == '-' && p + 1 < lineEnd && IsDigit(p[1])))
        return {p, SkipNumber(p + 1, lineEnd), CodeTintKind::Number};
    const char* end = p + 1;
    while (end < lineEnd && *end != ';' && *end != '#')
        ++end;
    return {p, end, CodeTintKind::String};
}

} // namespace

CodeTintRun NextCodeTintRun(CodeLanguage language, const char* lineBegin, const char** p, const char* lineEnd, bool* inBlock)
{
    CodeTintRun run{*p, lineEnd, CodeTintKind::Plain};
    if (*p >= lineEnd)
    {
        run.end = *p;
        return run;
    }
    bool block = inBlock != nullptr && *inBlock;
    switch (language)
    {
    case CodeLanguage::Cpp:
        run = Cpp(lineBegin, *p, lineEnd, &block);
        break;
    case CodeLanguage::Json:
        run = Json(*p, lineEnd);
        break;
    case CodeLanguage::Ini:
        run = Ini(lineBegin, *p, lineEnd);
        break;
    case CodeLanguage::Plain:
    default:
        break;
    }
    if (run.end <= *p)
        run.end = *p + 1;  // always advance
    if (run.end > lineEnd)
        run.end = lineEnd;
    if (inBlock != nullptr)
        *inBlock = block;
    *p = run.end;
    return run;
}

bool CodeTintEndsInBlock(CodeLanguage language, const char* lineBegin, const char* lineEnd, bool startsInBlock)
{
    if (language != CodeLanguage::Cpp)
        return false;
    bool block = startsInBlock;
    const char* p = lineBegin;
    while (p < lineEnd)
        NextCodeTintRun(language, lineBegin, &p, lineEnd, &block);
    return block;
}

} // namespace facet::internal
