// The folder reader of FileBrowser (file_browser_parts.h): one worker thread, one queue of jobs and
// one result slot per browser.

#include "widgets/pickers/file_browser_parts.h"

#include <algorithm>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <filesystem>
#include <mutex>
#include <system_error>
#include <thread>
#include <unordered_map>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace facet::internal
{
namespace
{

namespace fs = std::filesystem;

struct FileBrowserJobItem
{
    ImGuiID owner = 0;
    FileBrowserRequestDesc request;
};

struct FileBrowserReader
{
    std::mutex mutex;
    std::condition_variable wake;
    std::thread thread;
    bool stop = false;
    std::deque<FileBrowserJobItem> jobs;
    ImGuiID running = 0;  // the owner whose job the worker is in
    std::unordered_map<ImGuiID, FileBrowserResult> results;
};

FileBrowserReader& Reader()
{
    static FileBrowserReader reader;
    return reader;
}

fs::path FileBrowserPathOf(const std::string& utf8)
{
    return fs::path(std::u8string(reinterpret_cast<const char8_t*>(utf8.data()), utf8.size()));
}

std::string FileBrowserUtf8Of(const fs::path& path)
{
    const std::u8string text = path.u8string();
    return std::string(reinterpret_cast<const char*>(text.data()), text.size());
}

char FileBrowserLower(char c)
{
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

bool FileBrowserLess(const FileBrowserEntry& a, const FileBrowserEntry& b)
{
    if (a.folder != b.folder)
        return a.folder;
    const std::size_t n = std::min(a.name.size(), b.name.size());
    for (std::size_t i = 0; i < n; ++i)
    {
        const char x = FileBrowserLower(a.name[i]);
        const char y = FileBrowserLower(b.name[i]);
        if (x != y)
            return static_cast<unsigned char>(x) < static_cast<unsigned char>(y);
    }
    return a.name.size() < b.name.size();
}

long long FileBrowserStamp(const fs::path& folder, std::error_code& error)
{
    const auto time = fs::last_write_time(folder, error);
    return error ? 0 : static_cast<long long>(time.time_since_epoch().count());
}

void FileBrowserFill(FileBrowserResult& result, const fs::path& folder)
{
    std::error_code error;
    const fs::path absolute = fs::absolute(folder, error);
    const fs::path where = error ? folder : absolute.lexically_normal();
    result.folder = FileBrowserUtf8Of(where);
    // A trailing separator (C:\ stays) is dropped so the parent is found.
    fs::path trimmed = where;
    if (!trimmed.has_filename() && trimmed.has_relative_path())
        trimmed = trimmed.parent_path();
    result.folder = FileBrowserUtf8Of(trimmed);
    const fs::path parent = trimmed.parent_path();
    result.parent = parent != trimmed ? FileBrowserUtf8Of(parent) : std::string();
    if (result.kind == FileBrowserJob::Check)
    {
        result.ok = fs::is_directory(trimmed, error) && !error;
        result.stamp = result.ok ? FileBrowserStamp(trimmed, error) : 0;
        return;
    }
    // No skip_permission_denied: a folder the user may not list must fail, not look empty.
    fs::directory_iterator it(trimmed, error);
    if (error)
    {
        result.ok = false;
        return;
    }
    for (const fs::directory_iterator end; it != end; it.increment(error))
    {
        if (error)
        {
            result.ok = false;
            result.entries.clear();
            return;
        }
        FileBrowserEntry entry;
        entry.name = FileBrowserUtf8Of(it->path().filename());
        std::error_code kindError;
        entry.folder = it->is_directory(kindError);
        const DWORD attributes = GetFileAttributesW(it->path().c_str());
        entry.hidden = (!entry.name.empty() && entry.name[0] == '.') ||
                       (attributes != INVALID_FILE_ATTRIBUTES &&
                        (attributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) != 0);
        result.entries.push_back(std::move(entry));
    }
    std::sort(result.entries.begin(), result.entries.end(), &FileBrowserLess);
    result.ok = true;
    result.stamp = FileBrowserStamp(trimmed, error);
}

FileBrowserResult FileBrowserRun(const FileBrowserJobItem& job)
{
    FileBrowserResult result;
    result.kind = job.request.kind;
    result.generation = job.request.generation;
    try
    {
        fs::path folder = FileBrowserPathOf(job.request.folder);
        if (job.request.kind == FileBrowserJob::Start)
        {
            // The folder the text names, the folder of the file it names, or the current directory.
            std::error_code error;
            if (job.request.folder.empty() || !fs::is_directory(folder, error))
            {
                const fs::path parent = folder.parent_path();
                if (!job.request.folder.empty() && !parent.empty() && fs::is_directory(parent, error))
                    folder = parent;
                else
                    folder = fs::current_path(error);
            }
        }
        FileBrowserFill(result, folder);
    }
    catch (...)
    {
        // Out of memory or a conversion the platform refused: the folder counts as unreadable.
        result.ok = false;
        result.entries.clear();
    }
    return result;
}

void FileBrowserWorker()
{
    FileBrowserReader& reader = Reader();
    std::unique_lock lock(reader.mutex);
    while (true)
    {
        reader.wake.wait(lock, [&] { return reader.stop || !reader.jobs.empty(); });
        if (reader.stop)
            return;
        FileBrowserJobItem job = std::move(reader.jobs.front());
        reader.jobs.pop_front();
        reader.running = job.owner;
        lock.unlock();
        FileBrowserResult result = FileBrowserRun(job);
        lock.lock();
        reader.running = 0;
        try
        {
            reader.results[job.owner] = std::move(result);
        }
        catch (...)
        {
            // No room for the result: the browser asks again later.
        }
    }
}

} // namespace

void FileBrowserRequest(ImGuiID owner, const FileBrowserRequestDesc& request)
{
    FileBrowserReader& reader = Reader();
    try
    {
        std::lock_guard lock(reader.mutex);
        for (FileBrowserJobItem& job : reader.jobs)
            if (job.owner == owner)
            {
                job.request = request;
                return;
            }
        reader.jobs.push_back({owner, request});
        if (!reader.thread.joinable())
        {
            reader.stop = false;
            reader.thread = std::thread(&FileBrowserWorker);
        }
    }
    catch (...)
    {
        return;  // no memory or no thread: the browser stays in its loading state and asks again
    }
    reader.wake.notify_one();
}

bool FileBrowserTake(ImGuiID owner, FileBrowserResult* out)
{
    FileBrowserReader& reader = Reader();
    std::lock_guard lock(reader.mutex);
    const auto found = reader.results.find(owner);
    if (found == reader.results.end())
        return false;
    if (out != nullptr)
        *out = std::move(found->second);
    reader.results.erase(found);
    return true;
}

bool FileBrowserBusy(ImGuiID owner)
{
    FileBrowserReader& reader = Reader();
    std::lock_guard lock(reader.mutex);
    if (reader.running == owner)
        return true;
    for (const FileBrowserJobItem& job : reader.jobs)
        if (job.owner == owner)
            return true;
    return false;
}

void FileBrowserReaderShutdown()
{
    FileBrowserReader& reader = Reader();
    {
        std::lock_guard lock(reader.mutex);
        reader.stop = true;
        reader.jobs.clear();
    }
    reader.wake.notify_one();
    if (reader.thread.joinable())
        reader.thread.join();
    std::lock_guard lock(reader.mutex);
    reader.results.clear();
    reader.running = 0;
    reader.stop = false;
}

bool FileBrowserMatches(const char* name, const char* patterns)
{
    if (patterns == nullptr || patterns[0] == '\0')
        return true;
    // One pattern at a time, separated by ';'.
    const char* pattern = patterns;
    while (*pattern != '\0')
    {
        const char* end = pattern;
        while (*end != '\0' && *end != ';')
            ++end;
        // Wildcard match with backtracking over the last '*'.
        const char* p = pattern;
        const char* s = name;
        const char* star = nullptr;
        const char* retry = nullptr;
        bool matched = true;
        while (*s != '\0')
        {
            if (p < end && (*p == '?' || FileBrowserLower(*p) == FileBrowserLower(*s)))
            {
                ++p;
                ++s;
            }
            else if (p < end && *p == '*')
            {
                star = p++;
                retry = s;
            }
            else if (star != nullptr)
            {
                p = star + 1;
                s = ++retry;
            }
            else
            {
                matched = false;
                break;
            }
        }
        while (matched && p < end && *p == '*')
            ++p;
        if (matched && p == end && end != pattern)
            return true;
        pattern = *end == ';' ? end + 1 : end;
        while (*pattern == ' ')
            ++pattern;
    }
    return false;
}

} // namespace facet::internal
