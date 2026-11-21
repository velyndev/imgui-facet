#pragma once

// How long a copy confirmation shows: CopyButton and CopyField swap their icon (and label) for
// the check, InlineCode, KeyValueList and CodeView show Strings::copied above the copied text.
// All of them hold it for motion.feedbackHold, multiplied by motion.scale as the token says, so a
// slowed or sped up theme keeps every copy confirmation in step.

#include <algorithm>

#include <imgui.h>

#include "facet/core/theme.h"

namespace facet::internal
{

// Seconds a copy confirmation stays: motion.feedbackHold times motion.scale (never negative).
inline double CopyFeedbackHold()
{
    const Motion& motion = T().motion;
    return static_cast<double>(motion.feedbackHold) * static_cast<double>(std::max(motion.scale, 0.0f));
}

// True while a confirmation that started at `copiedAt` (ImGui::GetTime(); < 0 for none) is shown.
inline bool CopyFeedbackActive(double copiedAt)
{
    return copiedAt >= 0.0 && ImGui::GetTime() - copiedAt < CopyFeedbackHold();
}

} // namespace facet::internal
