// SwatchPalette: planned. Draws the placeholder at the palette's default size until Phase 2
// implements the widget in this file.

#include "facet/widgets/pickers.h"

#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "widgets/controls_stub.h"

namespace facet
{
namespace
{

void Stub(const char* label, std::size_t swatchCount, bool history, const SwatchPaletteOptions& options)
{
    namespace stub = internal::controls;
    const float width = stub::StubWidth(options.width, 0.0f);
    const float swatch = ControlHeight(options.size);
    const float gap = Px(T().spacing.xs);
    int columns = options.columns;
    if (columns <= 0)
        columns = static_cast<int>((width + gap) / (swatch + gap));
    if (columns < 1)
        columns = 1;
    // The trailing + swatch of the editable form is counted like a swatch.
    const int count = static_cast<int>(swatchCount) + 1;
    const int rows = (count + columns - 1) / columns + (history ? 1 : 0);
    stub::ControlStub("SwatchPalette", label, width, static_cast<float>(rows) * (swatch + gap) - gap);
}

} // namespace

bool SwatchPalette(const char* label, ImVec4*, std::span<const ImVec4> swatches, ColorHistory* history,
                   const SwatchPaletteOptions& options)
{
    Stub(label, swatches.size(), history != nullptr, options);
    return false;
}

bool SwatchPalette(const char* label, ImVec4*, std::vector<ImVec4>* swatches, ColorHistory* history,
                   const SwatchPaletteOptions& options)
{
    Stub(label, swatches != nullptr ? swatches->size() : 0, history != nullptr, options);
    return false;
}

} // namespace facet
