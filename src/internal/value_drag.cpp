// Value drag core (value_drag.h): value mapping, keyboard steps, drag accumulation and the
// input behaviour of value controls.

#include "internal/value_drag.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <iterator>
#include <span>
#include <utility>

#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "internal/scroll.h"

namespace facet::internal
{
namespace
{

// Tolerance in grid units when deciding whether a value lies on a grid point.
constexpr double kGridEpsilon = 1e-7;
// Ctrl while dragging snaps to this many steps (or tenths of the track).
constexpr double kCoarseSteps = 10.0;

double Clamp01(double t)
{
    if (!(t >= 0.0))
        return 0.0;
    return t > 1.0 ? 1.0 : t;
}

// Pure logarithm: a range above zero.
bool UsesLog(const ValueMapping& m)
{
    return m.scale == ValueScale::Logarithmic && m.min > 0.0;
}

// Logarithm on both sides of zero with a linear zone around it: a range that reaches zero or
// below, with logZero set.
bool UsesSplitLog(const ValueMapping& m)
{
    return m.scale == ValueScale::Logarithmic && !(m.min > 0.0) && m.logZero > 0.0;
}

// The split logarithm: linear in -e..e (slope 1/e), then 1 + ln(|v| / e) outward, so it is
// continuous with a continuous slope at +-e and odd around zero.
double SplitLog(double value, double e)
{
    const double a = std::fabs(value);
    const double y = a <= e ? a / e : 1.0 + std::log(a / e);
    return std::copysign(y, value);
}

double SplitExp(double y, double e)
{
    const double a = std::fabs(y);
    // Within a hair of the middle of the zero zone: exactly zero, so zero is reachable.
    if (a < kGridEpsilon * kGridEpsilon)
        return 0.0;
    const double v = a <= 1.0 ? a * e : e * std::exp(a - 1.0);
    return std::copysign(v, y);
}

// For a wrapping mapping: the grid index `target` brought back into 0 to `count` by whole
// turns; max (count) stays max when it is reached from below.
double WrapIndex(double target, double count)
{
    if (!(count > 0.0))
        return 0.0;
    if (target > count)
        target -= count * std::floor(target / count);
    else if (target < 0.0)
        target += count * std::ceil(-target / count);
    return target;
}

// The value of grid index `index` (min + index * step), with max as the stop after the last
// whole step.
double GridValue(const ValueMapping& m, double index)
{
    const double value = m.min + index * m.step;
    if (value >= m.max)
        return m.max;
    if (value <= m.min)
        return m.min;
    return value;
}

// How far from a grid point, in value units, a value still counts as on it: the rounding of a
// float that holds the value (a float 0.8 is 0.800000011920929 as a double). Callers keep most
// values in floats, and a value one float rounding off a grid point must step from that point,
// or the step lands on a value that rounds back to the same float and the key does nothing.
double FloatTolerance(double value)
{
    return std::isfinite(value) ? std::fabs(value) * static_cast<double>(FLT_EPSILON) : 0.0;
}

// The tolerance in grid units: at least kGridEpsilon, and never more than a tenth of a grid step,
// so a large value on a fine grid (where a float cannot tell neighbouring points apart anyway)
// never skips a point.
constexpr double kMaxGridTolerance = 0.1;

double GridTolerance(double floatSpread)
{
    return std::clamp(floatSpread, kGridEpsilon, kMaxGridTolerance);
}

// Moves `count` grid steps of `perStep` grid units in `direction` from `value`. A value between
// grid points goes to the next point in the direction first; one within a float rounding of a
// point (FloatTolerance) counts as on it. Above the last whole step, max counts as one more grid
// point.
double StepOnGrid(const ValueMapping& m, double value, int direction, double perStep, int count)
{
    const double epsilon = GridTolerance(FloatTolerance(value) / m.step);
    const double range = m.max - m.min;
    const double lastIndex = std::floor(range / m.step + kGridEpsilon);
    const bool maxOffGrid = range - lastIndex * m.step > m.step * kGridEpsilon;
    const double maxIndex = maxOffGrid ? lastIndex + 1.0 : lastIndex;
    // The position in grid units; max sits at maxIndex even when it is off the grid.
    double index = (std::clamp(value, m.min, m.max) - m.min) / m.step;
    if (maxOffGrid && index > lastIndex)
        index = lastIndex + (index - lastIndex) / ((range - lastIndex * m.step) / m.step);
    double target = 0.0;
    if (direction > 0)
        target = std::floor(index + epsilon) + perStep * count;
    else
        target = std::ceil(index - epsilon) - perStep * count;
    target = m.wrap ? WrapIndex(target, maxIndex) : std::clamp(target, 0.0, maxIndex);
    if (target >= maxIndex)
        return m.max;
    return GridValue(m, target);
}

struct Session
{
    ImGuiID id = 0;
    bool mouse = false;     // a mouse drag
    bool keyboard = false;  // a key or wheel edit
    double startValue = 0.0;
    double anchorValue = 0.0;  // the value at the drag's start fraction (after a jump)
    DragTracker tracker;
    ImVec2 lastMouse;
    bool pointerLost = false;  // the mouse position was invalid on a frame of the drag
};

Session& CurrentSession()
{
    static Session session;
    return session;
}

// The edit session of a two-axis item (ValueDrag2DBehavior).
struct Session2D
{
    ImGuiID id = 0;
    bool mouse = false;
    bool keyboard = false;
    double startX = 0.0;   // the values at the start of the session, for Escape
    double startY = 0.0;
    double anchorX = 0.0;  // the values at the drag's start fractions (after a jump)
    double anchorY = 0.0;
    DragTracker trackerX;
    DragTracker trackerY;
    ImVec2 lastMouse;
    bool pointerLost = false;
    int keptAxis = -1;     // while Shift is held: 0 the drag keeps x only, 1 y only; -1 not decided
};

Session2D& CurrentSession2D()
{
    static Session2D session;
    return session;
}

// Wheel movement below a whole notch (touchpads send fractions), kept for the item under the
// pointer until it adds up to a notch. A wheel session ends on the frame after its step, so the
// rest lives outside the session; it is dropped when the wheel turns another item, turns the
// other way, or stops applying to this item (the pointer left it).
struct WheelRest
{
    ImGuiID id = 0;
    double rest = 0.0;
};

WheelRest& CurrentWheelRest()
{
    static WheelRest rest;
    return rest;
}

// Sums of fractional notches such as ten times 0.1 land a hair below a whole notch.
constexpr double kNotchEpsilon = 1e-9;

struct CanceledRecord
{
    ImGuiID id = 0;
    int frame = -1;
};

CanceledRecord& Canceled()
{
    static CanceledRecord record;
    return record;
}

// The key presses of `key` this frame including repeats, when `id` may read it.
int PressCount(ImGuiKey key, ImGuiID id)
{
    if (!ImGui::TestKeyOwner(key, id))
        return 0;
    const ImGuiIO& io = ImGui::GetIO();
    return std::max(0, ImGui::GetKeyPressedAmount(key, io.KeyRepeatDelay, io.KeyRepeatRate));
}

} // namespace

ValueMapping NormalizeMapping(const ValueMapping& mapping)
{
    ValueMapping m = mapping;
    if (std::isnan(m.min))
        m.min = std::isnan(m.max) ? 0.0 : m.max;
    if (std::isnan(m.max))
        m.max = m.min;
    if (m.min > m.max)
        std::swap(m.min, m.max);
    if (!(m.step > 0.0) || std::isinf(m.step))
        m.step = 0.0;
    if (m.integer)
    {
        m.min = std::round(m.min);
        m.max = std::round(m.max);
        m.step = std::max(1.0, std::round(m.step));
    }
    if (!(m.logZero > 0.0) || std::isinf(m.logZero))
        m.logZero = 0.0;
    if (m.scale == ValueScale::Logarithmic && !(m.min > 0.0) && m.logZero == 0.0)
        m.scale = ValueScale::Linear;
    if (m.scale == ValueScale::Logarithmic && (std::isinf(m.min) || std::isinf(m.max)))
        m.scale = ValueScale::Linear;
    if (m.wrap && !(std::isfinite(m.min) && std::isfinite(m.max) && m.max > m.min))
        m.wrap = false;
    return m;
}

double ValueToFraction(const ValueMapping& mapping, double value)
{
    const ValueMapping m = NormalizeMapping(mapping);
    if (std::isnan(value) || !(m.max > m.min))
        return 0.0;
    value = std::clamp(value, m.min, m.max);
    if (UsesLog(m))
        return Clamp01((std::log(value) - std::log(m.min)) / (std::log(m.max) - std::log(m.min)));
    if (UsesSplitLog(m))
    {
        const double low = SplitLog(m.min, m.logZero);
        return Clamp01((SplitLog(value, m.logZero) - low) / (SplitLog(m.max, m.logZero) - low));
    }
    return Clamp01((value - m.min) / (m.max - m.min));
}

double FractionToValue(const ValueMapping& mapping, double fraction)
{
    const ValueMapping m = NormalizeMapping(mapping);
    const double t = Clamp01(fraction);
    if (t <= 0.0)
        return m.min;
    if (t >= 1.0)
        return m.max;
    double value = 0.0;
    if (UsesLog(m))
    {
        value = std::exp(std::log(m.min) + (std::log(m.max) - std::log(m.min)) * t);
    }
    else if (UsesSplitLog(m))
    {
        const double low = SplitLog(m.min, m.logZero);
        value = SplitExp(low + (SplitLog(m.max, m.logZero) - low) * t, m.logZero);
    }
    else
    {
        value = m.min + (m.max - m.min) * t;
    }
    return SnapValue(m, value);
}

double WrapValue(const ValueMapping& mapping, double value)
{
    const ValueMapping m = NormalizeMapping(mapping);
    if (!m.wrap || std::isnan(value))
        return value;
    const double range = m.max - m.min;
    if (value > m.max)
        value = m.min + std::fmod(value - m.min, range);
    else if (value < m.min)
        value = m.max - std::fmod(m.min - value, range);
    return std::clamp(value, m.min, m.max);
}

double OffsetValue(const ValueMapping& mapping, double value, double offset)
{
    const ValueMapping m = NormalizeMapping(mapping);
    if (std::isnan(value))
        value = std::isfinite(m.min) ? m.min : (std::isfinite(m.max) ? m.max : 0.0);
    double next = value + (std::isfinite(offset) ? offset : 0.0);
    next = m.wrap ? WrapValue(m, next) : std::clamp(next, m.min, m.max);
    if (m.step > 0.0)
    {
        // On the grid from min, or from zero when min is unbounded; the finite ends stay
        // reachable.
        const double origin = std::isfinite(m.min) ? m.min : 0.0;
        next = origin + std::round((next - origin) / m.step) * m.step;
    }
    if (m.integer)
        next = std::round(next);
    return m.wrap ? WrapValue(m, next) : std::clamp(next, m.min, m.max);
}

double SnapValue(const ValueMapping& mapping, double value)
{
    const ValueMapping m = NormalizeMapping(mapping);
    if (std::isnan(value))
        return m.min;
    value = std::clamp(value, m.min, m.max);
    if (m.step > 0.0)
    {
        const double range = m.max - m.min;
        const double lastIndex = std::floor(range / m.step + kGridEpsilon);
        const double lastGrid = m.min + lastIndex * m.step;
        if (value > lastGrid)
            value = (value - lastGrid < m.max - value) ? lastGrid : m.max;
        else
            value = GridValue(m, std::round((value - m.min) / m.step));
    }
    if (m.integer)
        value = std::round(value);
    return std::clamp(value, m.min, m.max);
}

double KeyboardStep(const ValueMapping& mapping, double value, int direction, StepSize size, int count)
{
    const ValueMapping m = NormalizeMapping(mapping);
    if (direction == 0 || count <= 0 || !(m.max > m.min))
        return std::isnan(value) ? m.min : std::clamp(value, m.min, m.max);
    if (std::isnan(value))
        value = direction > 0 ? m.min : m.max;
    direction = direction > 0 ? 1 : -1;
    if (m.step > 0.0)
    {
        const double perStep = size == StepSize::Coarse ? kCoarseSteps : 1.0;
        return StepOnGrid(m, value, direction, perStep, count);
    }
    const double grid = size == StepSize::Fine ? kKeyboardGridFine
                        : size == StepSize::Coarse ? kKeyboardGridCoarse
                                                   : kKeyboardGrid;
    const double index = ValueToFraction(m, value) * grid;
    // The float rounding of the value, as grid units of this track (StepOnGrid).
    const double tolerance = FloatTolerance(value);
    const double spread = (ValueToFraction(m, value + tolerance) - ValueToFraction(m, value - tolerance)) * 0.5 * grid;
    const double epsilon = GridTolerance(spread);
    double target = direction > 0 ? std::floor(index + epsilon) + count : std::ceil(index - epsilon) - count;
    target = m.wrap ? WrapIndex(target, grid) : std::clamp(target, 0.0, grid);
    return FractionToValue(m, target / grid);
}

double PageStep(const ValueMapping& mapping, double value, int direction, int count)
{
    const ValueMapping m = NormalizeMapping(mapping);
    if (direction == 0 || count <= 0 || !(m.max > m.min))
        return std::isnan(value) ? m.min : std::clamp(value, m.min, m.max);
    if (m.step > 0.0)
    {
        if (std::isnan(value))
            value = direction > 0 ? m.min : m.max;
        const double perStep = std::max(1.0, std::round((m.max - m.min) / m.step / 10.0));
        return StepOnGrid(m, value, direction > 0 ? 1 : -1, perStep, count);
    }
    return KeyboardStep(m, value, direction, StepSize::Coarse, count);
}

double ApplySnapStops(const ValueMapping& mapping, double value, double fraction, double radiusFraction)
{
    const ValueMapping m = NormalizeMapping(mapping);
    double best = radiusFraction;
    double result = value;
    for (const double stop : m.stops)
    {
        if (std::isnan(stop) || stop < m.min || stop > m.max)
            continue;
        const double distance = std::fabs(ValueToFraction(m, stop) - fraction);
        if (distance <= best)
        {
            best = distance;
            result = stop;
        }
    }
    return result;
}

void DragTracker::Begin(double fraction, double trackLength)
{
    start = fraction;
    travel = 0.0;
    length = std::max(1.0, trackLength);
}

void DragTracker::Move(double deltaPixels, double factor)
{
    if (std::isfinite(deltaPixels) && std::isfinite(factor))
        travel += deltaPixels * factor;
}

float FractionToTrack(const ImRect& track, Orientation orientation, double fraction)
{
    const double t = Clamp01(fraction);
    if (orientation == Orientation::Vertical)
        return static_cast<float>(track.Max.y - t * track.GetHeight());
    return static_cast<float>(track.Min.x + t * track.GetWidth());
}

double TrackToFraction(const ImRect& track, Orientation orientation, ImVec2 point)
{
    if (orientation == Orientation::Vertical)
    {
        const double height = track.GetHeight();
        return height > 0.0 ? Clamp01((track.Max.y - point.y) / height) : 0.0;
    }
    const double width = track.GetWidth();
    return width > 0.0 ? Clamp01((point.x - track.Min.x) / width) : 0.0;
}

void MarkEditCanceled(ImGuiID id)
{
    if (ImGui::GetCurrentContext() == nullptr)
        return;
    Canceled() = {id, ImGui::GetFrameCount()};
}

void ResetValueDragState()
{
    CurrentSession() = {};
    CurrentSession2D() = {};
    CurrentWheelRest() = {};
    Canceled() = {};
}

bool WasEditCanceled(ImGuiID id)
{
    if (ImGui::GetCurrentContext() == nullptr || id == 0)
        return false;
    const CanceledRecord& record = Canceled();
    return record.id == id && record.frame == ImGui::GetFrameCount();
}

ValueDragResult ValueDragBehavior(const ItemState& item, double* value, const ImRect& track,
                                  const ValueDragOptions& options)
{
    ValueDragResult result;
    ImGuiContext* context = ImGui::GetCurrentContext();
    if (context == nullptr || value == nullptr || item.id == 0)
        return result;
    ImGuiContext& g = *context;
    ImGuiWindow* window = g.CurrentWindow;
    ImGuiIO& io = g.IO;
    Session& session = CurrentSession();
    const ImGuiID id = item.id;
    const ValueMapping m = NormalizeMapping(options.mapping);
    const bool vertical = options.orientation == Orientation::Vertical;
    const bool readOnly = options.readOnly || item.readOnly;
    const double trackLength = std::max(1.0f, vertical ? track.GetHeight() : track.GetWidth());
    const double dragLength = options.dragLength > 0.0f ? options.dragLength : trackLength;
    const bool deltaMode = options.speed > 0.0 && std::isfinite(options.speed);
    // Delta mode: the value units of one key step.
    const double keyUnit = options.keyStep > 0.0 ? options.keyStep : (m.step > 0.0 ? m.step : options.speed);
    // A key or wheel step of `count` presses in `direction`, in either mode.
    const auto stepBy = [&](double from, int direction, StepSize size, int count) {
        if (!deltaMode)
            return KeyboardStep(m, from, direction, size, count);
        const bool whole = m.step > 0.0 || m.integer;
        const double factor = size == StepSize::Coarse ? kCoarseDeltaFactor
                              : size == StepSize::Fine && !whole ? kFineDragFactor
                                                                  : 1.0;
        return OffsetValue(m, from, direction * count * keyUnit * factor);
    };
    const auto pageBy = [&](double from, int direction, int count) {
        if (!deltaMode)
            return PageStep(m, from, direction, count);
        return OffsetValue(m, from, direction * count * keyUnit * kCoarseDeltaFactor);
    };

    // A session of an item that stopped being active (released, focus moved) is over.
    if (session.id == id && g.ActiveId != id)
        session = {};
    if (item.disabled || item.forced != ForcedItemState::None)
    {
        // A key or wheel session of this item that is still running ends here. Its active id
        // has no input source, so Dear ImGui would never release it, and every other item in
        // the app would stay blocked. A mouse drag is released by the button behaviour.
        if (session.id == id)
        {
            if (session.keyboard && g.ActiveId == id && g.ActiveIdSource == ImGuiInputSource_None)
                ImGui::ClearActiveID();
            session = {};
        }
        if (CurrentWheelRest().id == id)
            CurrentWheelRest() = {};
        return result;
    }

    const auto write = [&](double next) {
        if (readOnly || std::isnan(next))
            return;
        // Never past the limits of the caller (a handle's neighbours); an edit that the limits
        // stop at the current value writes nothing.
        if (options.clampLow > options.clampHigh)
            return;
        next = std::clamp(next, options.clampLow, options.clampHigh);
        if (next != *value)
        {
            *value = next;
            result.changed = true;
            MarkWidgetEdited(id);
        }
    };
    const auto cancel = [&]() {
        // Only a value that moved is written back: a session canceled before any change edits nothing.
        if (!readOnly && *value != session.startValue)
        {
            *value = session.startValue;
            result.changed = true;
            MarkWidgetEdited(id);
        }
        result.canceled = true;
        MarkEditCanceled(id);
        session = {};
        ImGui::ClearActiveID();
    };

    // ---- Mouse drag ----
    if (g.ActiveId == id && g.ActiveIdSource == ImGuiInputSource_Mouse && item.held)
    {
        const ImVec2 mouse = io.MousePos;
        if (session.id != id || !session.mouse)
        {
            // From where the button went down: a press handed over from another item a few
            // frames later (a run of handles waiting for the drag direction) keeps the travel
            // since the press.
            const ImVec2 press = ImGui::IsMousePosValid(&io.MouseClickedPos[ImGuiMouseButton_Left])
                                     ? io.MouseClickedPos[ImGuiMouseButton_Left]
                                     : mouse;
            session = {};
            session.id = id;
            session.mouse = true;
            session.startValue = *value;
            double start = deltaMode ? 0.0 : ValueToFraction(m, *value);
            if (options.grabRadius >= 0.0f && !deltaMode)
            {
                const float knob = FractionToTrack(track, options.orientation, start);
                const float along = vertical ? press.y : press.x;
                if (std::fabs(along - knob) > options.grabRadius)
                {
                    start = TrackToFraction(track, options.orientation, press);
                    write(ApplySnapStops(m, FractionToValue(m, start), start, Px(T().spacing.sm) / trackLength));
                }
            }
            // In delta mode the tracker counts pixels (a length of one).
            session.tracker.Begin(start, deltaMode ? 1.0 : dragLength);
            session.lastMouse = press;
            session.anchorValue = *value;
        }
        else if (!ImGui::IsMousePosValid(&mouse))
        {
            // The pointer is unknown (it left the platform window, or the backend lost it): the
            // value stays, and the drag goes on from wherever the pointer comes back.
            session.pointerLost = true;
        }
        else if (session.pointerLost)
        {
            session.pointerLost = false;
            session.lastMouse = mouse;
        }
        else
        {
            const double delta = vertical ? -(mouse.y - session.lastMouse.y) : mouse.x - session.lastMouse.x;
            session.lastMouse = mouse;
            const double factor = io.KeyShift ? kFineDragFactor : (deltaMode && io.KeyCtrl ? kCoarseDeltaFactor : 1.0);
            session.tracker.Move(delta, factor);
            if (deltaMode && delta != 0.0)
            {
                const double travel = session.tracker.Fraction();
                write(travel == 0.0 ? session.anchorValue : OffsetValue(m, session.anchorValue, travel * options.speed));
            }
            else if (delta != 0.0 || io.KeyCtrl)
            {
                // Past an end the fraction stops there, or turns over to the other end when the
                // mapping wraps.
                const double raw = session.tracker.Fraction();
                double t = Clamp01(raw);
                if (m.wrap && (raw < 0.0 || raw > 1.0))
                    t = raw - std::floor(raw);
                double next = FractionToValue(m, t);
                if (io.KeyCtrl)
                {
                    if (m.step > 0.0)
                        next = SnapValue(m, m.min + std::round((next - m.min) / (m.step * kCoarseSteps)) * m.step * kCoarseSteps);
                    else
                        next = FractionToValue(m, std::round(t * kKeyboardGridCoarse) / kKeyboardGridCoarse);
                }
                else
                {
                    next = ApplySnapStops(m, next, t, Px(T().spacing.sm) / trackLength);
                }
                // Back where the drag started: exactly the value it started from.
                if (!io.KeyCtrl && session.tracker.Fraction() == session.tracker.StartFraction())
                    next = session.anchorValue;
                write(next);
            }
        }
        result.dragging = true;
        ImGui::SetKeyOwner(ImGuiKey_Escape, id);
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, ImGuiInputFlags_None, id))
        {
            cancel();
            result.dragging = false;
        }
        return result;
    }

    // ---- Keyboard and wheel ----
    const bool focused = item.focused && g.NavId == id;
    const bool keysEnabled = options.keyboard && focused && !readOnly;
    const ImGuiKey decKey = vertical ? ImGuiKey_DownArrow : ImGuiKey_LeftArrow;
    const ImGuiKey incKey = vertical ? ImGuiKey_UpArrow : ImGuiKey_RightArrow;
    const ImGuiKey decPad = vertical ? ImGuiKey_GamepadDpadDown : ImGuiKey_GamepadDpadLeft;
    const ImGuiKey incPad = vertical ? ImGuiKey_GamepadDpadUp : ImGuiKey_GamepadDpadRight;
    // With crossArrows, the arrows across the track as well: Right and Up increase.
    const ImGuiKey crossDecKey = vertical ? ImGuiKey_LeftArrow : ImGuiKey_DownArrow;
    const ImGuiKey crossIncKey = vertical ? ImGuiKey_RightArrow : ImGuiKey_UpArrow;
    const ImGuiKey crossDecPad = vertical ? ImGuiKey_GamepadDpadLeft : ImGuiKey_GamepadDpadDown;
    const ImGuiKey crossIncPad = vertical ? ImGuiKey_GamepadDpadRight : ImGuiKey_GamepadDpadUp;
    const ImGuiKey allStepKeys[] = {decKey, incKey, decPad, incPad, ImGuiKey_PageUp, ImGuiKey_PageDown,
                                    ImGuiKey_Home, ImGuiKey_End, crossDecKey, crossIncKey, crossDecPad, crossIncPad};
    constexpr size_t kTrackStepKeys = 8;
    const std::span<const ImGuiKey> stepKeys(allStepKeys, options.crossArrows ? std::size(allStepKeys) : kTrackStepKeys);

    // A key session lasts while one of its keys is held.
    if (session.id == id && session.keyboard)
    {
        bool anyDown = false;
        for (const ImGuiKey key : stepKeys)
            anyDown = anyDown || ImGui::IsKeyDown(key);
        if (!anyDown || !focused)
        {
            session = {};
            ImGui::ClearActiveID();
        }
    }

    double next = *value;
    bool stepped = false;
    if (keysEnabled)
    {
        for (const ImGuiKey key : stepKeys)
            ImGui::SetKeyOwner(key, id);
        const StepSize size = io.KeyCtrl ? StepSize::Coarse : (io.KeyShift ? StepSize::Fine : StepSize::Normal);
        int inc = PressCount(incKey, id) + PressCount(incPad, id);
        int dec = PressCount(decKey, id) + PressCount(decPad, id);
        if (options.crossArrows)
        {
            inc += PressCount(crossIncKey, id) + PressCount(crossIncPad, id);
            dec += PressCount(crossDecKey, id) + PressCount(crossDecPad, id);
        }
        const int pageUp = PressCount(ImGuiKey_PageUp, id);
        const int pageDown = PressCount(ImGuiKey_PageDown, id);
        if (inc > 0)
            next = stepBy(next, 1, size, inc);
        if (dec > 0)
            next = stepBy(next, -1, size, dec);
        if (pageUp > 0)
            next = pageBy(next, 1, pageUp);
        if (pageDown > 0)
            next = pageBy(next, -1, pageDown);
        // The ends, where there are ends.
        const int home = std::isfinite(m.min) ? PressCount(ImGuiKey_Home, id) : 0;
        const int end = std::isfinite(m.max) ? PressCount(ImGuiKey_End, id) : 0;
        if (home > 0)
            next = m.min;
        if (end > 0)
            next = m.max;
        stepped = inc > 0 || dec > 0 || pageUp > 0 || pageDown > 0 || home > 0 || end > 0;
        // The keyboard is in use on this item now: Dear ImGui's navigation treats it as visible
        // focus, so the next Tab moves on instead of first showing the focus where it is.
        if (stepped)
            ImGui::SetNavCursorVisible(true);
    }

    // The wheel, over the item, while it is focused or Alt is held, unless the window under the
    // pointer is still scrolling: the scroll gesture keeps the wheel until it ends.
    const bool wheelEnabled = options.wheel && !readOnly && item.hovered && (focused || io.KeyAlt) &&
                              !ScrollWheelBelongsToScroll();
    WheelRest& wheelRest = CurrentWheelRest();
    if (!wheelEnabled && wheelRest.id == id)
        wheelRest = {};
    if (wheelEnabled)
    {
        ImGui::SetKeyOwner(ImGuiKey_MouseWheelY, id);
        ImGui::SetKeyOwner(ImGuiKey_MouseWheelX, id);
        double wheel = 0.0;
        if (ImGui::TestKeyOwner(ImGuiKey_MouseWheelY, id))
            wheel += io.MouseWheel;
        if (ImGui::TestKeyOwner(ImGuiKey_MouseWheelX, id))
            wheel -= io.MouseWheelH;
        // Fractional notches (touchpads) add up across frames until they make a whole one.
        if (wheelRest.id != id || wheel * wheelRest.rest < 0.0)
            wheelRest = {id, 0.0};
        const double total = wheel + wheelRest.rest;
        const int notches = static_cast<int>(std::trunc(total + std::copysign(kNotchEpsilon, total)));
        wheelRest.rest = total - notches;
        if (notches != 0)
        {
            const StepSize size = io.KeyCtrl ? StepSize::Coarse : (io.KeyShift ? StepSize::Fine : StepSize::Normal);
            next = stepBy(next, notches > 0 ? 1 : -1, size, std::abs(notches));
            stepped = true;
        }
    }

    if (stepped)
    {
        if (g.ActiveId != id)
        {
            // One edit session per key press (with its repeats) or wheel turn. The source is
            // none, so the button behaviour neither holds nor drops the item on later frames;
            // the session ends above when the keys are up.
            ImGui::SetActiveID(id, window);
            g.ActiveIdSource = ImGuiInputSource_None;
            // Keys need the focus already; the wheel with Alt focuses the item it turns, like a click.
            if (g.NavId != id)
                ImGui::SetFocusID(id, window);
            session = {};
            session.id = id;
            session.keyboard = true;
            session.startValue = *value;
        }
        write(next);
        result.keyboard = true;
    }

    if (session.id == id && session.keyboard)
    {
        result.keyboard = true;
        ImGui::SetKeyOwner(ImGuiKey_Escape, id);
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, ImGuiInputFlags_None, id))
        {
            cancel();
            result.keyboard = false;
        }
    }
    return result;
}

// ---- Two axes ------------------------------------------------------------------------------

namespace
{

// In PadKeys::Hold, Shift holds the point this share of the way from the middle to the edge.
constexpr double kHoldFineShare = 0.5;

} // namespace

ImVec2 PadPoint(const ImRect& area, const ValueMapping& mappingX, const ValueMapping& mappingY, bool yDown, double x,
                double y)
{
    const double fx = ValueToFraction(mappingX, x);
    const double fy = ValueToFraction(mappingY, y);
    const double px = area.Min.x + fx * area.GetWidth();
    const double py = yDown ? area.Min.y + fy * area.GetHeight() : area.Max.y - fy * area.GetHeight();
    return ImVec2(static_cast<float>(px), static_cast<float>(py));
}

void PadFractions(const ImRect& area, bool yDown, ImVec2 point, double* fx, double* fy)
{
    const double width = area.GetWidth();
    const double height = area.GetHeight();
    *fx = width > 0.0 ? (point.x - area.Min.x) / width : 0.0;
    *fy = height > 0.0 ? (yDown ? point.y - area.Min.y : area.Max.y - point.y) / height : 0.0;
}

void ClampToDisc(double* fx, double* fy)
{
    const double u = *fx * 2.0 - 1.0;
    const double v = *fy * 2.0 - 1.0;
    const double r = std::sqrt(u * u + v * v);
    if (!(r > 1.0) || !std::isfinite(r))
        return;
    *fx = 0.5 + 0.5 * u / r;
    *fy = 0.5 + 0.5 * v / r;
}

ValueDrag2DResult ValueDrag2DBehavior(const ItemState& item, double* x, double* y, const ImRect& area,
                                      const ValueDrag2DOptions& options)
{
    ValueDrag2DResult result;
    ImGuiContext* context = ImGui::GetCurrentContext();
    if (context == nullptr || x == nullptr || y == nullptr || item.id == 0)
        return result;
    ImGuiContext& g = *context;
    ImGuiWindow* window = g.CurrentWindow;
    ImGuiIO& io = g.IO;
    Session2D& session = CurrentSession2D();
    const ImGuiID id = item.id;
    const ValueMapping mx = NormalizeMapping(options.mappingX);
    const ValueMapping my = NormalizeMapping(options.mappingY);
    const bool readOnly = options.readOnly || item.readOnly;
    const double width = std::max(1.0f, area.GetWidth());
    const double height = std::max(1.0f, area.GetHeight());
    // Up on screen is toward max.y, or toward min.y when y grows downward.
    const int upSign = options.yDown ? -1 : 1;

    // A session of an item that stopped being active (released, focus moved) is over.
    if (session.id == id && g.ActiveId != id)
    {
        session = {};
        result.ended = true;
    }
    if (item.disabled || item.forced != ForcedItemState::None)
    {
        // As ValueDragBehavior: a key session without an input source would never be released.
        if (session.id == id)
        {
            if (session.keyboard && g.ActiveId == id && g.ActiveIdSource == ImGuiInputSource_None)
                ImGui::ClearActiveID();
            session = {};
            result.ended = true;
        }
        return result;
    }

    const auto write = [&](double nx, double ny) {
        if (readOnly)
            return;
        bool wrote = false;
        if (options.moveX && !std::isnan(nx) && options.clampLowX <= options.clampHighX)
        {
            nx = std::clamp(nx, options.clampLowX, options.clampHighX);
            if (nx != *x)
            {
                *x = nx;
                wrote = true;
            }
        }
        if (options.moveY && !std::isnan(ny) && options.clampLowY <= options.clampHighY)
        {
            ny = std::clamp(ny, options.clampLowY, options.clampHighY);
            if (ny != *y)
            {
                *y = ny;
                wrote = true;
            }
        }
        if (wrote)
        {
            result.changed = true;
            MarkWidgetEdited(id);
        }
    };
    const auto cancel = [&]() {
        if (!readOnly && (*x != session.startX || *y != session.startY))
        {
            *x = session.startX;
            *y = session.startY;
            result.changed = true;
            MarkWidgetEdited(id);
        }
        result.canceled = true;
        result.ended = true;
        MarkEditCanceled(id);
        session = {};
        ImGui::ClearActiveID();
    };
    // The value at a fraction of one axis, snapped to the coarse grid while Ctrl is held.
    const auto valueAt = [&](const ValueMapping& m, double t) {
        t = Clamp01(t);
        if (!io.KeyCtrl)
            return FractionToValue(m, t);
        if (m.step > 0.0)
        {
            const double coarse = m.step * kCoarseSteps;
            return SnapValue(m, m.min + std::round((FractionToValue(m, t) - m.min) / coarse) * coarse);
        }
        return FractionToValue(m, std::round(t * kKeyboardGridCoarse) / kKeyboardGridCoarse);
    };

    // ---- Mouse drag ----
    if (g.ActiveId == id && g.ActiveIdSource == ImGuiInputSource_Mouse && item.held)
    {
        const ImVec2 mouse = io.MousePos;
        if (session.id != id || !session.mouse)
        {
            // From where the button went down, also for a press handed over from another item.
            const ImVec2 press = ImGui::IsMousePosValid(&io.MouseClickedPos[ImGuiMouseButton_Left])
                                     ? io.MouseClickedPos[ImGuiMouseButton_Left]
                                     : mouse;
            session = {};
            session.id = id;
            session.mouse = true;
            session.startX = *x;
            session.startY = *y;
            double fx = ValueToFraction(mx, *x);
            double fy = ValueToFraction(my, *y);
            if (options.grabRadius >= 0.0f)
            {
                const ImVec2 point = PadPoint(area, mx, my, options.yDown, *x, *y);
                const float dx = options.moveX ? press.x - point.x : 0.0f;
                const float dy = options.moveY ? press.y - point.y : 0.0f;
                if (dx * dx + dy * dy > options.grabRadius * options.grabRadius)
                {
                    double px = 0.0;
                    double py = 0.0;
                    PadFractions(area, options.yDown, press, &px, &py);
                    px = Clamp01(px);
                    py = Clamp01(py);
                    if (options.disc)
                        ClampToDisc(&px, &py);
                    if (options.moveX)
                        fx = px;
                    if (options.moveY)
                        fy = py;
                    write(FractionToValue(mx, fx), FractionToValue(my, fy));
                }
            }
            session.trackerX.Begin(fx, width);
            session.trackerY.Begin(fy, height);
            session.lastMouse = press;
            session.anchorX = *x;
            session.anchorY = *y;
        }
        else if (!ImGui::IsMousePosValid(&mouse))
        {
            // The pointer is unknown: the point stays, and the drag goes on from where it returns.
            session.pointerLost = true;
        }
        else if (session.pointerLost)
        {
            session.pointerLost = false;
            session.lastMouse = mouse;
        }
        else
        {
            const double dx = mouse.x - session.lastMouse.x;
            const double dy = options.yDown ? mouse.y - session.lastMouse.y : -(mouse.y - session.lastMouse.y);
            session.lastMouse = mouse;
            session.trackerX.Move(dx);
            session.trackerY.Move(dy);
            if (dx != 0.0 || dy != 0.0 || io.KeyCtrl || io.KeyShift)
            {
                const double sx = session.trackerX.StartFraction();
                const double sy = session.trackerY.StartFraction();
                double fx = session.trackerX.Fraction();
                double fy = session.trackerY.Fraction();
                bool backX = fx == sx;
                bool backY = fy == sy;
                // Shift keeps the axis the drag goes along when it is pressed (the larger travel
                // since the press), until it is let go.
                if (options.axisLock && io.KeyShift && options.moveX && options.moveY)
                {
                    const double travelX = std::fabs(fx - sx) * width;
                    const double travelY = std::fabs(fy - sy) * height;
                    if (session.keptAxis < 0 && travelX != travelY)
                        session.keptAxis = travelX > travelY ? 0 : 1;
                    if (session.keptAxis == 0)
                        backY = true;
                    else if (session.keptAxis == 1)
                        backX = true;
                    else
                        backX = backY = true;
                }
                else
                {
                    session.keptAxis = -1;
                }
                if (backX)
                    fx = sx;
                if (backY)
                    fy = sy;
                fx = Clamp01(fx);
                fy = Clamp01(fy);
                if (options.disc)
                {
                    ClampToDisc(&fx, &fy);
                    // The edge of the disc moves both axes, so only a return of both is exact.
                    backX = backY = backX && backY;
                }
                // Back where the drag started: exactly the values it started from.
                const double nx = backX && !io.KeyCtrl ? session.anchorX : valueAt(mx, fx);
                const double ny = backY && !io.KeyCtrl ? session.anchorY : valueAt(my, fy);
                write(nx, ny);
            }
        }
        result.dragging = true;
        ImGui::SetKeyOwner(ImGuiKey_Escape, id);
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, ImGuiInputFlags_None, id))
        {
            cancel();
            result.dragging = false;
        }
        return result;
    }

    // ---- Keyboard ----
    const bool focused = item.focused && g.NavId == id;
    const bool keysEnabled = options.keyboard && focused && !readOnly;
    const bool hold = options.keys == PadKeys::Hold;
    ImGuiKey keys[14];
    int keyCount = 0;
    if (options.moveX)
    {
        for (const ImGuiKey key : {ImGuiKey_LeftArrow, ImGuiKey_RightArrow, ImGuiKey_GamepadDpadLeft, ImGuiKey_GamepadDpadRight})
            keys[keyCount++] = key;
    }
    if (options.moveY)
    {
        for (const ImGuiKey key : {ImGuiKey_UpArrow, ImGuiKey_DownArrow, ImGuiKey_GamepadDpadUp, ImGuiKey_GamepadDpadDown})
            keys[keyCount++] = key;
        if (!hold)
        {
            keys[keyCount++] = ImGuiKey_PageUp;
            keys[keyCount++] = ImGuiKey_PageDown;
        }
    }
    if (!hold && (options.moveX || options.moveY))
    {
        keys[keyCount++] = ImGuiKey_Home;
        keys[keyCount++] = ImGuiKey_End;
    }
    const std::span<const ImGuiKey> sessionKeys(keys, static_cast<size_t>(keyCount));

    // A key session lasts while one of its keys is held.
    if (session.id == id && session.keyboard)
    {
        bool anyDown = false;
        for (const ImGuiKey key : sessionKeys)
            anyDown = anyDown || ImGui::IsKeyDown(key);
        if (!anyDown || !focused)
        {
            session = {};
            ImGui::ClearActiveID();
            result.ended = true;
        }
    }

    double nx = *x;
    double ny = *y;
    bool stepped = false;
    if (keysEnabled && keyCount > 0)
    {
        for (const ImGuiKey key : sessionKeys)
            ImGui::SetKeyOwner(key, id);
        const auto count = [&](ImGuiKey key, ImGuiKey pad) { return PressCount(key, id) + PressCount(pad, id); };
        if (!hold)
        {
            const StepSize size = io.KeyCtrl ? StepSize::Coarse : (io.KeyShift ? StepSize::Fine : StepSize::Normal);
            const int right = options.moveX ? count(ImGuiKey_RightArrow, ImGuiKey_GamepadDpadRight) : 0;
            const int left = options.moveX ? count(ImGuiKey_LeftArrow, ImGuiKey_GamepadDpadLeft) : 0;
            const int up = options.moveY ? count(ImGuiKey_UpArrow, ImGuiKey_GamepadDpadUp) : 0;
            const int down = options.moveY ? count(ImGuiKey_DownArrow, ImGuiKey_GamepadDpadDown) : 0;
            const int pageUp = options.moveY ? PressCount(ImGuiKey_PageUp, id) : 0;
            const int pageDown = options.moveY ? PressCount(ImGuiKey_PageDown, id) : 0;
            const int home = PressCount(ImGuiKey_Home, id);
            const int end = PressCount(ImGuiKey_End, id);
            if (right > 0)
                nx = KeyboardStep(mx, nx, 1, size, right);
            if (left > 0)
                nx = KeyboardStep(mx, nx, -1, size, left);
            if (up > 0)
                ny = KeyboardStep(my, ny, upSign, size, up);
            if (down > 0)
                ny = KeyboardStep(my, ny, -upSign, size, down);
            if (pageUp > 0)
                ny = PageStep(my, ny, upSign, pageUp);
            if (pageDown > 0)
                ny = PageStep(my, ny, -upSign, pageDown);
            // Home and End act on x, or on y for an item that moves y only.
            if (home > 0)
                (options.moveX ? nx : ny) = options.moveX ? mx.min : my.min;
            if (end > 0)
                (options.moveX ? nx : ny) = options.moveX ? mx.max : my.max;
            stepped = right > 0 || left > 0 || up > 0 || down > 0 || pageUp > 0 || pageDown > 0 || home > 0 || end > 0;
            if (stepped && options.disc)
            {
                double fx = ValueToFraction(mx, nx);
                double fy = ValueToFraction(my, ny);
                const double ox = fx;
                const double oy = fy;
                ClampToDisc(&fx, &fy);
                if (fx != ox || fy != oy)
                {
                    nx = FractionToValue(mx, fx);
                    ny = FractionToValue(my, fy);
                }
            }
        }
        else
        {
            const auto down = [&](ImGuiKey key, ImGuiKey pad) {
                return (ImGui::TestKeyOwner(key, id) && ImGui::IsKeyDown(key)) ||
                       (ImGui::TestKeyOwner(pad, id) && ImGui::IsKeyDown(pad));
            };
            const int dirX = options.moveX ? static_cast<int>(down(ImGuiKey_RightArrow, ImGuiKey_GamepadDpadRight)) -
                                                 static_cast<int>(down(ImGuiKey_LeftArrow, ImGuiKey_GamepadDpadLeft))
                                           : 0;
            const int dirUp = options.moveY ? static_cast<int>(down(ImGuiKey_UpArrow, ImGuiKey_GamepadDpadUp)) -
                                                  static_cast<int>(down(ImGuiKey_DownArrow, ImGuiKey_GamepadDpadDown))
                                            : 0;
            bool anyArrow = false;
            for (const ImGuiKey key : sessionKeys)
                anyArrow = anyArrow || ImGui::IsKeyDown(key);
            if (anyArrow)
            {
                const double amount = io.KeyShift ? kHoldFineShare : 1.0;
                double u = dirX * amount;
                double v = dirUp * upSign * amount;
                // A round stick holds the diagonal on its edge, a square one in the corner.
                if (options.disc && u != 0.0 && v != 0.0)
                {
                    const double r = std::sqrt(u * u + v * v);
                    u = u / r * amount;
                    v = v / r * amount;
                }
                nx = FractionToValue(mx, 0.5 + 0.5 * u);
                ny = FractionToValue(my, 0.5 + 0.5 * v);
                stepped = true;
            }
        }
        if (stepped)
            ImGui::SetNavCursorVisible(true);
    }

    if (stepped)
    {
        if (g.ActiveId != id)
        {
            // One edit session per key press with its repeats (in Hold, while any arrow is down).
            ImGui::SetActiveID(id, window);
            g.ActiveIdSource = ImGuiInputSource_None;
            if (g.NavId != id)
                ImGui::SetFocusID(id, window);
            session = {};
            session.id = id;
            session.keyboard = true;
            session.startX = *x;
            session.startY = *y;
        }
        write(nx, ny);
        result.keyboard = true;
    }

    if (session.id == id && session.keyboard)
    {
        result.keyboard = true;
        ImGui::SetKeyOwner(ImGuiKey_Escape, id);
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, ImGuiInputFlags_None, id))
        {
            cancel();
            result.keyboard = false;
        }
    }
    return result;
}

} // namespace facet::internal
