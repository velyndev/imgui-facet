#pragma once

// How long a confirmation of a finished action stays: motion.feedbackHold, multiplied by
// motion.scale as the token says. Every user of the token reads it through FeedbackHoldSeconds,
// so a slowed or sped up theme keeps every confirmation in step and a negative scale never makes
// a hold negative. The copy confirmations (copy_feedback.h) and the SettingsSearch reveal
// highlight use it.

#include <algorithm>

#include "facet/core/theme.h"

namespace facet::internal
{

// Seconds a confirmation stays: motion.feedbackHold times motion.scale (never negative).
inline double FeedbackHoldSeconds()
{
    const Motion& motion = T().motion;
    return static_cast<double>(motion.feedbackHold) * static_cast<double>(std::max(motion.scale, 0.0f));
}

} // namespace facet::internal
