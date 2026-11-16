// Warnings for problems the library recovers from (a missing font, an unusable setting).

#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "core/core_internal.h"

namespace facet::internal
{

void LogWarning(const char* format, ...)
{
    char message[512];
    constexpr char kPrefix[] = "facet: warning: ";
    std::snprintf(message, sizeof(message), "%s", kPrefix);
    const size_t prefixLength = sizeof(kPrefix) - 1;

    va_list args;
    va_start(args, format);
    std::vsnprintf(message + prefixLength, sizeof(message) - prefixLength - 1, format, args);
    va_end(args);

    size_t length = std::strlen(message);
    if (length + 1 < sizeof(message))
    {
        message[length] = '\n';
        message[length + 1] = '\0';
    }
    OutputDebugStringA(message);
    std::fputs(message, stderr);
}

} // namespace facet::internal
