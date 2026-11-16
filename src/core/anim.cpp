// Motion store: smoothing, springs and tweens keyed by ImGui id, plus the reduced motion
// setting of Windows.

#include "facet/core/anim.h"

#include <windows.h>

#include <algorithm>
#include <cmath>

#include "core/anim_store.h"
#include "core/core_internal.h"
#include "facet/core/theme.h"
#include "internal/device.h"
#include "internal/subsystem.h"

namespace facet
{
namespace internal
{
namespace
{

constexpr float kTwoPi = 6.28318530718f;
// Longest time step a slot advances in one go, for a single very long frame (a debugger
// break, a stalled host). Every motion here has long settled by then.
constexpr float kMaxStep = 10.0f;

std::uint64_t Mix64(std::uint64_t x)
{
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return x;
}

} // namespace

void SpringStep(float& x, float& v, float omega, float zeta, float dt)
{
    if (dt <= 0.0f)
        return;
    if (omega <= 0.0f)
    {
        x = 0.0f;
        v = 0.0f;
        return;
    }
    zeta = std::max(0.0f, zeta);
    const double w = omega;
    const double t = dt;
    const double x0 = x;
    const double v0 = v;
    double xt = 0.0;
    double vt = 0.0;
    if (zeta < 1.0f - 1e-4f)
    {
        // Underdamped: decaying oscillation.
        const double z = zeta;
        const double wd = w * std::sqrt(1.0 - z * z);
        const double decay = std::exp(-z * w * t);
        const double c = std::cos(wd * t);
        const double s = std::sin(wd * t);
        const double b = (v0 + z * w * x0) / wd;
        xt = decay * (x0 * c + b * s);
        vt = decay * ((-z * w) * (x0 * c + b * s) + (-x0 * wd * s + b * wd * c));
    }
    else if (zeta > 1.0f + 1e-4f)
    {
        // Overdamped: sum of two decaying exponentials.
        const double z = zeta;
        const double root = w * std::sqrt(z * z - 1.0);
        const double r1 = -z * w + root;
        const double r2 = -z * w - root;
        const double c2 = (v0 - r1 * x0) / (r2 - r1);
        const double c1 = x0 - c2;
        const double e1 = std::exp(r1 * t);
        const double e2 = std::exp(r2 * t);
        xt = c1 * e1 + c2 * e2;
        vt = c1 * r1 * e1 + c2 * r2 * e2;
    }
    else
    {
        // Critically damped.
        const double decay = std::exp(-w * t);
        const double b = v0 + w * x0;
        xt = (x0 + b * t) * decay;
        vt = (b - w * (x0 + b * t)) * decay;
    }
    x = static_cast<float>(xt);
    v = static_cast<float>(vt);
}

std::uint64_t AnimStore::MakeKey(ImGuiID id, int sub)
{
    return (static_cast<std::uint64_t>(id) << 32) | static_cast<std::uint32_t>(sub);
}

void AnimStore::BeginFrame(float dt)
{
    frameDelta = std::max(0.0f, dt);
    clock += frameDelta;
    ++frame;
}

void AnimStore::EndFrame()
{
    if (live == 0)
        return;
    bool removed = false;
    for (Slot& slot : slots)
    {
        if (slot.kind == Kind::Empty || slot.kind == Kind::Tombstone)
            continue;
        if (frame - slot.lastFrame >= kPruneFrames)
        {
            slot.kind = Kind::Tombstone;
            --live;
            removed = true;
        }
    }
    // Rebuild when tombstones pile up, so lookups stay short.
    if (removed && used - live > static_cast<int>(slots.size()) / 4)
        Rehash(slots.size());
}

void AnimStore::Clear()
{
    slots.clear();
    spare.clear();
    live = 0;
    used = 0;
}

void AnimStore::Grow()
{
    Rehash(slots.empty() ? 64 : slots.size() * 2);
}

void AnimStore::Rehash(size_t capacity)
{
    // The live slots move into `spare`, which then becomes the table. The two buffers trade
    // places, so a rebuild at the same capacity reuses the previous buffer and allocates
    // nothing once both exist.
    spare.assign(capacity, Slot{});
    live = 0;
    used = 0;
    const size_t mask = capacity - 1;
    for (const Slot& slot : slots)
    {
        if (slot.kind == Kind::Empty || slot.kind == Kind::Tombstone)
            continue;
        size_t index = static_cast<size_t>(Mix64(slot.key)) & mask;
        while (spare[index].kind != Kind::Empty)
            index = (index + 1) & mask;
        spare[index] = slot;
        ++live;
        ++used;
    }
    slots.swap(spare);
}

const AnimStore::Slot* AnimStore::Find(std::uint64_t key) const
{
    if (slots.empty())
        return nullptr;
    const size_t mask = slots.size() - 1;
    size_t index = static_cast<size_t>(Mix64(key)) & mask;
    for (size_t probe = 0; probe < slots.size(); ++probe)
    {
        const Slot& slot = slots[index];
        if (slot.kind == Kind::Empty)
            return nullptr;
        if (slot.kind != Kind::Tombstone && slot.key == key)
            return &slot;
        index = (index + 1) & mask;
    }
    return nullptr;
}

AnimStore::Slot& AnimStore::Acquire(std::uint64_t key, Kind kind, bool& created)
{
    // Keep the load (live slots plus tombstones) at or below 70 %.
    if (slots.empty() || (used + 1) * 10 > static_cast<int>(slots.size()) * 7)
        Grow();
    const size_t mask = slots.size() - 1;
    size_t index = static_cast<size_t>(Mix64(key)) & mask;
    Slot* reusable = nullptr;
    for (;;)
    {
        Slot& slot = slots[index];
        if (slot.kind == Kind::Empty)
            break;
        if (slot.kind == Kind::Tombstone)
        {
            if (reusable == nullptr)
                reusable = &slot;
        }
        else if (slot.key == key)
        {
            // A key queried as another kind starts over as the new kind.
            created = slot.kind != kind;
            if (created)
            {
                slot = Slot{};
                slot.key = key;
                slot.kind = kind;
                slot.time = clock;
            }
            else if (slot.lastFrame + 1 < frame)
            {
                // The slot missed frames (its widget was not drawn). Motion time passes only
                // while a slot is queried, so the gap counts as one frame: a widget that comes
                // back with a new target starts its motion from the beginning.
                slot.time = std::max(slot.time, clock - static_cast<double>(frameDelta));
            }
            slot.lastFrame = frame;
            return slot;
        }
        index = (index + 1) & mask;
    }
    Slot* target = reusable;
    if (target == nullptr)
    {
        target = &slots[index];
        ++used;
    }
    *target = Slot{};
    target->key = key;
    target->kind = kind;
    target->lastFrame = frame;
    target->time = clock;
    ++live;
    created = true;
    return *target;
}

float AnimStore::Elapsed(Slot& slot)
{
    const float dt = static_cast<float>(std::min(clock - slot.time, static_cast<double>(kMaxStep)));
    slot.time = clock;
    return std::max(0.0f, dt);
}

namespace
{

// Settles a scalar when it is close enough to its target, relative to the motion's span.
bool CloseEnough(float distance, float span, float tolerance)
{
    const float scale = std::max(1.0f, std::fabs(span));
    return std::fabs(distance) <= tolerance * scale;
}

} // namespace

float AnimStore::Smooth(ImGuiID id, int sub, float target, float speed)
{
    bool created = false;
    Slot& slot = Acquire(MakeKey(id, sub), Kind::Smooth, created);
    const float dt = Elapsed(slot);
    if (created || config.reduced || config.motionScale <= 0.0f || speed <= 0.0f)
    {
        slot.value[0] = target;
        slot.target[0] = target;
        slot.settled = true;
        return target;
    }
    if (target != slot.target[0])
    {
        slot.span = target - slot.value[0];
        slot.target[0] = target;
        slot.settled = false;
    }
    if (!slot.settled)
    {
        const float k = std::exp(-speed / config.motionScale * dt);
        slot.value[0] = target + (slot.value[0] - target) * k;
        if (CloseEnough(slot.value[0] - target, slot.span, kSettleDistance))
        {
            slot.value[0] = target;
            slot.settled = true;
        }
    }
    return slot.value[0];
}

float AnimStore::Spring(ImGuiID id, int sub, float target, const SpringSpec& spec)
{
    bool created = false;
    Slot& slot = Acquire(MakeKey(id, sub), Kind::Spring, created);
    const float dt = Elapsed(slot);
    const float response = spec.response * config.motionScale;
    if (created || config.reduced || response <= 0.0f)
    {
        slot.value[0] = target;
        slot.target[0] = target;
        slot.velocity = 0.0f;
        slot.settled = true;
        return target;
    }
    if (target != slot.target[0])
    {
        // The velocity carries over, so a retarget mid-flight bends the path smoothly.
        slot.span = target - slot.value[0];
        slot.target[0] = target;
        slot.settled = false;
    }
    if (!slot.settled)
    {
        float x = slot.value[0] - target;
        float v = slot.velocity;
        SpringStep(x, v, kTwoPi / response, spec.dampingRatio, dt);
        slot.value[0] = target + x;
        slot.velocity = v;
        if (CloseEnough(x, slot.span, kSettleDistance) && CloseEnough(v, slot.span, kSettleVelocity))
        {
            slot.value[0] = target;
            slot.velocity = 0.0f;
            slot.settled = true;
        }
    }
    return slot.value[0];
}

float AnimStore::Tween(ImGuiID id, int sub, float target, float duration, Ease ease)
{
    bool created = false;
    Slot& slot = Acquire(MakeKey(id, sub), Kind::Tween, created);
    const float dt = Elapsed(slot);
    const float scaled = duration * config.motionScale;
    if (created || config.reduced || scaled <= 0.0f)
    {
        slot.value[0] = target;
        slot.target[0] = target;
        slot.from = target;
        slot.settled = true;
        return target;
    }
    if (target != slot.target[0])
    {
        // Starts from where the value is now, also when the previous tween was mid-way.
        // The frame time that passed counts, as for Smooth and Spring, so the start does not
        // depend on the frame rate.
        slot.from = slot.value[0];
        slot.target[0] = target;
        slot.elapsed = 0.0f;
        slot.settled = false;
    }
    if (!slot.settled)
    {
        slot.elapsed += dt;
        const float progress = slot.elapsed / scaled;
        if (progress >= 1.0f)
        {
            slot.value[0] = target;
            slot.settled = true;
        }
        else
        {
            slot.value[0] = slot.from + (target - slot.from) * EaseValue(ease, progress);
        }
    }
    return slot.value[0];
}

ImVec4 AnimStore::SmoothColor(ImGuiID id, int sub, const ImVec4& target, float speed)
{
    bool created = false;
    Slot& slot = Acquire(MakeKey(id, sub), Kind::Color, created);
    const float dt = Elapsed(slot);
    const Oklab lab = ToOklab(target);
    const float goal[4] = {lab.L, lab.a, lab.b, target.w};
    const bool changed = goal[0] != slot.target[0] || goal[1] != slot.target[1] || goal[2] != slot.target[2] ||
                         goal[3] != slot.target[3];
    if (created || config.reduced || config.motionScale <= 0.0f || speed <= 0.0f)
    {
        for (int i = 0; i < 4; ++i)
            slot.value[i] = slot.target[i] = goal[i];
        slot.settled = true;
        return target;
    }
    if (changed)
    {
        for (int i = 0; i < 4; ++i)
            slot.target[i] = goal[i];
        slot.settled = false;
    }
    if (slot.settled)
        return target;
    const float k = std::exp(-speed / config.motionScale * dt);
    bool close = true;
    for (int i = 0; i < 4; ++i)
    {
        slot.value[i] = goal[i] + (slot.value[i] - goal[i]) * k;
        close = close && std::fabs(slot.value[i] - goal[i]) <= kSettleDistance;
    }
    if (close)
    {
        slot.settled = true;
        return target;
    }
    return FromOklab({slot.value[0], slot.value[1], slot.value[2]}, std::clamp(slot.value[3], 0.0f, 1.0f));
}

float AnimStore::Velocity(ImGuiID id, int sub) const
{
    const Slot* slot = Find(MakeKey(id, sub));
    return slot != nullptr && slot->kind == Kind::Spring ? slot->velocity : 0.0f;
}

bool AnimStore::Settled(ImGuiID id, int sub) const
{
    const Slot* slot = Find(MakeKey(id, sub));
    return slot != nullptr && slot->settled;
}

AnimStore& GlobalAnimStore()
{
    static AnimStore store;
    return store;
}

namespace
{

struct MotionState
{
    bool followSystem = true;
    bool systemReduced = false;
    double nextSystemRead = 0.0;  // motion clock time of the next read of the setting
};

// The Windows setting is read at most this often (seconds of motion time).
constexpr double kSystemReadInterval = 1.0;

MotionState& Motion()
{
    static MotionState state;
    return state;
}

bool ReadSystemReduced()
{
    BOOL animations = TRUE;
    if (!SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animations, 0))
        return false;
    return animations == FALSE;
}

void UpdateSettings()
{
    const Theme& theme = T();
    AnimStore::Settings settings;
    settings.motionScale = std::max(0.0f, theme.motion.scale);
    settings.reduced = ReduceMotion();
    GlobalAnimStore().SetSettings(settings);
}

bool Init()
{
    MotionState& state = Motion();
    state.systemReduced = ReadSystemReduced();
    state.nextSystemRead = GlobalAnimStore().Clock() + kSystemReadInterval;
    UpdateSettings();
    return true;
}

void Shutdown()
{
    GlobalAnimStore().Clear();
}

void NewFrame()
{
    AnimStore& store = GlobalAnimStore();
    store.BeginFrame(DeltaTime());
    MotionState& state = Motion();
    if (store.Clock() >= state.nextSystemRead)
    {
        state.systemReduced = ReadSystemReduced();
        state.nextSystemRead = store.Clock() + kSystemReadInterval;
    }
    UpdateSettings();
}

void EndFrame()
{
    GlobalAnimStore().EndFrame();
}

} // namespace

double MotionClock()
{
    return GlobalAnimStore().Clock();
}

// The store keeps its slots between frames; the theme notification refreshes the motion
// scale and reduced flag at once instead of at the next frame.
Subsystem AnimSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "anim";
    subsystem.init = &Init;
    subsystem.shutdown = &Shutdown;
    subsystem.newFrame = &NewFrame;
    subsystem.endFrame = &EndFrame;
    subsystem.themeChanged = &UpdateSettings;
    return subsystem;
}

} // namespace internal

float EaseValue(Ease ease, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    switch (ease)
    {
    case Ease::Linear:
        return t;
    case Ease::InCubic:
        return t * t * t;
    case Ease::OutCubic:
    {
        const float u = 1.0f - t;
        return 1.0f - u * u * u;
    }
    case Ease::InOutCubic:
    {
        if (t < 0.5f)
            return 4.0f * t * t * t;
        const float u = -2.0f * t + 2.0f;
        return 1.0f - u * u * u * 0.5f;
    }
    case Ease::OutQuint:
        return 1.0f - std::pow(1.0f - t, 5.0f);
    case Ease::OutBack:
    {
        constexpr float kOvershoot = 1.70158f;
        const float u = t - 1.0f;
        return 1.0f + (kOvershoot + 1.0f) * u * u * u + kOvershoot * u * u;
    }
    }
    return t;
}

float Smooth(ImGuiID id, int sub, float target, float speed)
{
    return internal::GlobalAnimStore().Smooth(id, sub, target, speed);
}

float Spring(ImGuiID id, int sub, float target, const SpringSpec& spec)
{
    return internal::GlobalAnimStore().Spring(id, sub, target, spec);
}

float Tween(ImGuiID id, int sub, float target, float duration, Ease ease)
{
    return internal::GlobalAnimStore().Tween(id, sub, target, duration, ease);
}

ImVec4 SmoothColor(ImGuiID id, int sub, const ImVec4& target, float speed)
{
    return internal::GlobalAnimStore().SmoothColor(id, sub, target, speed);
}

bool ReduceMotion()
{
    const internal::MotionState& state = internal::Motion();
    return T().motion.reduceMotion || (state.followSystem && state.systemReduced);
}

bool SystemReduceMotion()
{
    return internal::Motion().systemReduced;
}

void SetFollowSystemMotion(bool follow)
{
    internal::Motion().followSystem = follow;
    internal::UpdateSettings();
}

bool FollowsSystemMotion()
{
    return internal::Motion().followSystem;
}

} // namespace facet
