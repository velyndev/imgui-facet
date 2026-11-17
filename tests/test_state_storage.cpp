// Per-widget state storage: typed states per id, collection of unused ephemeral states and
// ini persistence of persistent ones.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

#include <d3d11.h>
#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <wrl/client.h>

#include <facet/facet.h>

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

    // At 30 Hz kGcSeconds pass after 60 frames, so the frame count decides.
    destroyedCounters = 0;
    for (std::uint32_t i = 0; i + 1 < facet::internal::StateStore::kGcFrames; ++i)
    {
        store.BeginFrame(1.0f / 30.0f);
        store.Get(10, counterType, false);
        store.EndFrame();
    }
    CHECK(store.Count() == 2);
    store.BeginFrame(1.0f / 30.0f);
    store.Get(10, counterType, false);
    store.EndFrame();
    CHECK(store.Count() == 1);
    CHECK(destroyedCounters == 1);
    CHECK(store.Find(10, trackedType) == nullptr);
    CHECK(store.Find(10, counterType) != nullptr);
}

TEST_CASE("Ephemeral state lives for the same time at 240 Hz as at 60 Hz")
{
    facet::internal::StateStore store;
    auto& counterType = facet::internal::StateTypeOf<CounterState>();
    static_cast<CounterState*>(store.Get(20, counterType, false))->value = 5;
    const float dt = 1.0f / 240.0f;
    // One second hidden: far more than kGcFrames frames, but less than kGcSeconds.
    for (int i = 0; i < 240; ++i)
    {
        store.BeginFrame(dt);
        store.EndFrame();
    }
    REQUIRE(store.Find(20, counterType) != nullptr);
    CHECK(static_cast<CounterState*>(store.Get(20, counterType, false))->value == 5);
    // Hidden for longer than kGcSeconds, it is collected at this rate too.
    const int frames = static_cast<int>(facet::internal::StateStore::kGcSeconds / dt) + 2;
    for (int i = 0; i < frames; ++i)
    {
        store.BeginFrame(dt);
        store.EndFrame();
    }
    CHECK(store.Find(20, counterType) == nullptr);
}

TEST_CASE("Ephemeral state is collected after kGcFramesAlways frames when the clock stands still")
{
    facet::internal::StateStore store;
    auto& counterType = facet::internal::StateTypeOf<CounterState>();
    store.Get(30, counterType, false);
    for (std::uint32_t i = 0; i + 1 < facet::internal::StateStore::kGcFramesAlways; ++i)
    {
        store.BeginFrame(0.0f);
        store.EndFrame();
    }
    CHECK(store.Count() == 1);
    store.BeginFrame(0.0f);
    store.EndFrame();
    CHECK(store.Count() == 0);
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
            store.BeginFrame(1.0f / 30.0f);
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

TEST_CASE("Ini text feeds only the records of the named handler")
{
    const char text[] = "[Window][Main]\r\nPos=1,2\r\n\r\n[FacetState][Saved,0x00000010]\r\nvalue=11\r\n"
                        "[Other][Saved,0x00000020]\nvalue=99\n[FacetState][Saved,0x00000030]\nvalue=33\n"
                        "[FacetState][broken]\nvalue=1\n";
    facet::internal::StateStore store;
    facet::internal::LoadRecordsFromIni(store, "FacetState", text, sizeof(text) - 1);
    CHECK(store.PendingRecords() == 2);
    auto& savedType = facet::internal::StateTypeOf<SavedState>();
    CHECK(static_cast<SavedState*>(store.Get(0x10u, savedType, true))->value == 11);
    CHECK(static_cast<SavedState*>(store.Get(0x30u, savedType, true))->value == 33);
    CHECK(static_cast<SavedState*>(store.Get(0x20u, savedType, true))->value == 0);
}

TEST_CASE("Init after the first frame still loads saved widget state")
{
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    REQUIRE(SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
                                        device.GetAddressOf(), nullptr, context.GetAddressOf())));
    const std::string path = (std::filesystem::temp_directory_path() / "facet_state_late_init.ini").string();
    {
        std::FILE* file = nullptr;
        REQUIRE(fopen_s(&file, path.c_str(), "wb") == 0);
        std::fputs("[FacetState][Saved,0x0000BEEF]\nvalue=9\n\n", file);
        std::fclose(file);
    }

    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = path.c_str();
    // What the first ImGui::NewFrame does: read the file once, before Facet's handler exists.
    ImGui::LoadIniSettingsFromDisk(path.c_str());
    GImGui->SettingsLoaded = true;

    facet::InitDesc desc;
    desc.device = device.Get();
    desc.context = context.Get();
    REQUIRE(facet::Init(desc));
    auto& saved = facet::internal::GetPersistentState<SavedState>(0xBEEFu);
    CHECK(saved.value == 9);
    facet::Shutdown();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::DestroyContext();
    std::filesystem::remove(path);
}