// AccentPicker (facet/widgets/composites.h): planned. A placeholder until Phase 2 implements it.

#include "facet/widgets/composites.h"
#include "widgets/content_stub.h"

namespace facet
{

bool AccentPicker(const char* label, ImVec4*, const AccentPickerOptions& options)
{
    namespace stub = internal::content;
    const std::size_t presets = options.presets.empty() ? AccentPresets().size() : options.presets.size();
    const float swatches = static_cast<float>(presets + (options.customColor ? 1u : 0u));
    const float height = ControlHeight(options.size);
    stub::Stub("AccentPicker", label, height * (swatches > 0.0f ? swatches : 1.0f), height);
    return false;
}

} // namespace facet