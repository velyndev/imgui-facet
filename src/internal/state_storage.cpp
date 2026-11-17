// Per-widget state storage with frame-based garbage collection and ini persistence.

#include "internal/state_storage.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <imgui_internal.h>

#include "internal/device.h"
#include "internal/subsystem.h"

namespace facet::internal
{
namespace
{

constexpr const char* kHandlerName = "FacetState";

} // namespace

void StateStore::Deleter::operator()(void* object) const
{
    if (object == nullptr || type == nullptr)
        return;
    type->destroy(object);
    ::operator delete(object, std::align_val_t(type->align));
}

StateStore::~StateStore()
{
    Clear();
}

std::uint64_t StateStore::Key(ImGuiID id, const StateTypeInfo& type)
{
    return (static_cast<std::uint64_t>(type.index) << 32) | id;
}

void StateStore::ApplyLine(const StateTypeInfo& type, void* object, const std::string& line)
{
    if (type.readIni == nullptr)
        return;
    const size_t equals = line.find('=');
    if (equals == std::string::npos)
        return;
    const std::string key = line.substr(0, equals);
    type.readIni(object, key.c_str(), line.c_str() + equals + 1);
}

void* StateStore::Get(ImGuiID id, StateTypeInfo& type, bool persistent, bool* created)
{
    const std::uint64_t key = Key(id, type);
    auto it = entries.find(key);
    if (it != entries.end())
    {
        it->second.lastFrame = frame;
        it->second.lastClock = clock;
        it->second.persistent = it->second.persistent || persistent;
        if (created != nullptr)
            *created = false;
        return it->second.object.get();
    }

    void* memory = ::operator new(type.size, std::align_val_t(type.align));
    type.construct(memory);
    Entry entry;
    entry.object = std::unique_ptr<void, Deleter>(memory, Deleter{&type});
    entry.type = &type;
    entry.id = id;
    entry.lastFrame = frame;
    entry.lastClock = clock;
    entry.persistent = persistent;

    // Restore a record the ini file held for this state.
    if (persistent && type.readIni != nullptr)
    {
        for (size_t i = 0; i < pending.size(); ++i)
        {
            if (pending[i].id != id || pending[i].type != type.name)
                continue;
            for (const std::string& line : pending[i].lines)
                ApplyLine(type, memory, line);
            pending.erase(pending.begin() + static_cast<std::ptrdiff_t>(i));
            readPending = -1;
            break;
        }
    }
    entries.emplace(key, std::move(entry));
    if (created != nullptr)
        *created = true;
    return memory;
}

void* StateStore::Find(ImGuiID id, const StateTypeInfo& type) const
{
    const auto it = entries.find(Key(id, type));
    return it != entries.end() ? it->second.object.get() : nullptr;
}

void StateStore::BeginFrame(float dt)
{
    ++frame;
    clock += dt > 0.0f ? static_cast<double>(dt) : 0.0;
}

void StateStore::EndFrame()
{
    for (auto it = entries.begin(); it != entries.end();)
    {
        const Entry& entry = it->second;
        const std::uint32_t idleFrames = frame - entry.lastFrame;
        const bool idle = idleFrames >= kGcFrames && clock - entry.lastClock >= kGcSeconds;
        if (!entry.persistent && (idle || idleFrames >= kGcFramesAlways))
        {
            if (readEntry == &it->second)
                readEntry = nullptr;
            it = entries.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void StateStore::Clear()
{
    entries.clear();
    readEntry = nullptr;
}

void StateStore::ParkAndClear()
{
    for (const auto& [key, entry] : entries)
    {
        if (!entry.persistent || entry.type->writeIni == nullptr)
            continue;
        ImGuiTextBuffer text;
        entry.type->writeIni(entry.object.get(), text);
        PendingRecord record;
        record.type = entry.type->name;
        record.id = entry.id;
        const char* line = text.c_str();
        while (*line != '\0')
        {
            const char* end = std::strchr(line, '\n');
            const size_t length = end != nullptr ? static_cast<size_t>(end - line) : std::strlen(line);
            if (length > 0)
                record.lines.emplace_back(line, length);
            line += length + (end != nullptr ? 1 : 0);
        }
        pending.push_back(std::move(record));
    }
    Clear();
}

bool StateStore::BeginRecord(const char* name)
{
    readEntry = nullptr;
    readPending = -1;
    const char* comma = std::strrchr(name, ',');
    if (comma == nullptr || comma == name)
        return false;
    const std::string type(name, comma);
    char* end = nullptr;
    const unsigned long id = std::strtoul(comma + 1, &end, 16);
    if (end == comma + 1 || *end != '\0')
        return false;

    // A state that already exists takes the values directly.
    for (auto& [key, entry] : entries)
    {
        if (entry.persistent && entry.id == static_cast<ImGuiID>(id) && type == entry.type->name)
        {
            readEntry = &entry;
            return true;
        }
    }
    for (size_t i = 0; i < pending.size(); ++i)
    {
        if (pending[i].id == static_cast<ImGuiID>(id) && pending[i].type == type)
        {
            pending[i].lines.clear();
            readPending = static_cast<int>(i);
            return true;
        }
    }
    PendingRecord record;
    record.type = type;
    record.id = static_cast<ImGuiID>(id);
    pending.push_back(std::move(record));
    readPending = static_cast<int>(pending.size()) - 1;
    return true;
}

void StateStore::RecordLine(const char* line)
{
    if (line == nullptr || line[0] == '\0')
        return;
    if (readEntry != nullptr)
        ApplyLine(*readEntry->type, readEntry->object.get(), line);
    else if (readPending >= 0 && readPending < static_cast<int>(pending.size()))
        pending[static_cast<size_t>(readPending)].lines.emplace_back(line);
}

void StateStore::WriteRecords(const char* handlerName, ImGuiTextBuffer& out) const
{
    for (const auto& [key, entry] : entries)
    {
        if (!entry.persistent || entry.type->writeIni == nullptr)
            continue;
        out.appendf("[%s][%s,0x%08X]\n", handlerName, entry.type->name, entry.id);
        entry.type->writeIni(entry.object.get(), out);
        out.append("\n");
    }
    for (const PendingRecord& record : pending)
    {
        out.appendf("[%s][%s,0x%08X]\n", handlerName, record.type.c_str(), record.id);
        for (const std::string& line : record.lines)
            out.appendf("%s\n", line.c_str());
        out.append("\n");
    }
}

StateStore& GlobalStateStore()
{
    static StateStore store;
    return store;
}

std::uint32_t NextStateTypeIndex()
{
    static std::atomic<std::uint32_t> next{1};
    return next++;
}

void LoadRecordsFromIni(StateStore& store, const char* handlerName, const char* text, size_t size)
{
    if (text == nullptr || handlerName == nullptr)
        return;
    const char* const end = text + size;
    const size_t handlerLength = std::strlen(handlerName);
    bool inRecord = false;
    std::string line;
    for (const char* cursor = text; cursor < end;)
    {
        const char* lineEnd = cursor;
        while (lineEnd < end && *lineEnd != '\n' && *lineEnd != '\r')
            ++lineEnd;
        line.assign(cursor, lineEnd);
        cursor = lineEnd;
        while (cursor < end && (*cursor == '\n' || *cursor == '\r'))
            ++cursor;
        if (line.empty())
            continue;
        if (line.front() == '[' && line.back() == ']')
        {
            // "[type][name]": a record of this handler starts, any other section ends it.
            inRecord = false;
            const size_t split = line.find("][");
            if (split == std::string::npos)
                continue;
            if (split - 1 == handlerLength && line.compare(1, handlerLength, handlerName) == 0)
            {
                const std::string name = line.substr(split + 2, line.size() - split - 3);
                inRecord = store.BeginRecord(name.c_str());
            }
            continue;
        }
        if (inRecord)
            store.RecordLine(line.c_str());
    }
}

void MarkStateDirty()
{
    if (ImGui::GetCurrentContext() != nullptr)
        ImGui::MarkIniSettingsDirty();
}

namespace
{

void* ReadOpen(ImGuiContext*, ImGuiSettingsHandler*, const char* name)
{
    // Any non-null value means "this record is ours"; the store remembers which one it is.
    static int token = 0;
    return GlobalStateStore().BeginRecord(name) ? &token : nullptr;
}

void ReadLine(ImGuiContext*, ImGuiSettingsHandler*, void*, const char* line)
{
    GlobalStateStore().RecordLine(line);
}

void WriteAll(ImGuiContext*, ImGuiSettingsHandler* handler, ImGuiTextBuffer* out)
{
    GlobalStateStore().WriteRecords(handler->TypeName, *out);
}

bool StateInit()
{
    if (ImGui::GetCurrentContext() != nullptr && ImGui::FindSettingsHandler(kHandlerName) == nullptr)
    {
        ImGuiSettingsHandler handler;
        handler.TypeName = kHandlerName;
        handler.TypeHash = ImHashStr(kHandlerName);
        handler.ReadOpenFn = &ReadOpen;
        handler.ReadLineFn = &ReadLine;
        handler.WriteAllFn = &WriteAll;
        ImGui::AddSettingsHandler(&handler);

        // Dear ImGui reads the ini file once, in the first ImGui::NewFrame, and skips the
        // sections of handlers it does not know yet. When the host calls facet::Init after
        // that, read Facet's own sections now, so the next save does not drop them.
        const ImGuiIO& io = ImGui::GetIO();
        if (GImGui->SettingsLoaded && io.IniFilename != nullptr)
        {
            size_t size = 0;
            char* text = static_cast<char*>(ImFileLoadToMemory(io.IniFilename, "rb", &size));
            if (text != nullptr)
            {
                LoadRecordsFromIni(GlobalStateStore(), kHandlerName, text, size);
                IM_FREE(text);
            }
        }
    }
    return true;
}

// The handler stays registered: Dear ImGui saves the ini file when its context is destroyed,
// after Shutdown, and the parked records must still be written then. Its functions only use
// the global store, which outlives the context.
void StateShutdown()
{
    GlobalStateStore().ParkAndClear();
}

void StateNewFrame()
{
    GlobalStateStore().BeginFrame(DeltaTime());
}

void StateEndFrame()
{
    GlobalStateStore().EndFrame();
}

} // namespace

// Registers the ini handler at Init (once per Dear ImGui context; when Dear ImGui already read
// the ini file, Facet's sections are read from it at once), counts frames and collects
// unused states.
Subsystem StateStorageSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "state_storage";
    subsystem.init = &StateInit;
    subsystem.shutdown = &StateShutdown;
    subsystem.newFrame = &StateNewFrame;
    subsystem.endFrame = &StateEndFrame;
    return subsystem;
}

} // namespace facet::internal
