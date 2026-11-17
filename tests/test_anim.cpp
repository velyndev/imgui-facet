// Motion: easing curves, the motion store (frame-time independence, stability, pruning) and
// the per-widget state store, which shares the frame-based collection rule.

#include <cmath>
#include <cstdlib>
#include <cstring>

#include <doctest/doctest.h>

#include <facet/facet.h>

#include "core/anim_store.h"
#include "internal/state_storage.h"

namespace
{

using facet::internal::AnimStore;

constexpr ImGuiID kId = 0x1234u;

// Runs a store at a fixed frame time for `seconds`, querying one motion every frame, and
// returns the value at the end. The target jumps from 0 to 100 after the first frame.
template <class Query>
float Run(float dt, float seconds, Query query)
{
    AnimStore store;
    query(store, 0.0f);  // first query: starts at the target 0
    const int frames = static_cast<int>(std::lround(seconds / dt));
    float value = 0.0f;
    for (int i = 0; i < frames; ++i)
    {
        store.BeginFrame(dt);
        value = query(store, 100.0f);
        store.EndFrame();
    }
    return value;
}

} // namespace

TEST_CASE("Every easing curve maps 0 to 0 and 1 to 1 and clamps its input")
{
    const facet::Ease eases[] = {facet::Ease::Linear,     facet::Ease::InCubic,  facet::Ease::OutCubic,
                                 facet::Ease::InOutCubic, facet::Ease::OutQuint, facet::Ease::OutBack};
    for (const facet::Ease ease : eases)
    {
        CAPTURE(static_cast<int>(ease));
        CHECK(facet::EaseValue(ease, 0.0f) == doctest::Approx(0.0f));
        CHECK(facet::EaseValue(ease, 1.0f) == doctest::Approx(1.0f));
        CHECK(facet::EaseValue(ease, -1.0f) == doctest::Approx(0.0f));
        CHECK(facet::EaseValue(ease, 2.0f) == doctest::Approx(1.0f));
    }
    CHECK(facet::EaseValue(facet::Ease::InOutCubic, 0.5f) == doctest::Approx(0.5f));
    CHECK(facet::EaseValue(facet::Ease::OutBack, 0.8f) > 1.0f);  // overshoots
}

TEST_CASE("The first query of an animation key returns the target")
{
    CHECK(facet::Smooth(1, 0, 3.0f, 10.0f) == doctest::Approx(3.0f));
    CHECK(facet::Spring(2, 0, 4.0f, facet::SpringSpec{}) == doctest::Approx(4.0f));
    CHECK(facet::Tween(3, 0, 5.0f, 0.2f) == doctest::Approx(5.0f));
    const ImVec4 color = facet::SmoothColor(4, 0, ImVec4(0.1f, 0.2f, 0.3f, 1.0f), 10.0f);
    CHECK(color.z == doctest::Approx(0.3f));
}

TEST_CASE("A query does not move the value without frame time")
{
    AnimStore store;
    store.Spring(kId, 0, 0.0f, {});
    CHECK(store.Spring(kId, 0, 50.0f, {}) == doctest::Approx(0.0f));
    CHECK(store.Spring(kId, 0, 50.0f, {}) == doctest::Approx(0.0f));  // same frame: same value
    store.BeginFrame(1.0f / 60.0f);
    CHECK(store.Spring(kId, 0, 50.0f, {}) > 0.0f);
}

TEST_CASE("Smooth, Spring and Tween give the same curve at 1/30 s and 1/240 s frames")
{
    const facet::SpringSpec bouncy{0.35f, 0.6f};
    const facet::SpringSpec critical{0.3f, 1.0f};
    const facet::SpringSpec overdamped{0.4f, 1.6f};
    for (const float seconds : {0.1f, 1.0f / 6.0f, 0.3f, 0.5f})
    {
        CAPTURE(seconds);
        const auto smooth = [](AnimStore& s, float target) { return s.Smooth(kId, 0, target, 12.0f); };
        CHECK(Run(1.0f / 30.0f, seconds, smooth) == doctest::Approx(Run(1.0f / 240.0f, seconds, smooth)).epsilon(1e-3));

        for (const facet::SpringSpec& spec : {bouncy, critical, overdamped})
        {
            CAPTURE(spec.dampingRatio);
            const auto spring = [&](AnimStore& s, float target) { return s.Spring(kId, 0, target, spec); };
            CHECK(Run(1.0f / 30.0f, seconds, spring) ==
                  doctest::Approx(Run(1.0f / 240.0f, seconds, spring)).epsilon(1e-3));
        }

        const auto tween = [](AnimStore& s, float target) {
            return s.Tween(kId, 0, target, 0.6f, facet::Ease::InOutCubic);
        };
        CHECK(Run(1.0f / 30.0f, seconds, tween) == doctest::Approx(Run(1.0f / 240.0f, seconds, tween)).epsilon(1e-3));
    }
}

TEST_CASE("A spring is stable at a 0.1 s frame time and settles exactly on its target")
{
    for (const float damping : {0.2f, 0.6f, 1.0f, 2.0f})
    {
        CAPTURE(damping);
        AnimStore store;
        const facet::SpringSpec spec{0.25f, damping};
        store.Spring(kId, 0, 0.0f, spec);
        float value = 0.0f;
        float peak = 0.0f;
        for (int i = 0; i < 200; ++i)
        {
            store.BeginFrame(0.1f);
            value = store.Spring(kId, 0, 100.0f, spec);
            CHECK(std::isfinite(value));
            peak = std::max(peak, std::fabs(value));
        }
        // Never beyond the overshoot of the continuous spring (100 plus less than 100 %).
        CHECK(peak < 200.0f);
        CHECK(value == 100.0f);
        CHECK(store.Settled(kId, 0));
        CHECK(store.Velocity(kId, 0) == 0.0f);
    }
}

TEST_CASE("A spring keeps its velocity when the target changes mid-flight")
{
    AnimStore store;
    const facet::SpringSpec spec{0.4f, 1.0f};
    store.Spring(kId, 0, 0.0f, spec);
    for (int i = 0; i < 6; ++i)
    {
        store.BeginFrame(1.0f / 60.0f);
        store.Spring(kId, 0, 100.0f, spec);
    }
    const float velocity = store.Velocity(kId, 0);
    CHECK(velocity > 0.0f);
    store.BeginFrame(1.0f / 60.0f);
    const float before = store.Spring(kId, 0, 100.0f, spec);
    store.BeginFrame(1.0f / 60.0f);
    // Retargeting back to 0 does not reverse at once: the value still moves on for a moment.
    CHECK(store.Spring(kId, 0, 0.0f, spec) > before);
}

TEST_CASE("Tween moves from where the value is to the new target over the duration")
{
    AnimStore store;
    store.Tween(kId, 0, 0.0f, 0.2f, facet::Ease::Linear);
    store.BeginFrame(0.05f);
    CHECK(store.Tween(kId, 0, 10.0f, 0.2f, facet::Ease::Linear) == doctest::Approx(2.5f));
    store.BeginFrame(0.05f);
    CHECK(store.Tween(kId, 0, 10.0f, 0.2f, facet::Ease::Linear) == doctest::Approx(5.0f));
    // Retargeting mid-way starts from the current value.
    store.BeginFrame(0.05f);
    CHECK(store.Tween(kId, 0, 0.0f, 0.2f, facet::Ease::Linear) == doctest::Approx(3.75f));
    store.BeginFrame(0.2f);
    CHECK(store.Tween(kId, 0, 10.0f, 0.2f, facet::Ease::Linear) == 10.0f);
}

TEST_CASE("SmoothColor converges in OKLab and ends exactly on the target")
{
    AnimStore store;
    const ImVec4 black(0.0f, 0.0f, 0.0f, 1.0f);
    const ImVec4 white(1.0f, 1.0f, 1.0f, 1.0f);
    store.SmoothColor(kId, 0, black, 10.0f);
    store.BeginFrame(0.05f);
    const ImVec4 mid = store.SmoothColor(kId, 0, white, 10.0f);
    CHECK(mid.x > 0.0f);
    CHECK(mid.x < 1.0f);
    for (int i = 0; i < 300; ++i)
    {
        store.BeginFrame(1.0f / 60.0f);
        store.SmoothColor(kId, 0, white, 10.0f);
    }
    const ImVec4 end = store.SmoothColor(kId, 0, white, 10.0f);
    CHECK(end.x == 1.0f);
    CHECK(end.w == 1.0f);
}

TEST_CASE("Reduced motion and a motion scale of 0 resolve at once")
{
    for (const bool reduced : {true, false})
    {
        AnimStore store;
        AnimStore::Settings settings;
        settings.reduced = reduced;
        settings.motionScale = reduced ? 1.0f : 0.0f;
        store.SetSettings(settings);
        store.Spring(kId, 0, 0.0f, {});
        store.Tween(kId, 1, 0.0f, 0.3f, facet::Ease::OutCubic);
        store.Smooth(kId, 2, 0.0f, 8.0f);
        store.BeginFrame(1.0f / 60.0f);
        CHECK(store.Spring(kId, 0, 7.0f, {}) == 7.0f);
        CHECK(store.Tween(kId, 1, 7.0f, 0.3f, facet::Ease::OutCubic) == 7.0f);
        CHECK(store.Smooth(kId, 2, 7.0f, 8.0f) == 7.0f);
    }
}

TEST_CASE("A motion scale of 2 makes a tween take twice as long")
{
    AnimStore store;
    AnimStore::Settings settings;
    settings.motionScale = 2.0f;
    store.SetSettings(settings);
    store.Tween(kId, 0, 0.0f, 0.2f, facet::Ease::Linear);
    store.BeginFrame(0.0f);
    store.Tween(kId, 0, 10.0f, 0.2f, facet::Ease::Linear);
    store.BeginFrame(0.2f);
    CHECK(store.Tween(kId, 0, 10.0f, 0.2f, facet::Ease::Linear) == doctest::Approx(5.0f));
}

TEST_CASE("Keys that are not queried are pruned after kPruneFrames frames once kPruneSeconds passed")
{
    // At 30 Hz the time limit passes after 15 frames, so the frame count decides.
    AnimStore store;
    store.Smooth(1, 0, 1.0f, 10.0f);
    store.Smooth(2, 0, 1.0f, 10.0f);
    CHECK(store.LiveSlots() == 2);
    for (std::uint32_t i = 0; i + 1 < AnimStore::kPruneFrames; ++i)
    {
        store.BeginFrame(1.0f / 30.0f);
        store.Smooth(1, 0, 1.0f, 10.0f);
        store.EndFrame();
    }
    CHECK(store.LiveSlots() == 2);
    store.BeginFrame(1.0f / 30.0f);
    store.Smooth(1, 0, 1.0f, 10.0f);
    store.EndFrame();
    CHECK(store.LiveSlots() == 1);
    // A pruned key starts again at its target.
    store.BeginFrame(1.0f / 30.0f);
    CHECK(store.Smooth(2, 0, 5.0f, 10.0f) == 5.0f);
}

TEST_CASE("A key hidden for 0.2 s survives at 240 Hz and animates when it comes back")
{
    AnimStore store;
    store.Tween(kId, 0, 0.0f, 0.2f, facet::Ease::Linear);
    const float dt = 1.0f / 240.0f;
    // 48 hidden frames: more than kPruneFrames, but less than kPruneSeconds.
    for (int i = 0; i < 48; ++i)
    {
        store.BeginFrame(dt);
        store.EndFrame();
    }
    CHECK(store.LiveSlots() == 1);
    store.BeginFrame(dt);
    CHECK(store.Tween(kId, 0, 1.0f, 0.2f, facet::Ease::Linear) == doctest::Approx(dt / 0.2f));
    store.EndFrame();

    // Hidden for longer than kPruneSeconds, it is dropped at this rate too.
    const int frames = static_cast<int>(AnimStore::kPruneSeconds / dt) + 2;
    for (int i = 0; i < frames; ++i)
    {
        store.BeginFrame(dt);
        store.EndFrame();
    }
    CHECK(store.LiveSlots() == 0);
}

TEST_CASE("Keys are pruned after kPruneFramesAlways frames when the clock stands still")
{
    AnimStore store;
    store.Smooth(kId, 0, 1.0f, 10.0f);
    for (std::uint32_t i = 0; i + 1 < AnimStore::kPruneFramesAlways; ++i)
    {
        store.BeginFrame(0.0f);
        store.EndFrame();
    }
    CHECK(store.LiveSlots() == 1);
    store.BeginFrame(0.0f);
    store.EndFrame();
    CHECK(store.LiveSlots() == 0);
}

TEST_CASE("Frames a key is not queried in do not count as motion time")
{
    AnimStore store;
    const facet::SpringSpec spec{0.4f, 1.0f};
    store.Tween(kId, 0, 0.0f, 0.2f, facet::Ease::Linear);
    store.Spring(kId, 1, 0.0f, spec);
    store.Smooth(kId, 2, 0.0f, 12.0f);
    // The widget is hidden for 20 frames (fewer than kPruneFrames and kPruneSeconds, so the
    // keys survive).
    for (int i = 0; i < 20; ++i)
    {
        store.BeginFrame(1.0f / 60.0f);
        store.EndFrame();
    }
    // It comes back with a new target: the motions start, they do not jump to the end.
    store.BeginFrame(1.0f / 60.0f);
    const float tween = store.Tween(kId, 0, 1.0f, 0.2f, facet::Ease::Linear);
    CHECK(tween == doctest::Approx(1.0f / 60.0f / 0.2f));
    CHECK(store.Spring(kId, 1, 1.0f, spec) < 0.1f);
    CHECK(store.Smooth(kId, 2, 1.0f, 12.0f) < 0.3f);
    store.EndFrame();
    store.BeginFrame(1.0f / 60.0f);
    CHECK(store.Tween(kId, 0, 1.0f, 0.2f, facet::Ease::Linear) == doctest::Approx(2.0f / 60.0f / 0.2f));
}

TEST_CASE("Pruning rebuilds keep the remaining keys")
{
    AnimStore store;
    for (int i = 0; i < 40; ++i)
        store.Smooth(static_cast<ImGuiID>(i + 1), 0, 1.0f, 10.0f);
    const int capacity = store.Capacity();
    // Only the first ten keys stay in use; the other thirty are pruned, which triggers a
    // rebuild of the table.
    for (std::uint32_t frame = 0; frame <= AnimStore::kPruneFrames; ++frame)
    {
        store.BeginFrame(1.0f / 60.0f);
        for (int i = 0; i < 10; ++i)
            store.Smooth(static_cast<ImGuiID>(i + 1), 0, 2.0f, 10.0f);
        store.EndFrame();
    }
    CHECK(store.LiveSlots() == 10);
    CHECK(store.Capacity() == capacity);
    // The kept keys still hold their motion: a new target is approached, not jumped to, which
    // is what a key that was lost and created again would do.
    store.BeginFrame(1.0f / 60.0f);
    for (int i = 0; i < 10; ++i)
        CHECK(store.Smooth(static_cast<ImGuiID>(i + 1), 0, 50.0f, 10.0f) < 50.0f);
}

TEST_CASE("Queries do not grow the store once its keys exist")
{
    AnimStore store;
    for (int i = 0; i < 500; ++i)
        store.Smooth(static_cast<ImGuiID>(i), i % 3, 1.0f, 10.0f);
    const int capacity = store.Capacity();
    CHECK(store.LiveSlots() == 500);
    for (int frame = 0; frame < 200; ++frame)
    {
        store.BeginFrame(1.0f / 60.0f);
        for (int i = 0; i < 500; ++i)
            store.Smooth(static_cast<ImGuiID>(i), i % 3, static_cast<float>(frame), 10.0f);
        store.EndFrame();
    }
    CHECK(store.Capacity() == capacity);
    CHECK(store.LiveSlots() == 500);
}

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
