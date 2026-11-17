// Value drag core (value_drag.h): value mapping, keyboard steps, drag accumulation and the
// input behaviour of value controls.

#include "internal/value_drag.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "facet/core/scale.h"
#include "facet/core/theme.h"

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

// Moves `count` grid steps of `perStep` grid units in `direction` from `value`. A value between
// grid points goes to the next point in the direction first. Above the last whole step, max
// counts as one more grid point.
double StepOnGrid(const ValueMapping& m, double value, int direction, double perStep, int count)
{
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
        target = std::floor(index + kGridEpsilon) + perStep * count;
    else
        target = std::ceil(index - kGridEpsilon) - perStep * count;
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
    double target = direction > 0 ? std::floor(index + kGridEpsilon) + count : std::ceil(index - kGridEpsilon) - count;
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
        return result;

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
            ImGui::MarkItemEdited(id);
        }
    };
    const auto cancel = [&]() {
        // Only a value that moved is written back: a session canceled before any change edits nothing.
        if (!readOnly && *value != session.startValue)
        {
            *value = session.startValue;
            result.changed = true;
            ImGui::MarkItemEdited(id);
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
    const ImGuiKey stepKeys[] = {decKey, incKey, decPad, incPad, ImGuiKey_PageUp, ImGuiKey_PageDown,
                                 ImGuiKey_Home, ImGuiKey_End};

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
        const int inc = PressCount(incKey, id) + PressCount(incPad, id);
        const int dec = PressCount(decKey, id) + PressCount(decPad, id);
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

    // The wheel, over the item, while it is focused or Alt is held.
    const bool wheelEnabled = options.wheel && !readOnly && item.hovered && (focused || io.KeyAlt);
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

} // namespace facet::internal
