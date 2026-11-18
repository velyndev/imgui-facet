// PinCodeInput: planned. Draws the placeholder at the widget's default size until Phase 2
// implements it in this file on the text field core.

#include "facet/widgets/inputs.h"

#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "widgets/controls_stub.h"

namespace facet
{
namespace
{

// Cells are clamped to the documented range of PinCodeInputOptions::length.
constexpr int kMaxCells = 16;

void Stub(const char* label, const PinCodeInputOptions& options)
{
    namespace stub = internal::controls;
    int cells = options.length < 1 ? 1 : options.length;
    if (cells > kMaxCells)
        cells = kMaxCells;
    const float cell = ControlHeight(options.size);
    const float gap = Px(T().spacing.xs);
    const float content = stub::StubLabelWidth(label, options.size) + static_cast<float>(cells) * (cell + gap) - gap;
    stub::ControlStub("PinCodeInput", label, stub::StubFittedWidth(options.width, content),
                          stub::StubHeight(options.size, options.error, options.helpText));
}

} // namespace

bool PinCodeInput(const char* label, std::string*, const PinCodeInputOptions& options)
{
    Stub(label, options);
    return false;
}

bool PinCodeInput(const char* label, char*, std::size_t, const PinCodeInputOptions& options)
{
    Stub(label, options);
    return false;
}

} // namespace facet
