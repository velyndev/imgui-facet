// OptionCardGroup: planned. Draws the placeholder at the group's default size until Phase 2
// implements the widget in this file.

#include "facet/widgets/selection.h"

#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "widgets/controls_stub.h"

namespace facet
{
namespace
{

// Default card size in multiples of spacing.xxxl until the theme has card tokens.
constexpr float kCardWidthInXxxl = 6.0f;
constexpr float kCardHeightInXxxl = 3.0f;

void Stub(const char* label, std::size_t cardCount, const OptionCardGroupOptions& options)
{
    const Theme& t = T();
    const float width = internal::StubWidth(options.width, 0.0f);
    const float gap = Px(t.spacing.sm);
    const float cardWidth = options.cardWidth > 0.0f ? Px(options.cardWidth) : Px(t.spacing.xxxl) * kCardWidthInXxxl;
    int columns = options.columns;
    if (columns <= 0)
        columns = static_cast<int>((width + gap) / (cardWidth + gap));
    if (columns < 1)
        columns = 1;
    const int count = cardCount > 0 ? static_cast<int>(cardCount) : 1;
    const int rows = (count + columns - 1) / columns;
    const float height = static_cast<float>(rows) * (Px(t.spacing.xxxl) * kCardHeightInXxxl + gap) - gap
                         + internal::StubMessageHeight(options.error, options.helpText);
    internal::ControlStub("OptionCardGroup", label, width, height);
}

} // namespace

bool OptionCardGroup(const char* label, int*, std::span<const OptionCard> cards, const OptionCardGroupOptions& options)
{
    Stub(label, cards.size(), options);
    return false;
}

bool OptionCardGroup(const char* label, std::span<bool>, std::span<const OptionCard> cards,
                     const OptionCardGroupOptions& options)
{
    Stub(label, cards.size(), options);
    return false;
}

} // namespace facet
