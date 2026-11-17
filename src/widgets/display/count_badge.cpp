// CountBadge (facet/widgets/display.h): planned. A placeholder until Phase 2 implements it.

#include <cstdio>

#include "facet/widgets/display.h"
#include "widgets/content_stub.h"

namespace facet
{

void CountBadge(int count, const CountBadgeOptions& options)
{
    namespace stub = internal::content;
    if (!stub::CanSubmit())
        return;
    if (count <= 0 && !options.showZero)
    {
        ImGui::Dummy(ImVec2(0.0f, 0.0f));  // a hidden badge still advances like one item
        return;
    }
    char text[24];
    if (options.maxCount > 0 && count > options.maxCount)
        std::snprintf(text, sizeof(text), "%d+", options.maxCount);
    else
        std::snprintf(text, sizeof(text), "%d", count < 0 ? 0 : count);
    const float height = ControlHeight(ControlSize::Small);
    stub::Stub("CountBadge", text, stub::TextWidth(text, TextStyle::Footnote, height), height);
}

} // namespace facet