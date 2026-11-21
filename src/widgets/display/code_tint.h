#pragma once

// The light syntax tint of CodeView (facet/widgets/display.h): splits one line of code into runs
// of one kind each. It knows comments, string literals, numbers and keywords of C++ (with the
// preprocessor and block comments across lines), JSON (keys, strings, numbers, true, false, null)
// and INI files (sections, keys, values, comments); anything else is plain. It does not parse the
// language, so an unusual construct may be tinted wrongly, never dropped.
//
//     bool inBlock = lineStartsInBlockComment;
//     const char* p = lineBegin;
//     while (p < lineEnd)
//     {
//         const CodeTintRun run = NextCodeTintRun(language, lineBegin, &p, lineEnd, &inBlock);
//         ... draw [run.begin, run.end) in the color of run.kind ...
//     }

#include "facet/widgets/display.h"

namespace facet::internal
{

enum class CodeTintKind
{
    Plain,
    Keyword,  // keywords, the preprocessor, JSON literals, INI sections
    String,
    Number,
    Comment,
};

struct CodeTintRun
{
    const char* begin = nullptr;
    const char* end = nullptr;
    CodeTintKind kind = CodeTintKind::Plain;
};

// The run starting at *p (inside [lineBegin, lineEnd), a line without its line break); moves *p to
// its end, which is always past *p while *p < lineEnd. *inBlock carries a C++ block comment from one
// line to the next.
CodeTintRun NextCodeTintRun(CodeLanguage language, const char* lineBegin, const char** p, const char* lineEnd,
                            bool* inBlock);

// Whether a C++ block comment is still open at the end of [lineBegin, lineEnd), given whether one
// was open at its start. Other languages have none.
bool CodeTintEndsInBlock(CodeLanguage language, const char* lineBegin, const char* lineEnd, bool startsInBlock);

} // namespace facet::internal
