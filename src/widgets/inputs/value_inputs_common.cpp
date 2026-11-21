// Helpers shared by the number-like inputs (value_inputs_common.h).

#include "widgets/inputs/value_inputs_common.h"

#include <imgui_internal.h>

#include "facet/core/scale.h"
#include "facet/widgets/common.h"
#include "internal/state_storage.h"

namespace facet::internal
{
namespace
{

struct ValueInputRejectState
{
    bool rejected = false;
};

} // namespace

const char* ValueInputError(ImGuiID id, const char* callerError)
{
    if (callerError != nullptr)
        return callerError;
    // Only fields that failed once have the state; asking for it keeps it alive while shown.
    if (GlobalStateStore().Find(id, StateTypeOf<ValueInputRejectState>()) == nullptr)
        return nullptr;
    return GetState<ValueInputRejectState>(id).rejected ? GetStrings().invalidValue : nullptr;
}

void ValueInputTrackRejected(ImGuiID id, bool parseFailed, bool written)
{
    if (parseFailed)
    {
        ValueInputSetRejected(id, true);
        return;
    }
    if (written || ImGui::IsItemActivated())
        ValueInputSetRejected(id, false);
}

void ValueInputSetRejected(ImGuiID id, bool rejected)
{
    if (!rejected && GlobalStateStore().Find(id, StateTypeOf<ValueInputRejectState>()) == nullptr)
        return;  // nothing to clear: no state is created for fields that never failed
    GetState<ValueInputRejectState>(id).rejected = rejected;
}

float ValueInputUnits(float pixels)
{
    const float unit = Px(1.0f);
    return unit > 0.0f ? pixels / unit : pixels;
}

} // namespace facet::internal
