#pragma once

// The folder reader of FileBrowser (pickers.h): every file system call of the browser runs on one
// worker thread, so a slow or unreachable folder never blocks a frame. A browser posts a request
// under its id and a generation, and takes the result in a later frame; a result of an older
// generation is dropped. The worker starts with the first request and stops in the Shutdown hook of
// the date pickers (date_pickers_frame.cpp).
//
//     FileBrowserRequest(id, {.kind = FileBrowserJob::List, .generation = ++state.generation, .folder = folder});
//     FileBrowserResult result;
//     if (FileBrowserTake(id, &result) && result.generation == state.generation) ... apply it ...
//
// Paths are UTF-8 text; the worker converts them for std::filesystem.

#include <cstdint>
#include <string>
#include <vector>

#include <imgui.h>

namespace facet::internal
{

struct FileBrowserEntry
{
    std::string name;     // UTF-8, the last part of the path
    bool folder = false;
    bool hidden = false;  // a hidden or system entry, or a name starting with a dot
};

enum class FileBrowserJob
{
    Start,  // the folder to start in: the folder `folder` names, else the folder of the file it names,
            // else the current directory; then List it
    List,   // the entries of `folder`, folders first, then by name without case
    Check,  // whether `folder` still exists and when it last changed
};

struct FileBrowserRequestDesc
{
    FileBrowserJob kind = FileBrowserJob::List;
    unsigned generation = 0;
    std::string folder;
};

struct FileBrowserResult
{
    FileBrowserJob kind = FileBrowserJob::List;
    unsigned generation = 0;
    bool ok = false;                       // the folder was read (List, Start) or exists (Check)
    std::string folder;                    // the absolute folder, UTF-8
    std::string parent;                    // its parent folder; empty at a root
    std::vector<FileBrowserEntry> entries; // List and Start
    long long stamp = 0;                   // the folder's last write time, as a count
};

// Queues a job for browser `owner`; a queued job of the same owner that has not started is replaced.
void FileBrowserRequest(ImGuiID owner, const FileBrowserRequestDesc& request);
// Moves out the latest result for `owner`. False when there is none.
bool FileBrowserTake(ImGuiID owner, FileBrowserResult* out);
// True while a job of `owner` is queued or running.
bool FileBrowserBusy(ImGuiID owner);
// Stops the worker (it finishes the job it is in) and forgets every job and result.
void FileBrowserReaderShutdown();

// Whether `name` matches one of the wildcard patterns in `patterns` ("*.png;*.jpg"; * and ? with
// ASCII case folded); null or "" matches everything.
bool FileBrowserMatches(const char* name, const char* patterns);

// What a browser shows, for tests and selftests: its folder, the number of entries listed, the
// data state (0 Ready, 1 Loading, 2 Error) and the entry name of a listed row.
struct FileBrowserView
{
    std::string folder;
    int rows = 0;
    int state = 1;
};
bool FileBrowserViewOf(ImGuiID browserId, FileBrowserView* out);
const char* FileBrowserRowName(ImGuiID browserId, int row);

} // namespace facet::internal
