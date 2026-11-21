// ShowToast and the toast queries (overlays.h). Planned: nothing is queued until Phase 2
// implements the toast stack (drawn from src/widgets/overlays/overlays_frame.cpp), so ShowToast returns
// an invalid id and every query answers for a toast that does not exist.

#include "facet/widgets/overlays.h"

namespace facet
{

ToastId ShowToast(const char*, const ToastOptions&)
{
    return ToastId{};
}

void UpdateToast(ToastId, const char*, const ToastOptions&)
{
}

bool ToastActionPressed(ToastId)
{
    return false;
}

bool IsToastVisible(ToastId)
{
    return false;
}

void DismissToast(ToastId)
{
}

void SetToastCorner(ScreenCorner)
{
}

} // namespace facet
