// The control anatomy block (src/internal/control.h): metrics per control size, the fill
// feedback, fitted text, labels in front and above, the message line and the error records that
// ValidationSummary reads.

#include <cstring>
#include <string>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "internal/control.h"
#include "support/session.h"

TEST_CASE("Control metrics follow the control size and the tokens")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    const facet::Theme& t = facet::T();
    const facet::internal::ControlMetrics smallMetrics = facet::internal::GetControlMetrics(facet::ControlSize::Small);
    const facet::internal::ControlMetrics regularMetrics = facet::internal::GetControlMetrics(facet::ControlSize::Regular);
    const facet::internal::ControlMetrics largeMetrics = facet::internal::GetControlMetrics(facet::ControlSize::Large);
    CHECK(smallMetrics.height == doctest::Approx(facet::ControlHeight(facet::ControlSize::Small)));
    CHECK(largeMetrics.height == doctest::Approx(facet::ControlHeight(facet::ControlSize::Large)));
    CHECK(smallMetrics.paddingX == doctest::Approx(facet::Px(t.spacing.sm)));
    CHECK(regularMetrics.paddingX == doctest::Approx(facet::Px(t.spacing.md)));
    CHECK(largeMetrics.paddingX == doctest::Approx(facet::Px(t.spacing.lg)));
    CHECK(smallMetrics.textStyle == facet::TextStyle::Footnote);
    CHECK(regularMetrics.textStyle == facet::TextStyle::Body);
    CHECK(regularMetrics.radius == doctest::Approx(facet::Px(t.radii.control)));
    // Inherit resolves through the control size stack.
    facet::PushControlSize(facet::ControlSize::Large);
    CHECK(facet::internal::GetControlMetrics(facet::ControlSize::Inherit).size == facet::ControlSize::Large);
    facet::PopControlSize();
}

TEST_CASE("StateFill is the rest color at rest and the disabled fill while disabled")
{
    const facet::Colors& c = facet::T().colors;
    facet::internal::ItemState item;
    const ImVec4 rest = facet::internal::StateFill(c.accent, c.accentHover, c.accentPressed, item);
    CHECK(facet::ToU32(rest) == facet::ToU32(c.accent));
    item.pressAmount = 1.0f;
    const ImVec4 pressed = facet::internal::StateFill(c.accent, c.accentHover, c.accentPressed, item);
    CHECK(facet::ToU32(pressed) == facet::ToU32(c.accentPressed));
    item.disabled = true;
    CHECK(facet::ToU32(facet::internal::StateFill(c.accent, c.accentHover, c.accentPressed, item)) ==
          facet::ToU32(c.fill[3]));
}

TEST_CASE("An error replaces the help text, and an empty text is no message")
{
    using facet::internal::PickControlMessage;
    CHECK(PickControlMessage(nullptr, nullptr).IsSet() == false);
    CHECK(PickControlMessage("", "").IsSet() == false);
    const facet::internal::ControlMessage help = PickControlMessage(nullptr, "Help");
    CHECK(help.IsSet());
    CHECK_FALSE(help.error);
    const facet::internal::ControlMessage error = PickControlMessage("Wrong", "Help");
    CHECK(error.error);
    CHECK(std::string(error.text) == "Wrong");
    CHECK(PickControlMessage("", "Help").error == false);
}

TEST_CASE("Text fits its width with an ellipsis and labels take their share")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    facet::internal::FittedText fits;
    facet::internal::FittedText cut;
    facet::internal::FrontLabel front;
    facet::internal::FrontLabel none;
    facet::internal::AboveLabel above;
    float total = 0.0f;
    float gap = 0.0f;
    session.SetUi([&] {
        facet::internal::ControlTextScope text(facet::TextStyle::Body);
        fits = facet::internal::FitText("Short", nullptr, 1000.0f);
        cut = facet::internal::FitText("A label far too long for its box", nullptr, facet::Px(60.0f));
        total = facet::Px(120.0f);
        gap = facet::Px(facet::T().spacing.sm);
        front = facet::internal::MeasureFrontLabel("A label far too long for its box##field", facet::ControlSize::Regular,
                                                  total);
        none = facet::internal::MeasureFrontLabel("##field", facet::ControlSize::Regular, total);
        above = facet::internal::MeasureAboveLabel("Options", facet::ControlSize::Regular);
    });
    session.Frame();
    CHECK(std::string(fits.text) == "Short");
    CHECK_FALSE(fits.truncated);
    CHECK(cut.truncated);
    CHECK(cut.width <= facet::Px(60.0f) + 0.5f);
    // A long label in front takes at most half the widget and is cut.
    CHECK(front.shown.truncated);
    CHECK(front.width <= total * facet::internal::kFrontLabelMaxShare + 1.0f);
    CHECK(front.width >= front.shown.width + gap - 1.0f);
    CHECK(none.width == 0.0f);
    CHECK(none.text == nullptr);
    CHECK(above.height > 0.0f);
}

TEST_CASE("A drawn error message is reported to the next frame with its window and item")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ImGuiID windowId = 0;
    std::size_t seen = 0;
    std::string text;
    ImGuiID item = 0;
    ImGuiID recordWindow = 0;
    float bottom = 0.0f;
    float top = 0.0f;
    bool report = true;
    session.SetUi([&] {
        windowId = ImGui::GetCurrentWindow()->ID;
        const auto previous = facet::internal::PreviousFrameFieldErrors();
        seen = previous.size();
        if (!previous.empty())
        {
            text = previous[0].text;
            item = previous[0].itemId;
            recordWindow = previous[0].windowId;
        }
        if (!report)
            return;
        top = ImGui::GetCursorScreenPos().y;
        const ImVec2 at = ImGui::GetCursorScreenPos();
        const ImRect control(at, ImVec2(at.x + 100.0f, at.y + 20.0f));
        bottom = facet::internal::DrawControlMessage(facet::internal::PickControlMessage("Required", nullptr),
                                                     ImVec2(control.Min.x, control.Max.y), control.GetWidth(), false, 42,
                                                     control);
        // Help text is not reported.
        facet::internal::DrawControlMessage(facet::internal::PickControlMessage(nullptr, "Help"),
                                            ImVec2(control.Min.x, bottom), control.GetWidth(), false, 43, control);
    });
    session.Frame();
    CHECK(seen == 0);
    CHECK(bottom > top + 20.0f);
    session.Frame();
    CHECK(seen == 1);
    CHECK(text == "Required");
    CHECK(item == 42);
    CHECK(recordWindow == windowId);
    // A frame without reports leaves nothing for the one after it.
    report = false;
    session.Frame();
    session.Frame();
    CHECK(seen == 0);
}

TEST_CASE("Controls submit nothing outside a frame")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    CHECK(facet::internal::ControlWindow() == nullptr);
    ImGuiWindow* inside = nullptr;
    session.SetUi([&] { inside = facet::internal::ControlWindow(); });
    session.Frame();
    CHECK(inside != nullptr);
}
