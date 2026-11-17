#pragma once

// The motion store behind facet::Smooth, Spring, Tween and SmoothColor (anim.h).
//
// Slots live in an open-addressing hash table keyed by (ImGui id, sub key). A query finds or
// creates its slot, advances it to the store's clock and returns the value. Motion time passes
// only while a key is queried: a key that skipped frames advances by one frame on its next
// query. The table grows by doubling, so queries allocate only when the number of live keys
// reaches a new maximum, never per query. EndFrame drops slots that were not queried for
// kPruneFrames frames and kPruneSeconds of store time, so how long a hidden key survives does
// not depend on the frame rate; kPruneFramesAlways frames drop a key also when the clock
// stands still (a host that passes a delta time of 0).
//
// The public functions use one global store, driven by the anim subsystem hooks. Tests and
// tools create their own store and drive it with BeginFrame(dt) / EndFrame().

#include <cstdint>
#include <vector>

#include <imgui.h>

#include "facet/core/anim.h"
#include "facet/core/color.h"

namespace facet::internal
{

// Time resolution of the store: the time of one query is the store clock, which advances once
// per BeginFrame.
class AnimStore
{
public:
    // Slots not queried for kPruneFrames frames and kPruneSeconds of store time are removed at
    // EndFrame. Both must pass: the frame count keeps a key across a few long frames, the
    // time keeps it across the same wall time at any refresh rate (0.5 s is 30 frames at
    // 60 Hz). kPruneFramesAlways removes a slot whatever the clock says.
    static constexpr std::uint32_t kPruneFrames = 30;
    static constexpr double kPruneSeconds = 0.5;
    static constexpr std::uint32_t kPruneFramesAlways = 600;

    // A value counts as settled (and snaps to its target) when the distance and, for springs,
    // the velocity fall below these, relative to the size of the last jump of the target.
    static constexpr float kSettleDistance = 1e-4f;
    static constexpr float kSettleVelocity = 1e-3f;

    struct Settings
    {
        float motionScale = 1.0f;  // multiplies durations and spring responses
        bool reduced = false;      // every query resolves to its target
    };

    // Advances the clock by dt seconds (negative values count as 0).
    void BeginFrame(float dt);
    // Prunes slots that were not queried recently (see kPruneFrames).
    void EndFrame();
    void Clear();

    void SetSettings(const Settings& settings) { config = settings; }
    const Settings& GetSettings() const { return config; }

    float Smooth(ImGuiID id, int sub, float target, float speed);
    float Spring(ImGuiID id, int sub, float target, const SpringSpec& spec);
    float Tween(ImGuiID id, int sub, float target, float duration, Ease ease);
    ImVec4 SmoothColor(ImGuiID id, int sub, const ImVec4& target, float speed);

    // The spring velocity of a key, 0 for unknown keys or other kinds.
    float Velocity(ImGuiID id, int sub) const;
    // True when the key exists and has reached its target.
    bool Settled(ImGuiID id, int sub) const;

    int LiveSlots() const { return live; }
    int Capacity() const { return static_cast<int>(slots.size()); }
    double Clock() const { return clock; }
    std::uint32_t Frame() const { return frame; }

private:
    enum class Kind : std::uint8_t
    {
        Empty,
        Tombstone,
        Smooth,
        Spring,
        Tween,
        Color,
    };

    struct Slot
    {
        std::uint64_t key = 0;
        Kind kind = Kind::Empty;
        bool settled = true;
        std::uint32_t lastFrame = 0;
        double lastQueryClock = 0.0;  // store clock at the last query, for pruning
        double time = 0.0;     // store clock at the last update
        float value[4] = {};   // current value (Color: OKLab L, a, b and alpha)
        float target[4] = {};  // target of the running motion
        float velocity = 0.0f; // Spring
        float from = 0.0f;     // Tween: start value
        float elapsed = 0.0f;  // Tween: seconds since the target changed
        float span = 0.0f;     // size of the last target jump, the scale of the settle test
    };

    static std::uint64_t MakeKey(ImGuiID id, int sub);
    // Finds the slot of key, or creates one of `kind` (`created` tells which).
    Slot& Acquire(std::uint64_t key, Kind kind, bool& created);
    const Slot* Find(std::uint64_t key) const;
    void Grow();
    // Rebuilds the table at `capacity` (a power of two) without its tombstones.
    void Rehash(size_t capacity);
    float Elapsed(Slot& slot);

    std::vector<Slot> slots;  // size is zero or a power of two
    std::vector<Slot> spare;  // the previous table buffer, reused by the next Rehash
    int live = 0;
    int used = 0;             // live plus tombstones
    double clock = 0.0;
    float frameDelta = 0.0f;  // dt of the last BeginFrame
    std::uint32_t frame = 1;
    Settings config;
};

// One step of a damped spring of unit mass toward 0, solved in closed form so the result is
// exact for any dt. `x` is the displacement from the target, `v` the velocity.
void SpringStep(float& x, float& v, float omega, float dampingRatio, float dt);

// The store behind the public functions in anim.h.
AnimStore& GlobalAnimStore();

} // namespace facet::internal
