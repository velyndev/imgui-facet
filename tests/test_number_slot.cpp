// Number core (src/internal/text_field_number.h): the trailing slot a widget fills itself.

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "internal/text_field.h"
#include "internal/text_field_number.h"
#include "support/session.h"

namespace
{

struct SlotProbe
{
    int calls = 0;
    int presses = 0;
    ImRect rect;
};

void ProbeSlot(const facet::internal::TextFieldSlot& slot, void* user)
{
    SlotProbe& probe = *static_cast<SlotProbe*>(user);
    ++probe.calls;
    probe.rect = slot.rect;
    const facet::internal::ItemState button = facet::internal::TextFieldSlotButton(
        slot, 0, ImGui::GetIDWithSeed("probe", nullptr, slot.fieldId), facet::Icon("lock"), {.tabStop = true, .name = "Probe"});
    probe.presses += button.pressed ? 1 : 0;
}

} // namespace

TEST_CASE("Number field: a widget's own trailing slot sits at the end of the frame, without steppers only")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    double value = 2.0;
    SlotProbe probe;
    facet::internal::NumberFieldResult result;
    bool steppers = false;
    session.SetUi([&] {
        facet::internal::NumberFieldParams params;
        params.steppers = steppers;
        params.trailingWidth = facet::internal::TextFieldSlotButtonsWidth(1, facet::ControlSize::Regular);
        params.trailingSlot = ProbeSlot;
        params.trailingUser = &probe;
        result = facet::internal::NumberFieldBehavior("##slot", &value, params);
    });
    session.Frame();
    CHECK(probe.calls == 1);
    CHECK(probe.rect.Max.x <= result.field.frame.Max.x);
    CHECK(probe.rect.Min.x > result.field.text.Max.x - 1.0f);
    session.Click(probe.rect.GetCenter());
    CHECK(probe.presses == 1);
    CHECK(value == 2.0);
    // Steppers take the slot themselves.
    steppers = true;
    const int before = probe.calls;
    session.Frame();
    CHECK(probe.calls == before);
}
