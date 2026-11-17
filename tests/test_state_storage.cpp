// Per-widget state storage: typed states per id, collection of unused ephemeral states and
// ini persistence of persistent ones.

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <doctest/doctest.h>

#include <imgui.h>

#include "internal/state_storage.h"

namespace
{

struct CounterState
{
    int value = 0;
};

int destroyedCounters = 0;

struct TrackedState
{
    int value = 7;
    ~TrackedState() { ++destroyedCounters; }
};

struct SavedState
{
    static constexpr const char* kStateName = "Saved";
    int value = 0;
    void WriteIni(ImGuiTextBuffer& out) const { out.appendf("value=%d\n", value); }
    void ReadIni(const char* key, const char* text)
    {
        if (std::strcmp(key, "value") == 0)
            value = std::atoi(text);
    }
};

} // namespace

TEST_CASE("State storage keeps a typed struct per id and collects unused ones")
{
    facet::internal::StateStore store;
    auto& counterType = facet::internal::StateTypeOf<CounterState>();
    auto& trackedType = facet::internal::StateTypeOf<TrackedState>();

    bool created = false;
    auto* a = static_cast<CounterState*>(store.Get(10, counterType, false, &created));
    CHECK(created);
    a->value = 3;
    auto* again = static_cast<CounterState*>(store.Get(10, counterType, false, &created));
    CHECK_FALSE(created);
    CHECK(again == a);
    CHECK(again->value == 3);

    // The same id with another type is a separate state.
    auto* tracked = static_cast<TrackedState*>(store.Get(10, trackedType, false));
    CHECK(tracked->value == 7);
    CHECK(store.Count() == 2);

    destroyedCounters = 0;
    for (std::uint32_t i = 0; i < facet::internal::StateStore::kGcFrames; ++i)
    {
        store.BeginFrame();
        store.Get(10, counterType, false);
        store.EndFrame();
    }
    CHECK(store.Count() == 1);
    CHECK(destroyedCounters == 1);
    CHECK(store.Find(10, trackedType) == nullptr);
    CHECK(store.Find(10, counterType) != nullptr);
}

TEST_CASE("Persistent states survive collection and round-trip through ini records")
{
    auto& savedType = facet::internal::StateTypeOf<SavedState>();
    ImGuiTextBuffer text;
    {
        facet::internal::StateStore store;
        static_cast<SavedState*>(store.Get(0xABCDu, savedType, true))->value = 42;
        for (std::uint32_t i = 0; i < facet::internal::StateStore::kGcFrames * 2; ++i)
        {
            store.BeginFrame();
            store.EndFrame();
        }
        CHECK(store.Count() == 1);
        store.WriteRecords("FacetState", text);
    }
    CHECK(std::strstr(text.c_str(), "[FacetState][Saved,0x0000ABCD]") != nullptr);
    CHECK(std::strstr(text.c_str(), "value=42") != nullptr);

    // Loading before the state is requested keeps the record pending until then.
    facet::internal::StateStore store;
    REQUIRE(store.BeginRecord("Saved,0x0000ABCD"));
    store.RecordLine("value=42");
    CHECK_FALSE(store.BeginRecord("no comma"));
    REQUIRE(store.BeginRecord("Unknown,0x1"));
    store.RecordLine("x=1");
    CHECK(store.PendingRecords() == 2);
    const auto* restored = static_cast<SavedState*>(store.Get(0xABCDu, savedType, true));
    CHECK(restored->value == 42);
    CHECK(store.PendingRecords() == 1);

    // Records of types this run never requests are written back unchanged.
    ImGuiTextBuffer again;
    store.WriteRecords("FacetState", again);
    CHECK(std::strstr(again.c_str(), "[FacetState][Unknown,0x00000001]\nx=1") != nullptr);

    // Parking turns live persistent states into records that a later save still writes.
    store.ParkAndClear();
    CHECK(store.Count() == 0);
    CHECK(store.PendingRecords() == 2);
    CHECK(static_cast<SavedState*>(store.Get(0xABCDu, savedType, true))->value == 42);
}
