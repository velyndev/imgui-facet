// Warnings for problems the library recovers from (a missing font, an unusable setting), and
// reports of API misuse (an unbalanced Push and Pop).

#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include <imgui.h>
#include <imgui_internal.h>

#include "core/core_internal.h"

namespace facet::internal
{
namespace
{

constexpr size_t kMessageSize = 512;

// Writes `prefix` and the formatted text into `message`, followed by a line break.
void Format(char (&message)[kMessageSize], const char* prefix, const char* format, va_list args)
{
    std::snprintf(message, sizeof(message), "%s", prefix);
    const size_t prefixLength = std::strlen(message);
    std::vsnprintf(message + prefixLength, sizeof(message) - prefixLength - 1, format, args);
    const size_t length = std::strlen(message);
    if (length + 1 < sizeof(message))
    {
        message[length] = '\n';
        message[length + 1] = '\0';
    }
}

void Emit(const char* message)
{
    OutputDebugStringA(message);
    std::fputs(message, stderr);
}

int& UsageErrors()
{
    static int count = 0;
    return count;
}

} // namespace

int UsageErrorCount()
{
    return UsageErrors();
}

void LogWarning(const char* format, ...)
{
    char message[kMessageSize];
    va_list args;
    va_start(args, format);
    Format(message, "facet: warning: ", format, args);
    va_end(args);
    Emit(message);
}

void ReportUsageError(const char* format, ...)
{
    char message[kMessageSize];
    va_list args;
    va_start(args, format);
    Format(message, "facet: error: ", format, args);
    va_end(args);
    Emit(message);
    ++UsageErrors();
    if (ImGui::GetCurrentContext() == nullptr)
        return;
    // Dear ImGui's recoverable error path: a log line and tooltip, and an assert only when the
    // host keeps io.ConfigErrorRecoveryEnableAssert on.
    const char* text = message + std::strlen("facet: error: ");
    bool valid = false;
    IM_ASSERT_USER_ERROR(valid, text);
}

} // namespace facet::internal
