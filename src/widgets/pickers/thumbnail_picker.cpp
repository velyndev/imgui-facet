// ThumbnailPicker: planned. Draws the placeholder at the grid's default size until Phase 2
// implements the widget in this file on the list and selection core.

#include "facet/widgets/pickers.h"

#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "widgets/controls_stub.h"

namespace facet
{
namespace
{

// Default tile side in spacing.xxxl until the theme has a token for it.
constexpr float kTileInXxxl = 3.0f;

void Stub(const char* label, std::size_t itemCount, const ThumbnailPickerOptions& options)
{
    namespace stub = internal::controls;
    const Theme& t = T();
    const float width = stub::StubWidth(options.width, 0.0f);
    const float gap = Px(t.spacing.sm);
    const float tileWidth = options.thumbnailSize.x > 0.0f ? Px(options.thumbnailSize.x) : Px(t.spacing.xxxl) * kTileInXxxl;
    const float tileHeight = options.thumbnailSize.y > 0.0f ? Px(options.thumbnailSize.y) : tileWidth;
    const float caption = MeasureText(TextStyle::Footnote, "Ag").y + Px(t.spacing.xs);
    int columns = options.columns;
    if (columns <= 0)
        columns = static_cast<int>((width + gap) / (tileWidth + gap));
    if (columns < 1)
        columns = 1;
    const int count = itemCount > 0 ? static_cast<int>(itemCount) : 1;
    const int rows = (count + columns - 1) / columns;
    stub::ControlStub("ThumbnailPicker", label, width, static_cast<float>(rows) * (tileHeight + caption + gap) - gap);
}

} // namespace

bool ThumbnailPicker(const char* label, int*, std::span<const ThumbnailItem> items, const ThumbnailPickerOptions& options)
{
    Stub(label, items.size(), options);
    return false;
}

bool ThumbnailPicker(const char* label, std::span<bool>, std::span<const ThumbnailItem> items,
                     const ThumbnailPickerOptions& options)
{
    Stub(label, items.size(), options);
    return false;
}

} // namespace facet
