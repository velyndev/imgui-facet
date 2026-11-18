// Rating: planned. Draws the placeholder at the widget's default size until Phase 2 implements
// it in this file.

#include "facet/widgets/inputs.h"

#include "widgets/controls_stub.h"

namespace facet
{
namespace
{

// Icons are clamped to the documented range of RatingOptions::count.
constexpr int kMaxIcons = 10;

void Stub(const char* label, const RatingOptions& options)
{
    namespace stub = internal::controls;
    int icons = options.count < 1 ? 1 : options.count;
    if (icons > kMaxIcons)
        icons = kMaxIcons;
    const float content = stub::StubLabelWidth(label, options.size) + static_cast<float>(icons) * ControlHeight(options.size);
    stub::ControlStub("Rating", label, stub::StubFitAvailable(content),
                          stub::StubHeight(options.size, options.error, options.helpText));
}

} // namespace

bool Rating(const char* label, float*, const RatingOptions& options)
{
    Stub(label, options);
    return false;
}

bool Rating(const char* label, int*, const RatingOptions& options)
{
    Stub(label, options);
    return false;
}

} // namespace facet
