#pragma once

// The caller's number types (int, float, double) on top of the double the number core edits,
// for the number-like inputs of inputs.h. Header only, because the functions are templates.
//
//     double low = options.min, high = options.max;
//     NumberValueClampRange<int>(&low, &high);          // [INT_MIN, INT_MAX] at most
//     double edited = *value;
//     ... NumberFieldBehavior(label, &edited, {.min = low, .max = high, .integer = true}) ...
//     if (result.changed && NumberValueStore(edited, value)) return true;

#include <algorithm>
#include <cfloat>
#include <climits>
#include <cmath>
#include <type_traits>

namespace facet::internal
{

// The smallest and largest value of T as a double (finite for float, exact for int).
template <class T>
constexpr double NumberValueLowest()
{
    if constexpr (std::is_integral_v<T>)
        return static_cast<double>(INT_MIN);
    else if constexpr (std::is_same_v<T, float>)
        return -static_cast<double>(FLT_MAX);
    else
        return -DBL_MAX;
}

template <class T>
constexpr double NumberValueHighest()
{
    if constexpr (std::is_integral_v<T>)
        return static_cast<double>(INT_MAX);
    else if constexpr (std::is_same_v<T, float>)
        return static_cast<double>(FLT_MAX);
    else
        return DBL_MAX;
}

// Orders [*low, *high] (min > max is swapped, a NaN bound is open) and limits it to T.
template <class T>
void NumberValueClampRange(double* low, double* high)
{
    double a = std::isnan(*low) ? -HUGE_VAL : *low;
    double b = std::isnan(*high) ? HUGE_VAL : *high;
    if (a > b)
        std::swap(a, b);
    *low = std::clamp(a, NumberValueLowest<T>(), NumberValueHighest<T>());
    *high = std::clamp(b, NumberValueLowest<T>(), NumberValueHighest<T>());
}

// A step for T: 0 or less (or NaN) is no step; integer steps are whole and at least 1.
template <class T>
double NumberValueStep(double step)
{
    if (!(step > 0.0) || !std::isfinite(step))
        return 0.0;
    if constexpr (std::is_integral_v<T>)
        return std::max(1.0, std::round(step));
    else
        return step;
}

// Converts an edited double to T inside T's range and writes it when it differs from *value.
// Returns whether *value changed.
template <class T>
bool NumberValueStore(double edited, T* value)
{
    if (value == nullptr || std::isnan(edited))
        return false;
    edited = std::clamp(edited, NumberValueLowest<T>(), NumberValueHighest<T>());
    T next{};
    if constexpr (std::is_integral_v<T>)
        next = static_cast<T>(std::llround(edited));
    else
        next = static_cast<T>(edited);
    if (next == *value)
        return false;
    *value = next;
    return true;
}

} // namespace facet::internal
