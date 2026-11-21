// The frozen API of the overlays family (facet/widgets/overlays.h): every options struct follows
// the conventions, and every widget called closed (a false or null open flag, no change, no
// hover) inside a real frame keeps the stacks balanced, writes nothing and returns the neutral
// result. The behaviour of each widget is tested in tests/test_overlays.cpp.

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

// Calls every function of overlays.h once with its layer closed, checking balance and the neutral
// result. `v.closed` is false; null open flags show nothing.
void CallClosedOverlays(Values& v)
{
    CHECK_BALANCED(if (facet::BeginModal("Modal", &v.closed)) {
        FAIL("BeginModal opened");
        facet::EndModal();
    });
    CHECK_BALANCED(if (facet::BeginModal("Modal null", nullptr)) {
        FAIL("BeginModal opened without a flag");
        facet::EndModal();
    });
    CHECK_BALANCED(CHECK(facet::ConfirmDialog("Confirm", &v.closed, "Message") == facet::DialogResult::None));
    CHECK_BALANCED(CHECK(facet::ConfirmDialog("Confirm null", nullptr, "Message") == facet::DialogResult::None));
    CHECK_BALANCED(if (facet::BeginPopover("Popover", &v.closed)) {
        FAIL("BeginPopover opened");
        facet::EndPopover();
    });
    CHECK_BALANCED(if (facet::BeginPopover("Popover null", nullptr)) {
        FAIL("BeginPopover opened without a flag");
        facet::EndPopover();
    });
    CHECK_BALANCED(if (facet::BeginContextMenu("Context")) {
        FAIL("BeginContextMenu opened without a request");
        facet::EndContextMenu();
    });
    CHECK_BALANCED(facet::ItemTooltip("Tooltip %d", 1));
    CHECK_BALANCED(if (facet::BeginRichTooltip()) {
        FAIL("BeginRichTooltip opened without hover");
        facet::EndRichTooltip();
    });
    CHECK_BALANCED(if (facet::BeginDrawer("Drawer", &v.closed)) {
        FAIL("BeginDrawer opened");
        facet::EndDrawer();
    });
    CHECK_BALANCED(if (facet::BeginDrawer("Drawer null", nullptr)) {
        FAIL("BeginDrawer opened without a flag");
        facet::EndDrawer();
    });
    CHECK_BALANCED({
        facet::BeginLoadingOverlay("Loading", {.loading = true});
        ImGui::TextUnformatted("child");
        facet::EndLoadingOverlay();
    });
    CHECK_BALANCED(CHECK_FALSE(facet::CommandPalette("Commands", &v.closed, &v.picked, kItems)));
    CHECK_BALANCED(CHECK_FALSE(facet::CommandPalette("Command labels", &v.closed, &v.picked, kLabels)));
    CHECK_BALANCED(CHECK_FALSE(facet::CommandPalette("Shortcuts palette", &v.closed, &v.pickedShortcut)));
    CHECK_BALANCED(CHECK_FALSE(facet::CommandPalette("Palette null", nullptr, &v.picked, kItems)));
    CHECK_BALANCED(facet::WatermarkOverlay("Watermark", kFields));
    CHECK_BALANCED(CHECK(facet::UnsavedChangesBar("No changes", 0) == facet::UnsavedChangesAction::None));
}

} // namespace

TEST_CASE("Every overlay widget called closed is balanced, writes nothing and returns the neutral result")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    const int errorsBefore = facet::internal::UsageErrorCount();
    Values v;
    session.SetUi([&] {
        CallClosedOverlays(v);
        ImGui::BeginDisabled();
        CallClosedOverlays(v);
        ImGui::EndDisabled();
    });
    session.Frames(2);
    CHECK(v == Values{});
    // facet::EndFrame found no scope left open.
    CHECK(facet::internal::UsageErrorCount() == errorsBefore);
    CHECK(facet::internal::OpenScopeCount() == 0);
}

TEST_CASE("Toast calls outside a frame return an invalid id and change nothing")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    const facet::ToastId toast = facet::ShowToast("Toast");
    CHECK_FALSE(toast.IsValid());
    facet::UpdateToast(toast, "Toast");
    CHECK_FALSE(facet::ToastActionPressed(toast));
    CHECK_FALSE(facet::IsToastVisible(toast));
    facet::DismissToast(toast);
    facet::SetToastCorner(facet::ScreenCorner::BottomRight);
}

TEST_CASE("Overlay scope classes end only what began")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values v;
    session.SetUi([&] {
        CHECK_BALANCED({
            facet::ModalScope modal("modal", &v.closed);
            CHECK_FALSE(static_cast<bool>(modal));
        });
        CHECK_BALANCED({
            facet::PopoverScope popover("popover", &v.closed);
            CHECK_FALSE(static_cast<bool>(popover));
        });
        CHECK_BALANCED({
            facet::DrawerScope drawer("drawer", &v.closed);
            CHECK_FALSE(static_cast<bool>(drawer));
        });
        CHECK_BALANCED({ facet::ContextMenuScope menu("menu"); });
        CHECK_BALANCED({ facet::RichTooltipScope tip; });
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
        facet::EndPopover();
        facet::EndDrawer();
        facet::EndContextMenu();
        facet::EndRichTooltip();
        facet::EndLoadingOverlay();
        Snapshot end = Snapshot::Take(v);
        reported = facet::internal::UsageErrorCount() - before;
        end.errors = start.errors;
        CHECK(end == start);
    });
    session.Frame();
    CHECK(reported == 6);
}
