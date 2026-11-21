// The frozen API of the overlays family (facet/widgets/overlays.h): every options struct follows
// the conventions, and every widget that is still planned is safe to call inside a real frame
// (stacks balanced, no value written, the neutral result). The placeholder helpers themselves are
// tested in tests/test_api_structure.cpp.

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "support/options_check.h"
#include "support/session.h"
#include "support/structure_stub_check.h"

namespace
{

using facet::test::OptionsFollowConventions;
using namespace facet::test::structure;

static_assert(OptionsFollowConventions<facet::ModalOptions>());
static_assert(OptionsFollowConventions<facet::ConfirmDialogOptions>());
static_assert(OptionsFollowConventions<facet::PopoverOptions>());
static_assert(OptionsFollowConventions<facet::ContextMenuOptions>());
static_assert(OptionsFollowConventions<facet::TooltipOptions>());
static_assert(OptionsFollowConventions<facet::ToastOptions>());
static_assert(OptionsFollowConventions<facet::DrawerOptions>());
static_assert(OptionsFollowConventions<facet::LoadingOverlayOptions>());
static_assert(OptionsFollowConventions<facet::CommandPaletteOptions>());
static_assert(OptionsFollowConventions<facet::WatermarkOptions>());
static_assert(OptionsFollowConventions<facet::UnsavedChangesBarOptions>());

// Calls every function of overlays.h once, checking balance and the neutral result.
void CallOverlayStubs(Values& v)
{
    CHECK_BALANCED(if (facet::BeginModal("Modal", &v.open)) {
        FAIL("BeginModal opened");
        facet::EndModal();
    });
    CHECK_BALANCED(CHECK(facet::ConfirmDialog("Confirm", &v.open, "Message") == facet::DialogResult::None));
    CHECK_BALANCED(if (facet::BeginPopover("Popover", &v.open)) {
        FAIL("BeginPopover opened");
        facet::EndPopover();
    });
    CHECK_BALANCED(if (facet::BeginContextMenu("Context")) {
        FAIL("BeginContextMenu opened");
        facet::EndContextMenu();
    });
    CHECK_BALANCED({
        const facet::ToastId toast = facet::ShowToast("Toast");
        CHECK_FALSE(toast.IsValid());
        facet::UpdateToast(toast, "Toast");
        CHECK_FALSE(facet::ToastActionPressed(toast));
        CHECK_FALSE(facet::IsToastVisible(toast));
        facet::DismissToast(toast);
        facet::SetToastCorner(facet::ScreenCorner::TopLeft);
    });
    CHECK_BALANCED(if (facet::BeginDrawer("Drawer", &v.open)) {
        FAIL("BeginDrawer opened");
        facet::EndDrawer();
    });
    CHECK_BALANCED({
        facet::BeginLoadingOverlay("Loading", {.loading = true});
        ImGui::TextUnformatted("child");
        facet::EndLoadingOverlay();
    });
    CHECK_BALANCED(CHECK_FALSE(facet::CommandPalette("Commands", &v.open, &v.picked, kItems)));
    CHECK_BALANCED(CHECK_FALSE(facet::CommandPalette("Command labels", &v.open, &v.picked, kLabels)));
    CHECK_BALANCED(CHECK_FALSE(facet::CommandPalette("Shortcuts palette", &v.open, &v.pickedShortcut)));
    CHECK_BALANCED(facet::WatermarkOverlay("Watermark", kFields));
    CHECK_BALANCED(CHECK(facet::UnsavedChangesBar("Changes", 2) == facet::UnsavedChangesAction::None));
}

} // namespace

TEST_CASE("Every overlay widget stub is safe to call, also disabled, and returns the neutral result")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    const int errorsBefore = facet::internal::UsageErrorCount();
    Values v;
    session.SetUi([&] {
        CallOverlayStubs(v);
        ImGui::BeginDisabled();
        CallOverlayStubs(v);
        ImGui::EndDisabled();
    });
    session.Frames(2);
    CHECK(v == Values{});
    // facet::EndFrame found no scope left open.
    CHECK(facet::internal::UsageErrorCount() == errorsBefore);
    CHECK(facet::internal::OpenScopeCount() == 0);
}

TEST_CASE("Overlay scope classes end only what began")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values v;
    session.SetUi([&] {
        CHECK_BALANCED({
            facet::ModalScope modal("modal", &v.open);
            CHECK_FALSE(static_cast<bool>(modal));
        });
        CHECK_BALANCED({ facet::LoadingOverlayScope loading("loading"); });
    });
    session.Frame();
}

TEST_CASE("An overlay End without its Begin is reported and changes no stack")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values v;
    int reported = 0;
    session.SetUi([&] {
        const int before = facet::internal::UsageErrorCount();
        const Snapshot start = Snapshot::Take(v);
        facet::EndModal();
        Snapshot end = Snapshot::Take(v);
        reported = facet::internal::UsageErrorCount() - before;
        end.errors = start.errors;
        CHECK(end == start);
    });
    session.Frame();
    CHECK(reported == 1);
}
