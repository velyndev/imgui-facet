// InventoryDetailCard (facet/widgets/composites.h): the details of one item, assembled from finished
// widgets: a Card (containers.h) with the title as its header, the image as an ImageView (media.h)
// beside the description, the stats as a key-value list (display.h) whose values show the
// difference to the compared value, the tags as badges and the actions as buttons in flows
// (layout.h), so both wrap at narrow widths. The card adds the layout and the stat differences.

#include "facet/widgets/composites.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "facet/widgets/buttons.h"
#include "facet/widgets/containers.h"
#include "facet/widgets/display.h"
#include "facet/widgets/layout.h"
#include "facet/widgets/media.h"
#include "internal/control.h"
#include "internal/item.h"

namespace facet
{
namespace
{

constexpr int kDetailTextSize = 64;

// The value of a stat, and its difference to the compared value with its sign: "12 (+3)". The
// tone says whether the difference is better or worse; the sign says which way it goes.
Tone InventoryDetailStatText(const InventoryStat& stat, const char* format, char* out, std::size_t outSize)
{
    char value[kDetailTextSize];
    ImFormatString(value, sizeof(value), format, stat.value);
    if (std::isnan(stat.compare))
    {
        ImFormatString(out, outSize, "%s", value);
        return Tone::Neutral;
    }
    const double difference = stat.value - stat.compare;
    char amount[kDetailTextSize];
    ImFormatString(amount, sizeof(amount), format, std::fabs(difference));
    if (difference == 0.0)
    {
        ImFormatString(out, outSize, "%s", value);
        return Tone::Neutral;
    }
    ImFormatString(out, outSize, "%s (%c%s)", value, difference > 0.0 ? '+' : '-', amount);
    const bool better = stat.lowerIsBetter ? difference < 0.0 : difference > 0.0;
    return better ? Tone::Success : Tone::Destructive;
}

} // namespace

int InventoryDetailCard(const char* label, ImageHandle image, const InventoryDetailCardOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return -1;
    if (label == nullptr)
        label = "";
    const internal::NextItemData next = internal::TakeNextItemData();
    const Theme& t = T();
    const ImGuiID cardId = window->GetID(label);
    const float available = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    const float width = internal::ResolveItemWidth(options.width, next, std::min(Px(t.metrics.cardWidth), available));
    const char* format = options.statFormat != nullptr ? options.statFormat : "%g";

    int pressed = -1;
    ImGui::BeginGroup();
    ImGui::PushID(label);
    BeginCard(label, {.width = width / Scale()});
    {
        // The picture beside the description, or above it where the card is too narrow for both.
        const float inner = std::max(1.0f, ImGui::GetContentRegionAvail().x);
        const float picture = std::min(Px(t.metrics.slotSize), inner);
        const bool hasImage = image.IsValid();
        const bool beside = hasImage && options.description != nullptr &&
                            inner - picture - Px(t.spacing.md) >= Px(t.layout.minRowControl);
        if (hasImage)
        {
            char alt[kDetailTextSize * 2];
            const char* end = ImGui::FindRenderedTextEnd(label);
            ImFormatString(alt, sizeof(alt), "%.*s##image", static_cast<int>(end - label), label);
            ImageView(alt, image, {.fit = ImageFit::Contain, .corners = Radius::Element, .width = picture / Scale(),
                                   .height = picture / Scale()});
            if (beside)
                ImGui::SameLine(0.0f, Px(t.spacing.md));
        }
        if (options.description != nullptr)
            StyledText(options.description, {.wrap = true});

        if (!options.stats.empty())
        {
            BeginKeyValueList("##stats");
            int index = 0;
            for (const InventoryStat& stat : options.stats)
            {
                ImGui::PushID(index++);
                char text[kDetailTextSize * 2];
                const Tone tone = InventoryDetailStatText(stat, format, text, sizeof(text));
                char key[kDetailTextSize * 2];
                ImFormatString(key, sizeof(key), "%s##stat", stat.label != nullptr ? stat.label : "");
                KeyValue(key, text, {.tone = tone, .wrap = false});
                ImGui::PopID();
            }
            EndKeyValueList();
        }

        if (!options.tags.empty())
        {
            BeginFlow("##tags", {.gap = Gap::Tight});
            for (const char* tag : options.tags)
                if (FlowNext())
                    Badge(tag);
            EndFlow();
        }

        if (!options.actions.empty())
        {
            BeginFlow("##actions");
            int index = 0;
            for (const char* action : options.actions)
            {
                ImGui::PushID(index);
                if (FlowNext() &&
                    Button(action, {.variant = index == 0 ? ButtonVariant::Primary : ButtonVariant::Secondary}))
                    pressed = index;
                ImGui::PopID();
                ++index;
            }
            EndFlow();
        }
    }
    EndCard();
    ImGui::PopID();
    ImGui::EndGroup();
    internal::FinishWidgetGroup(cardId);
    return pressed;
}

} // namespace facet
