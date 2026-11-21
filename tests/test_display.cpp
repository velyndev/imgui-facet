// Behaviour of the display family (facet/widgets/display.h, DataTable excepted): one block of test
// cases per widget, in the order the widgets were implemented. The API checks of every display
// widget are in tests/test_api_display.cpp.

#include <cfloat>
#include <cmath>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "internal/control.h"
#include "internal/focus.h"
#include "internal/list_view.h"
#include "internal/loading.h"
#include "internal/popup_host.h"
#include "internal/text_util.h"
#include "support/session.h"
#include "widgets/display/display_look.h"

namespace
{

// The rightmost and leftmost x of the vertices of `color` the window draw list received since
// `from`; FLT_MAX and -FLT_MAX when there are none.
struct Span
{
    float min = FLT_MAX;
    float max = -FLT_MAX;
    bool Empty() const { return max < min; }
};

inline Span ColorSpan(int from, ImU32 color)
{
    const ImDrawList* drawList = ImGui::GetWindowDrawList();
    Span span;
    for (int i = from; i < drawList->VtxBuffer.Size; ++i)
    {
        const ImDrawVert& v = drawList->VtxBuffer[i];
        if (v.col != color)
            continue;
        span.min = std::fmin(span.min, v.pos.x);
        span.max = std::fmax(span.max, v.pos.x);
    }
    return span;
}

// Vertices of `color` (any alpha when `anyAlpha`) the window draw list received since `from`.
inline int CountColor(int from, ImU32 color, bool anyAlpha = false)
{
    const ImDrawList* drawList = ImGui::GetWindowDrawList();
    int count = 0;
    for (int i = from; i < drawList->VtxBuffer.Size; ++i)
    {
        const ImU32 c = drawList->VtxBuffer[i].col;
        if (anyAlpha ? (c & ~IM_COL32_A_MASK) == (color & ~IM_COL32_A_MASK) && (c & IM_COL32_A_MASK) != 0 : c == color)
            ++count;
    }
    return count;
}

inline int VertexCount()
{
    return ImGui::GetWindowDrawList()->VtxBuffer.Size;
}

// The theme and the motion settings a test changes, restored at the end.
struct DisplayGuard
{
    facet::Theme saved = facet::T();
    bool follow = facet::FollowsSystemMotion();
    float scale = facet::UiScale();
    DisplayGuard()
    {
        facet::SetFollowSystemMotion(false);
        facet::SetTheme(facet::Theme::Light(), false);
    }
    ~DisplayGuard()
    {
        facet::SetUiScale(scale);
        facet::SetTheme(saved, false);
        facet::SetFollowSystemMotion(follow);
    }
    static void ReduceMotion(bool reduce)
    {
        facet::Theme theme = facet::T();
        theme.motion.reduceMotion = reduce;
        facet::SetTheme(theme, false);
    }
};

// Whether the zero-terminated 	ext is valid UTF-8: every lead byte has its continuation bytes.
inline bool ValidUtf8(const char* text)
{
    const unsigned char* p = reinterpret_cast<const unsigned char*>(text);
    while (*p != 0)
    {
        int extra = 0;
        if (*p < 0x80)
            extra = 0;
        else if ((*p & 0xE0) == 0xC0)
            extra = 1;
        else if ((*p & 0xF0) == 0xE0)
            extra = 2;
        else if ((*p & 0xF8) == 0xF0)
            extra = 3;
        else
            return false;
        ++p;
        for (int i = 0; i < extra; ++i, ++p)
            if ((*p & 0xC0) != 0x80)
                return false;
    }
    return true;
}

inline bool TooltipOpen(ImGuiID itemId)
{
    return facet::internal::IsPopupLevelOpen(facet::internal::PopupTooltipLevel(itemId));
}

// The baseline of Body text whose line starts at `top`.
inline float BodyBaseline(float top)
{
    facet::PushTextStyle(facet::TextStyle::Body);
    const float baseline = top + std::round(facet::internal::DisplayFontAscent());
    facet::PopTextStyle();
    return baseline;
}

constexpr const char* kUtf8Text = "Gr\xC3\xBC\xC3\x9F\x65 aus K\xC3\xB6ln \xE2\x82\xAC \xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E und mehr Text";

} // namespace

// ==== ProgressBar ====

TEST_CASE("A progress bar is one control height without a label line and a line plus its track with one")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float available = 0.0f;
    session.SetUi([&] {
        available = ImGui::GetContentRegionAvail().x;
        facet::ProgressBar("##bare", 0.5f);
        session.Track("bare");
        facet::ProgressBar("Copying", 0.5f);
        session.Track("labelled");
        facet::ProgressBar("##percent", 0.5f, {.showPercent = true});
        session.Track("percent");
        facet::ProgressBar("Copying##large", 0.5f, {.size = facet::ControlSize::Large});
        session.Track("large");
        facet::ProgressBar("##units", 0.5f, {.width = 120.0f});
        session.Track("units");
    });
    session.Frame();
    const facet::Metrics& m = facet::T().metrics;
    CHECK(session.ItemRect("bare").GetHeight() == doctest::Approx(facet::ControlHeight(facet::ControlSize::Regular)));
    const float line = session.ItemRect("labelled").GetHeight() - facet::PxSnapped(m.progressHeight);
    CHECK(line > 0.0f);
    CHECK(session.ItemRect("percent").GetHeight() == doctest::Approx(session.ItemRect("labelled").GetHeight()));
    CHECK(session.ItemRect("large").GetHeight() > session.ItemRect("labelled").GetHeight());
    // Field width by default (rule 6), units when asked.
    CHECK(session.ItemRect("bare").GetWidth() == doctest::Approx(available));
    CHECK(session.ItemRect("units").GetWidth() == doctest::Approx(facet::Px(120.0f)));
}

TEST_CASE("A progress bar is not a Tab stop and reports hover like any item")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool hovered = false;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        facet::ProgressBar("Progress", 0.3f);
        session.Track("bar");
        hovered = ImGui::IsItemHovered();
        ImGui::Button("after");
        session.Track("after");
    });
    session.Frame();
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("after"));
    session.MouseMove(session.ItemCenter("bar"));
    session.Frame();
    CHECK(hovered);
}

TEST_CASE("The progress fill eases to a new value and is clamped to the track")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    DisplayGuard guard;
    const ImU32 tone = facet::ToU32(facet::T().colors.focusRing);
    float fraction = 0.0f;
    Span fill;
    Span over;
    Span nan;
    ImRect bar;
    session.SetUi([&] {
        int from = ImGui::GetWindowDrawList()->VtxBuffer.Size;
        facet::ProgressBar("##eased", fraction);
        bar = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        fill = ColorSpan(from, tone);
        from = ImGui::GetWindowDrawList()->VtxBuffer.Size;
        facet::ProgressBar("##over", 2.0f);
        over = ColorSpan(from, tone);
        from = ImGui::GetWindowDrawList()->VtxBuffer.Size;
        facet::ProgressBar("##nan", std::numeric_limits<float>::quiet_NaN());
        nan = ColorSpan(from, tone);
    });
    session.Frame();
    CHECK(fill.Empty());
    CHECK(nan.Empty());
    CHECK(over.max == doctest::Approx(bar.Max.x).epsilon(0.01));
    fraction = 1.0f;
    session.Frame();
    CHECK_FALSE(fill.Empty());
    CHECK(fill.max < bar.Max.x - 1.0f);  // on its way
    session.Seconds(facet::T().motion.durationStandard + 0.1f);
    CHECK(fill.max == doctest::Approx(bar.Max.x).epsilon(0.01));
}

TEST_CASE("An indeterminate progress bar sweeps a segment along the track")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    DisplayGuard guard;
    const ImU32 tone = facet::ToU32(facet::T().colors.focusRing);
    Span segment;
    ImRect bar;
    session.SetUi([&] {
        const int from = ImGui::GetWindowDrawList()->VtxBuffer.Size;
        facet::ProgressBar("##busy", facet::kIndeterminate);
        bar = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        segment = ColorSpan(from, tone);
    });
    // Sample the segment over one sweep: it moves, and never leaves the track.
    float first = 0.0f;
    bool moved = false;
    for (int i = 0; i < 12; ++i)
    {
        session.Seconds(0.1f);
        if (segment.Empty())
            continue;
        CHECK(segment.min >= bar.Min.x - 1.0f);
        CHECK(segment.max <= bar.Max.x + 1.0f);
        if (first == 0.0f)
            first = segment.min;
        else if (std::fabs(segment.min - first) > 1.0f)
            moved = true;
    }
    CHECK(moved);
}

TEST_CASE("A disabled progress bar draws no tone color")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    DisplayGuard guard;
    const ImU32 tone = facet::ToU32(facet::T().colors.focusRing);
    Span fill;
    session.SetUi([&] {
        ImGui::BeginDisabled();
        const int from = ImGui::GetWindowDrawList()->VtxBuffer.Size;
        facet::ProgressBar("Disabled", 1.0f);
        fill = ColorSpan(from, tone);
        ImGui::EndDisabled();
    });
    session.Frame();
    CHECK(fill.Empty());
}

// ==== Truncation block ====

TEST_CASE("Start truncation keeps the end of a text and cuts it on a character boundary")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int checked = 0;
    session.SetUi([&] {
        ImFont* font = ImGui::GetFont();
        const float size = ImGui::GetFontSize();
        const float full = font->CalcTextSizeA(size, FLT_MAX, 0.0f, kUtf8Text).x;
        for (float width = 0.0f; width < full; width += 1.0f)
        {
            char out[128];
            for (const auto mode : {facet::internal::TruncateMode::Start, facet::internal::TruncateMode::Middle,
                                    facet::internal::TruncateMode::End})
            {
                const auto result = facet::internal::TruncateText(font, size, kUtf8Text, nullptr, width, mode, out, sizeof(out));
                CHECK(ValidUtf8(out));
                CHECK(result.truncated);
                CHECK(result.width <= width + 0.5f);
                if (mode == facet::internal::TruncateMode::Start && result.length > 0)
                {
                    // What follows the ellipsis (one code point) is the end of the text.
                    const std::string text(kUtf8Text);
                    const std::string shown(out);
                    std::size_t start = 1;
                    while (start < shown.size() && (static_cast<unsigned char>(shown[start]) & 0xC0) == 0x80)
                        ++start;
                    const std::string kept = shown.substr(start);
                    REQUIRE(text.size() >= kept.size());
                    CHECK(text.compare(text.size() - kept.size(), kept.size(), kept) == 0);
                }
            }
            ++checked;
        }
    });
    session.Frame();
    CHECK(checked > 10);
}

// ==== StyledText ====

TEST_CASE("Styled text takes its own width, wraps at the available width and aligns its lines")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float available = 0.0f;
    float ownWidth = 0.0f;
    float lineHeight = 0.0f;
    session.SetUi([&] {
        available = ImGui::GetContentRegionAvail().x;
        ownWidth = std::ceil(facet::MeasureText(facet::TextStyle::Headline, "Heading").x);
        lineHeight = facet::MeasureText(facet::TextStyle::Body, "").y;
        facet::StyledText("Heading", {.style = facet::TextStyle::Headline});
        session.Track("own");
        facet::StyledText("Two words that wrap into several lines at this narrow width",
                          {.wrap = true, .width = 80.0f});
        session.Track("wrapped");
        facet::StyledText("Short", {.wrap = true});
        session.Track("fill");
        facet::StyledText("First\nSecond");
        session.Track("breaks");
        facet::StyledText(nullptr);
        session.Track("null");
    });
    session.Frame();
    CHECK(session.ItemRect("own").GetWidth() == doctest::Approx(ownWidth));
    CHECK(session.ItemRect("wrapped").GetWidth() == doctest::Approx(facet::Px(80.0f)));
    CHECK(session.ItemRect("wrapped").GetHeight() >= lineHeight * 3.0f - 0.5f);
    CHECK(session.ItemRect("fill").GetWidth() == doctest::Approx(available));
    CHECK(session.ItemRect("breaks").GetHeight() == doctest::Approx(lineHeight * 2.0f));
    CHECK(session.ItemRect("null").GetHeight() == doctest::Approx(lineHeight));
}

TEST_CASE("Styled text draws its tone, the secondary label and the disabled color")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    DisplayGuard guard;
    const facet::Colors& c = facet::T().colors;
    int success = 0;
    int accent = 0;
    int secondary = 0;
    int disabled = 0;
    float endRight = 0.0f;
    float itemRight = 0.0f;
    session.SetUi([&] {
        int from = VertexCount();
        facet::StyledText("Saved", {.tone = facet::Tone::Success});
        success = CountColor(from, facet::ToU32(c.success));
        from = VertexCount();
        facet::StyledText("Link", {.tone = facet::Tone::Accent});
        accent = CountColor(from, facet::ToU32(c.accentText));
        from = VertexCount();
        facet::StyledText("Note", {.secondary = true});
        secondary = CountColor(from, facet::ToU32(c.labelSecondary));
        ImGui::BeginDisabled();
        from = VertexCount();
        facet::StyledText("Off", {.tone = facet::Tone::Destructive});
        disabled = CountColor(from, facet::ToU32(c.labelTertiary));
        ImGui::EndDisabled();
        from = VertexCount();
        facet::StyledText("End", {.align = facet::Align::End, .width = 200.0f});
        endRight = ColorSpan(from, facet::ToU32(c.label)).max;
        itemRight = ImGui::GetItemRectMax().x;
    });
    session.Frame();
    CHECK(success > 0);
    CHECK(accent > 0);
    CHECK(secondary > 0);
    CHECK(disabled > 0);
    CHECK(endRight == doctest::Approx(itemRight).epsilon(0.02));
}

// ==== TruncatedText ====

TEST_CASE("Truncated text shows its tooltip on hover only while it is cut")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool cutTip = false;
    bool wholeTip = false;
    session.SetUi([&] {
        facet::TruncatedText(kUtf8Text, {.width = 60.0f});
        session.Track("cut");
        cutTip = TooltipOpen(ImGui::GetItemID());
        facet::TruncatedText("Fits", {.width = 200.0f});
        session.Track("whole");
        wholeTip = TooltipOpen(ImGui::GetItemID());
    });
    session.Frame();
    session.MouseMove(session.ItemCenter("whole"));
    session.Seconds(facet::T().overlay.tooltipDelay + 0.2f);
    CHECK_FALSE(wholeTip);
    session.MouseMove(session.ItemCenter("cut"));
    session.Seconds(facet::T().overlay.tooltipDelay + 0.2f);
    CHECK(cutTip);
    CHECK(session.ItemRect("cut").GetWidth() == doctest::Approx(facet::Px(60.0f)));
}

TEST_CASE("Truncated text keeps a few pixels of width and every mode without trouble")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int calls = 0;
    session.SetUi([&] {
        for (const float width : {0.25f, 1.0f, 2.0f, 3.0f, 6.0f, 12.0f})
            for (const auto mode : {facet::TruncateMode::End, facet::TruncateMode::Middle, facet::TruncateMode::Start})
            {
                ImGui::PushID(calls);
                facet::TruncatedText(kUtf8Text, {.mode = mode, .maxLines = 3, .width = width});
                CHECK(ImGui::GetItemRectSize().x >= 1.0f);
                facet::TruncatedText("", {.mode = mode, .width = width});
                facet::TruncatedText(nullptr, {.mode = mode, .width = width});
                ImGui::PopID();
                ++calls;
            }
    });
    session.Frames(2);
    CHECK(calls == 36);
}

TEST_CASE("Truncated text wraps into at most maxLines lines in End mode")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float lineHeight = 0.0f;
    session.SetUi([&] {
        lineHeight = facet::MeasureText(facet::TextStyle::Body, "").y;
        facet::TruncatedText("One two three four five six seven eight nine ten eleven twelve",
                             {.maxLines = 2, .width = 80.0f});
        session.Track("two");
        facet::TruncatedText("Short", {.maxLines = 3, .width = 200.0f});
        session.Track("one");
        facet::TruncatedText("One two three four five six", {.mode = facet::TruncateMode::Middle, .maxLines = 3, .width = 40.0f});
        session.Track("middle");
    });
    session.Frame();
    CHECK(session.ItemRect("two").GetHeight() == doctest::Approx(lineHeight * 2.0f));
    CHECK(session.ItemRect("one").GetHeight() == doctest::Approx(lineHeight));
    CHECK(session.ItemRect("middle").GetHeight() == doctest::Approx(lineHeight));  // maxLines is End mode only
}

TEST_CASE("A focusable truncated text is a Tab stop and shows its whole text on keyboard focus")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool tip = false;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        facet::TruncatedText(kUtf8Text, {.focusable = true, .width = 60.0f});
        session.Track("text");
        tip = TooltipOpen(ImGui::GetItemID());
        ImGui::Button("after");
    });
    session.Frame();
    session.Click(session.ItemCenter("before"));
    session.MouseMove(ImVec2(1.0f, 590.0f));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("text"));
    session.Seconds(facet::T().overlay.tooltipDelay + 0.2f);
    CHECK(tip);
}

// ==== SectionHeader ====

TEST_CASE("A section header's action is the only Tab stop and activates by click, Enter and Space")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int presses = 0;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        facet::SectionHeader("Plain", {.description = "A description line under the heading.", .divider = true});
        session.Track("plain");
        if (facet::SectionHeader("With action", {.actionLabel = "Edit"}))
            ++presses;
        session.Track("action");
        ImGui::Button("after");
        session.Track("after");
    });
    session.Frame();
    CHECK(session.ItemRect("plain").GetHeight() > facet::MeasureText(facet::TextStyle::Title3, "").y);
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    // The plain header is passed over; the action of the second one takes the focus.
    CHECK(ImGui::GetFocusID() == session.ItemId("action"));
    session.KeyPress(ImGuiKey_Space);
    session.KeyPress(ImGuiKey_Enter);
    CHECK(presses == 2);
    const ImRect header = session.ItemRect("action");
    session.Click(ImVec2(header.Max.x - 4.0f, header.GetCenter().y));
    CHECK(presses == 3);
    session.Click(ImVec2(header.Min.x + 4.0f, header.GetCenter().y));  // the heading itself does nothing
    CHECK(presses == 3);
}

// ==== Badge ====

TEST_CASE("A badge draws its tone as a wash with label text, or solid with the text on the tone")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    DisplayGuard guard;
    const facet::Colors& c = facet::T().colors;
    int wash = 0;
    int washText = 0;
    int solid = 0;
    int onSolid = 0;
    int glyph = 0;
    session.SetUi([&] {
        int from = VertexCount();
        facet::Badge("Synced", {.tone = facet::Tone::Success, .icon = facet::Icon("check")});
        wash = CountColor(from, facet::ToU32(c.successSubtle));
        washText = CountColor(from, facet::ToU32(c.label));
        glyph = CountColor(from, facet::ToU32(c.success));
        from = VertexCount();
        facet::Badge("Failed", {.tone = facet::Tone::Destructive, .solid = true});
        solid = CountColor(from, facet::ToU32(c.destructive));
        onSolid = CountColor(from, facet::ToU32(c.onDestructive));
    });
    session.Frame();
    CHECK(wash > 0);
    CHECK(washText > 0);
    CHECK(glyph > 0);
    CHECK(solid > 0);
    CHECK(onSolid > 0);
}

// ==== CountBadge ====

TEST_CASE("A count badge caps large counts, hides zero unless asked and is never narrower than tall")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    session.SetUi([&] {
        facet::CountBadge(7);
        session.Track("one");
        facet::CountBadge(150);
        session.Track("capped");
        facet::CountBadge(99);
        session.Track("ninety");
        facet::CountBadge(0);
        session.Track("hidden");
        facet::CountBadge(0, {.showZero = true});
        session.Track("zero");
        facet::CountBadge(-4, {.showZero = true});
        session.Track("negative");
        facet::CountBadge(1000, {.maxCount = 999});
        session.Track("custom");
    });
    session.Frame();
    const ImRect one = session.ItemRect("one");
    CHECK(one.GetWidth() >= one.GetHeight());
    CHECK(session.ItemRect("capped").GetWidth() > session.ItemRect("ninety").GetWidth());  // "99+"
    CHECK(session.ItemRect("hidden").GetWidth() == 0.0f);
    CHECK(session.ItemRect("zero").GetWidth() == doctest::Approx(one.GetWidth()));
    CHECK(session.ItemRect("negative").GetWidth() == doctest::Approx(one.GetWidth()));  // drawn as 0
    CHECK(session.ItemRect("custom").GetWidth() > session.ItemRect("capped").GetWidth());
}

// ==== StatusDot ====

TEST_CASE("A pulsing status dot stands still while motion is reduced")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    DisplayGuard guard;
    const facet::Colors& c = facet::T().colors;
    int ring = 0;
    session.SetUi([&] {
        const int from = VertexCount();
        facet::StatusDot("Live", {.tone = facet::Tone::Success, .pulse = true});
        // The ring is the tone at a lower alpha; the dot itself is opaque.
        ring = CountColor(from, facet::ToU32(c.success), true) - CountColor(from, facet::ToU32(c.success));
        session.Track("dot");
    });
    session.Seconds(facet::T().motion.pulsePeriod * 0.3f);
    CHECK(ring > 0);
    DisplayGuard::ReduceMotion(true);
    session.Seconds(facet::T().motion.pulsePeriod * 0.3f);
    CHECK(ring == 0);
    // A dot alone names itself in a tooltip on hover.
    bool tip = false;
    session.SetUi([&] {
        facet::StatusDot("##Connected");
        session.Track("alone");
        tip = TooltipOpen(ImGui::GetItemID());
    });
    session.Frame();
    session.MouseMove(session.ItemCenter("alone"));
    session.Seconds(facet::T().overlay.tooltipDelay + 0.2f);
    CHECK(tip);
}

// ==== StatusPill ====

TEST_CASE("A status pill crossfades to a new tone over the fast duration")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    DisplayGuard guard;
    const facet::Colors& c = facet::T().colors;
    facet::Tone tone = facet::Tone::Success;
    int target = 0;
    session.SetUi([&] {
        const int from = VertexCount();
        facet::StatusPill("Status###pill", {.tone = tone, .icon = facet::Icon("check")});
        target = CountColor(from, facet::ToU32(c.warningSubtle));
    });
    session.Frames(2);
    CHECK(target == 0);
    tone = facet::Tone::Warning;
    session.Frame();
    CHECK(target == 0);  // on its way
    session.Seconds(facet::T().motion.durationFast * 4.0f);
    CHECK(target > 0);
    // With reduced motion the change is at once.
    DisplayGuard::ReduceMotion(true);
    tone = facet::Tone::Success;
    session.Frame();
    tone = facet::Tone::Warning;
    session.Frame();
    CHECK(target > 0);
}

// ==== Contrast ====

TEST_CASE("Display tones keep AA contrast on every content surface in both themes")
{
    for (const facet::Theme& theme : {facet::Theme::Light(), facet::Theme::Dark()})
    {
        CAPTURE(theme.dark);
        DisplayGuard guard;
        facet::SetTheme(theme, false);
        const facet::Colors& c = facet::T().colors;
        const ImVec4 surfaces[] = {c.bg, c.bgSecondary, c.bgTertiary};
        for (const facet::Tone tone : {facet::Tone::Neutral, facet::Tone::Accent, facet::Tone::Success,
                                       facet::Tone::Warning, facet::Tone::Destructive})
        {
            CAPTURE(static_cast<int>(tone));
            const facet::internal::DisplayToneColors look = facet::internal::DisplayToneLook(tone, false);
            // Text on a solid mark.
            for (const ImVec4& surface : surfaces)
            {
                const ImVec4 solid = facet::Over(look.solid, surface);
                CHECK(facet::ContrastRatio(look.onSolid, solid) >= 4.5f);
                // Tone text on the surface; label on the wash; the tone glyph on the wash and the
                // surface (3:1 for graphics).
                CHECK(facet::ContrastRatio(look.text, surface) >= 4.5f);
                const ImVec4 wash = facet::Over(look.wash, surface);
                CHECK(facet::ContrastRatio(c.label, wash) >= 4.5f);
                CHECK(facet::ContrastRatio(c.labelSecondary, wash) >= 4.5f);
                CHECK(facet::ContrastRatio(look.glyph, wash) >= 3.0f);
                CHECK(facet::ContrastRatio(look.glyph, surface) >= 3.0f);
                // Log icons on a selected or hovered row.
                for (const ImVec4& row : {c.accentSubtle, c.fill[1], c.fill[2]})
                    CHECK(facet::ContrastRatio(look.glyph, facet::Over(row, surface)) >= 3.0f);
                // Progress marks.
                CHECK(facet::ContrastRatio(facet::internal::DisplayProgressColor(tone), surface) >= 3.0f);
            }
        }
        // Key caps and inline code: label on the cap fill; the cap boundary at 3:1.
        for (const ImVec4& surface : surfaces)
        {
            CHECK(facet::ContrastRatio(c.label, facet::Over(c.fill[2], surface)) >= 4.5f);
            CHECK(facet::ContrastRatio(facet::ControlBoundaryColor(c), surface) >= 3.0f);
        }
        // Code view and stat tile text on their bgSecondary surface.
        for (const ImVec4& tint : {c.codeKeyword, c.codeString, c.codeNumber, c.labelSecondary, c.label})
            CHECK(facet::ContrastRatio(tint, c.bgSecondary) >= 4.5f);
        CHECK(facet::ContrastRatio(c.label, facet::Over(c.accentSubtle, c.bgSecondary)) >= 4.5f);
    }
}
