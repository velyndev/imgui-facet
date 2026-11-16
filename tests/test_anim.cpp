// Motion: easing curves and the motion store (frame-time independence, stability, pruning).

#include <cmath>

#include <doctest/doctest.h>

#include <facet/facet.h>

#include "core/anim_store.h"

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

TEST_CASE("Keys that are not queried are pruned after kPruneFrames frames")
{
    AnimStore store;
    store.Smooth(1, 0, 1.0f, 10.0f);
    store.Smooth(2, 0, 1.0f, 10.0f);
    CHECK(store.LiveSlots() == 2);
    for (std::uint32_t i = 0; i + 1 < AnimStore::kPruneFrames; ++i)
    {
        store.BeginFrame(1.0f / 60.0f);
        store.Smooth(1, 0, 1.0f, 10.0f);
        store.EndFrame();
    }
    CHECK(store.LiveSlots() == 2);
    store.BeginFrame(1.0f / 60.0f);
    store.Smooth(1, 0, 1.0f, 10.0f);
    store.EndFrame();
    CHECK(store.LiveSlots() == 1);
    // A pruned key starts again at its target.
    store.BeginFrame(1.0f / 60.0f);
    CHECK(store.Smooth(2, 0, 5.0f, 10.0f) == 5.0f);
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
