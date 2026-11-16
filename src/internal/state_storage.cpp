// Per-widget state storage with frame-based garbage collection and ini persistence.

#include "internal/state_storage.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <imgui_internal.h>

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

void StateStore::EndFrame()
{
    for (auto it = entries.begin(); it != entries.end();)
    {
        const Entry& entry = it->second;
        if (!entry.persistent && frame - entry.lastFrame >= kGcFrames)
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
    GlobalStateStore().BeginFrame();
}

void StateEndFrame()
{
    GlobalStateStore().EndFrame();
}

} // namespace

// Registers the ini handler at Init (once per Dear ImGui context, before the first ImGui::NewFrame loads the ini file, when
// the host keeps the usual order), counts frames and collects unused states.
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
