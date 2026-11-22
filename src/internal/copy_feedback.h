#pragma once

// How long a copy confirmation shows: CopyButton and CopyField swap their icon (and label) for
// the check, InlineCode, KeyValueList and CodeView show Strings::copied above the copied text.
// All of them hold it for FeedbackHoldSeconds (feedback_hold.h), so every copy confirmation
// stays in step with the other confirmations.

#include <imgui.h>

#include "internal/feedback_hold.h"

namespace facet::internal
{

// Seconds a copy confirmation stays: FeedbackHoldSeconds().
inline double CopyFeedbackHold()
{
    return FeedbackHoldSeconds();
}

// True while a confirmation that started at `copiedAt` (ImGui::GetTime(); < 0 for none) is shown.
inline bool CopyFeedbackActive(double copiedAt)
{
    return copiedAt >= 0.0 && ImGui::GetTime() - copiedAt < CopyFeedbackHold();
}

} // namespace facet::internal
