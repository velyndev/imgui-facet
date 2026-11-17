#pragma once

// Per-widget state that lives longer than one frame: a struct of any default-constructible
// type, kept per ImGui id.
//
//     struct DropdownState { float scroll = 0.0f; int hovered = -1; };
//     DropdownState& s = facet::internal::GetState<DropdownState>(id);
//
// Ephemeral states are destroyed (destructor runs) when they were not requested for both
// StateStore::kGcFrames frames and kGcSeconds of frame time (or for kGcFramesAlways frames
// when the frame time stays 0), so state of widgets that disappeared does not pile up, and
// how long a hidden widget keeps its state does not depend on the frame rate.
// Persistent states are never collected and are saved in the Dear ImGui ini file when the
// type opts in with a name and two functions:
//
//     struct AccordionState
//     {
//         static constexpr const char* kStateName = "Accordion";
//         bool open = false;
//         void WriteIni(ImGuiTextBuffer& out) const { out.appendf("open=%d\n", open ? 1 : 0); }
//         void ReadIni(const char* key, const char* value) { if (!strcmp(key, "open")) open = atoi(value) != 0; }
//     };
//     AccordionState& s = facet::internal::GetPersistentState<AccordionState>(id);
//     ... change s ... facet::internal::MarkStateDirty();
//
// Saved records look like "[FacetState][Accordion,0x1234ABCD]" followed by key=value lines.
// Records loaded before their state is first requested wait until it is, and records of
// types this run never requests are written back unchanged. A lookup does not allocate; a
// new state allocates its struct once.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <string>
#include <unordered_map>
#include <vector>

#include <imgui.h>

namespace facet::internal
{

struct StateTypeInfo
{
    const char* name = "";
    std::size_t size = 0;
    std::size_t align = alignof(std::max_align_t);
    void (*construct)(void* memory) = nullptr;
    void (*destroy)(void* object) = nullptr;
    // Persistent types only.
    void (*writeIni)(const void* object, ImGuiTextBuffer& out) = nullptr;
    void (*readIni)(void* object, const char* key, const char* value) = nullptr;
    std::uint32_t index = 0;  // assigned on first use, part of the storage key
};

class StateStore
{
public:
    // Ephemeral states not requested for kGcFrames frames and kGcSeconds of frame time are
    // destroyed in EndFrame (2 s is 120 frames at 60 Hz); kGcFramesAlways frames destroy them
    // whatever the clock says.
    static constexpr std::uint32_t kGcFrames = 120;
    static constexpr double kGcSeconds = 2.0;
    static constexpr std::uint32_t kGcFramesAlways = 2400;

    StateStore() = default;
    ~StateStore();
    StateStore(const StateStore&) = delete;
    StateStore& operator=(const StateStore&) = delete;

    // The state of `type` for `id`, created (default constructed) on the first request.
    void* Get(ImGuiID id, StateTypeInfo& type, bool persistent, bool* created = nullptr);
    // The state when it exists, without creating or touching it.
    void* Find(ImGuiID id, const StateTypeInfo& type) const;

    // Starts a frame `dt` seconds after the previous one (negative values count as 0).
    void BeginFrame(float dt);
    // Destroys ephemeral states that were not requested recently (see kGcFrames).
    void EndFrame();
    // Destroys every state; pending ini records are kept.
    void Clear();
    // Turns every persistent state into a pending ini record (so a later ini save still
    // writes it), then destroys every state.
    void ParkAndClear();

    int Count() const { return static_cast<int>(entries.size()); }
    std::uint32_t Frame() const { return frame; }

    // Ini persistence, used by the settings handler and by tests.
    // Starts reading a record "<type>,<id>"; returns false for a malformed name.
    bool BeginRecord(const char* name);
    void RecordLine(const char* line);
    // Every persistent state plus every pending record, in ini syntax under `handlerName`.
    void WriteRecords(const char* handlerName, ImGuiTextBuffer& out) const;
    int PendingRecords() const { return static_cast<int>(pending.size()); }

private:
    struct Deleter
    {
        const StateTypeInfo* type = nullptr;
        void operator()(void* object) const;
    };

    struct Entry
    {
        std::unique_ptr<void, Deleter> object;
        const StateTypeInfo* type = nullptr;
        ImGuiID id = 0;
        std::uint32_t lastFrame = 0;
        double lastClock = 0.0;  // store clock at the last request
        bool persistent = false;
    };

    struct PendingRecord
    {
        std::string type;
        ImGuiID id = 0;
        std::vector<std::string> lines;  // "key=value"
    };

    static std::uint64_t Key(ImGuiID id, const StateTypeInfo& type);
    static void ApplyLine(const StateTypeInfo& type, void* object, const std::string& line);

    std::unordered_map<std::uint64_t, Entry> entries;
    std::vector<PendingRecord> pending;
    // The record the ini reader is filling: a live state or a pending record.
    Entry* readEntry = nullptr;
    int readPending = -1;
    std::uint32_t frame = 1;
    double clock = 0.0;  // seconds of frame time since the store was created
};

// The store used by GetState and GetPersistentState, driven by the library's frame hooks.
StateStore& GlobalStateStore();

// Marks the ini file as changed so Dear ImGui saves it (after changing a persistent state).
void MarkStateDirty();

std::uint32_t NextStateTypeIndex();

template <class T>
concept PersistentStateType = requires(T state, const T constState, ImGuiTextBuffer& out) {
    { T::kStateName } -> std::convertible_to<const char*>;
    constState.WriteIni(out);
    state.ReadIni("", "");
};

template <class T>
StateTypeInfo& StateTypeOf()
{
    static StateTypeInfo info = [] {
        StateTypeInfo type;
        type.size = sizeof(T);
        type.align = alignof(T);
        type.construct = [](void* memory) { new (memory) T(); };
        type.destroy = [](void* object) { static_cast<T*>(object)->~T(); };
        if constexpr (PersistentStateType<T>)
        {
            type.name = T::kStateName;
            type.writeIni = [](const void* object, ImGuiTextBuffer& out) { static_cast<const T*>(object)->WriteIni(out); };
            type.readIni = [](void* object, const char* key, const char* value) {
                static_cast<T*>(object)->ReadIni(key, value);
            };
        }
        type.index = NextStateTypeIndex();
        return type;
    }();
    return info;
}

// The ephemeral state of type T for `id`. `created` reports a new (default) state.
template <class T>
T& GetState(ImGuiID id, bool* created = nullptr)
{
    return *static_cast<T*>(GlobalStateStore().Get(id, StateTypeOf<T>(), false, created));
}

// The persistent state of type T for `id`, restored from the ini file when a record exists.
template <PersistentStateType T>
T& GetPersistentState(ImGuiID id, bool* created = nullptr)
{
    return *static_cast<T*>(GlobalStateStore().Get(id, StateTypeOf<T>(), true, created));
}

} // namespace facet::internal
