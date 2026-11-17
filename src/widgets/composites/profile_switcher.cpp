// ProfileSwitcher (facet/widgets/composites.h): planned. A placeholder until Phase 2 implements it.

#include <algorithm>

#include "facet/widgets/composites.h"
#include "widgets/content_stub.h"

namespace facet
{

ProfileSwitcherAction ProfileSwitcher(const char* id, int*, std::span<const AvatarItem> profiles,
                                      const ProfileSwitcherOptions& options)
{
    namespace stub = internal::content;
    const float rows = static_cast<float>(std::max<std::size_t>(1, profiles.size()));
    stub::Stub("ProfileSwitcher", id, stub::ResolveWidth(options.width, 0.0f), stub::ControlRows(rows, ControlSize::Large));
    return ProfileSwitcherAction::None;
}

} // namespace facet