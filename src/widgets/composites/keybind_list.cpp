// KeybindList (facet/widgets/composites.h): planned. A placeholder until Phase 2 implements it.

#include <algorithm>

#include "facet/widgets/composites.h"
#include "widgets/content_stub.h"

namespace facet
{
namespace
{

void KeybindListStub(const char* id, int rows, const KeybindListOptions& options)
{
    namespace stub = internal::content;
    stub::Stub("KeybindList", id, stub::ResolveWidth(options.width, 0.0f),
               stub::ResolveHeight(options.height, stub::ControlRows(static_cast<float>(std::max(1, rows)))));
}

} // namespace

bool KeybindList(const char* id, const KeybindListOptions& options)
{
    KeybindListStub(id, ShortcutCount(), options);
    return false;
}

bool KeybindList(const char* id, std::span<KeybindListItem> items, const KeybindListOptions& options)
{
    KeybindListStub(id, static_cast<int>(items.size()), options);
    return false;
}

} // namespace facet