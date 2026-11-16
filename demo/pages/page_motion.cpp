// Motion page: exponential smoothing, a spring and a tween side by side on the same target,
// a replay button, the reduce motion switch, and the curves of each motion sampled at two
// frame rates to show that they do not depend on it.

#include <algorithm>
#include <cstdio>
#include <iterator>

#include <imgui.h>

#include <facet/facet.h>

#include "core/anim_store.h"
#include "gallery.h"
#include "pages/pages.h"

namespace
{

using facet::demo::Note;
using facet::demo::Paragraph;
using facet::demo::SectionHeader;

constexpr int kKinds = 3;
constexpr const char* kKindNames[kKinds] = {"Smooth", "Spring", "Tween"};

// Exponential smoothing reaches 95 % of the way after ln(20) / speed seconds; the speed is
// chosen so that this takes durationStandard.
constexpr float kSmoothSettle = 2.9957f;

// The curves cover this many seconds, sampled at 240 and at 30 frames per second.
constexpr float kCurveSeconds = 0.8f;
constexpr int kFineRate = 240;
constexpr int kCoarseRate = 30;
constexpr int kFineSamples = static_cast<int>(kCurveSeconds * kFineRate) + 1;
constexpr int kCoarseSamples = static_cast<int>(kCurveSeconds * kCoarseRate) + 1;

// The frame time of the "midflight" capture and how many frames after Replay it shows.
constexpr float kCaptureStep = 1.0f / 60.0f;
constexpr int kMidflightFrames = 9;

struct MotionPageState
{
    bool forward = false;     // target 1 when true, 0 otherwise
    float values[kKinds] = {};
};

MotionPageState motion;

ImU32 U32(const ImVec4& color)
{
    return facet::ToU32(color);
}

float SmoothSpeed(const facet::Theme& theme)
{
    return kSmoothSettle / std::max(theme.motion.durationStandard, 1e-3f);
}

// One motion of the given kind queried on a store.
float Query(facet::internal::AnimStore& store, int kind, float target)
{
    const facet::Theme& theme = facet::T();
    switch (kind)
    {
    case 0:
        return store.Smooth(1, 0, target, SmoothSpeed(theme));
    case 1:
        return store.Spring(1, 1, target, theme.motion.standard);
    default:
        return store.Tween(1, 2, target, theme.motion.durationSlow, facet::Ease::OutCubic);
    }
}

// Samples a motion from 0 toward 1 at `rate` frames per second into `out`.
void SampleCurve(int kind, int rate, ImVec2* out, int count)
{
    facet::internal::AnimStore store;
    facet::internal::AnimStore::Settings settings;
    settings.motionScale = facet::T().motion.scale;
    store.SetSettings(settings);
    Query(store, kind, 0.0f);
    out[0] = ImVec2(0.0f, 0.0f);
    const float dt = 1.0f / static_cast<float>(rate);
    for (int i = 1; i < count; ++i)
    {
        store.BeginFrame(dt);
        out[i] = ImVec2(static_cast<float>(i) * dt, Query(store, kind, 1.0f));
    }
}

void DrawControls(facet::demo::PageCtx& ctx)
{
    const float right = facet::demo::ContentRight();
    const float gap = facet::Px(facet::T().spacing.xl);
    // A fixed id: the label changes, the item (and its keyboard focus) stays.
    if (ImGui::Button(motion.forward ? "Replay backward###replay" : "Replay forward###replay"))
        motion.forward = !motion.forward;
    facet::ItemCursor();
    ctx.Probe("motion.replay");

    facet::demo::SameLineIfFits(right, facet::demo::CheckboxWidth("Reduce motion"), gap);
    bool reduce = facet::T().motion.reduceMotion;
    if (ImGui::Checkbox("Reduce motion", &reduce))
    {
        facet::Theme theme = facet::T();
        theme.motion.reduceMotion = reduce;
        facet::SetTheme(theme, false);
    }
    facet::ItemCursor();
    ctx.Probe("motion.reduce");

    facet::demo::SameLineIfFits(right, facet::demo::CheckboxWidth("Slow down 4x"), gap);
    bool slow = facet::T().motion.scale > 1.0f;
    if (ImGui::Checkbox("Slow down 4x", &slow))
    {
        facet::Theme theme = facet::T();
        theme.motion.scale = slow ? 4.0f : 1.0f;
        facet::SetTheme(theme, false);
    }
    facet::ItemCursor();
    ctx.Probe("motion.slow");

    Note("Windows animation effects are %s; Facet %s that setting here.",
         facet::SystemReduceMotion() ? "off" : "on",
         facet::FollowsSystemMotion() ? "follows" : "ignores (capture and selftest runs)");
}

// The three tracks: a rail with a knob at the current value.
void DrawTracks(const float* values)
{
    const facet::Theme& theme = facet::T();
    const facet::Colors& c = theme.colors;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const float labelWidth = facet::Px(theme.spacing.xxxl * 2.5f);
    const float knob = facet::Px(theme.metrics.sliderKnobSize);
    const float rail = facet::Px(theme.metrics.sliderTrackHeight);
    const float rowHeight = facet::Px(theme.metrics.controlHeightLarge);
    for (int kind = 0; kind < kKinds; ++kind)
    {
        const ImVec2 row = ImGui::GetCursorScreenPos();
        const float width = ImGui::GetContentRegionAvail().x;
        ImGui::Dummy(ImVec2(width, rowHeight));
        const float centerY = row.y + rowHeight * 0.5f;
        facet::PushTextStyle(facet::TextStyle::Headline);
        drawList->AddText(ImVec2(row.x, centerY - ImGui::GetFontSize() * 0.5f), U32(c.label), kKindNames[kind]);
        facet::PopTextStyle();

        const float left = row.x + labelWidth + knob * 0.5f;
        const float right = row.x + width - labelWidth - knob * 0.5f;
        facet::FillRect(drawList, ImVec2(left, centerY - rail * 0.5f), ImVec2(right, centerY + rail * 0.5f),
                        U32(c.fill[1]), rail * 0.5f);
        const float x = left + (right - left) * values[kind];
        drawList->AddCircleFilled(ImVec2(x, centerY), knob * 0.5f, U32(c.accent));

        char text[16];
        std::snprintf(text, sizeof(text), "%.3f", static_cast<double>(values[kind]));
        facet::PushTextStyle(facet::TextStyle::Footnote);
        drawList->AddText(ImVec2(right + knob, centerY - ImGui::GetFontSize() * 0.5f), U32(c.labelSecondary), text);
        facet::PopTextStyle();
    }
}

// Sampled curves, recomputed only when the motion tokens they depend on change, so drawing the
// page does not allocate the stores every frame.
struct CurveCache
{
    bool valid = false;
    float key[5] = {};
    ImVec2 fine[kKinds][kFineSamples];
    ImVec2 coarse[kKinds][kCoarseSamples];
};

CurveCache curves;

void UpdateCurves()
{
    const facet::Motion& m = facet::T().motion;
    const float key[5] = {m.scale, m.standard.response, m.standard.dampingRatio, m.durationStandard, m.durationSlow};
    if (curves.valid && std::equal(std::begin(key), std::end(key), std::begin(curves.key)))
        return;
    std::copy(std::begin(key), std::end(key), std::begin(curves.key));
    for (int kind = 0; kind < kKinds; ++kind)
    {
        SampleCurve(kind, kFineRate, curves.fine[kind], kFineSamples);
        SampleCurve(kind, kCoarseRate, curves.coarse[kind], kCoarseSamples);
    }
    curves.valid = true;
}

// One plot per motion: the 240 fps curve as a line and the 30 fps samples as dots.
void DrawCurves()
{
    const facet::Theme& theme = facet::T();
    const facet::Colors& c = theme.colors;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImGuiStyle& style = ImGui::GetStyle();
    const float gap = style.ItemSpacing.x * 2.0f;
    const float avail = ImGui::GetContentRegionAvail().x;
    const float width = std::min(avail, std::max(facet::Px(theme.spacing.xxxl * 3.0f), (avail - gap * (kKinds - 1)) / kKinds));
    const float height = facet::Px(theme.spacing.xxxl * 3.0f);
    const float inset = facet::Px(theme.spacing.md);
    const float dot = facet::Px(theme.spacing.xs) * 0.5f;
    UpdateCurves();
    const float right = facet::demo::ContentRight();
    for (int kind = 0; kind < kKinds; ++kind)
    {
        const ImVec2* fine = curves.fine[kind];
        const ImVec2* coarse = curves.coarse[kind];
        ImGui::BeginGroup();
        facet::Text(facet::TextStyle::Headline, "%s", kKindNames[kind]);
        ImGui::Dummy(ImVec2(width, height));
        const ImVec2 a = ImGui::GetItemRectMin();
        const ImVec2 b = ImGui::GetItemRectMax();
        facet::FillRect(drawList, a, b, U32(c.bgSecondary), facet::Px(theme.radii.card));
        // Value 0 at the bottom, 1 (the target) as a hairline near the top; values above 1
        // (spring overshoot) still fit.
        const float plotLeft = a.x + inset;
        const float plotRight = b.x - inset;
        const float zeroY = b.y - inset;
        const float oneY = a.y + inset * 2.0f;
        facet::HairlineH(drawList, plotLeft, plotRight, oneY, U32(c.separator));
        facet::HairlineH(drawList, plotLeft, plotRight, zeroY, U32(c.separator));
        const auto map = [&](ImVec2 sample) {
            return ImVec2(plotLeft + (plotRight - plotLeft) * sample.x / kCurveSeconds,
                          zeroY + (oneY - zeroY) * sample.y);
        };
        ImVec2 points[kFineSamples];
        for (int i = 0; i < kFineSamples; ++i)
            points[i] = map(fine[i]);
        drawList->AddPolyline(points, kFineSamples, U32(c.accent), facet::Px(theme.metrics.hairline) * 2.0f);
        for (int i = 0; i < kCoarseSamples; ++i)
            drawList->AddCircleFilled(map(coarse[i]), dot * 2.0f, U32(c.label));
        Note("%.1f s; line 240 fps, dots 30 fps", static_cast<double>(kCurveSeconds));
        ImGui::EndGroup();
        if (kind + 1 < kKinds)
            facet::demo::SameLineIfFits(right, width, gap);
    }
}

} // namespace

FACET_DEMO_PAGE("motion", "Motion", "Foundations", 2, "default", "midflight", "reduced")
{
    const facet::Theme& theme = facet::T();
    // Capture state "reduced" turns reduce motion on; the other states keep the preset.
    if (ctx.capture && theme.motion.reduceMotion != (ctx.captureState == 2))
    {
        facet::Theme changed = theme;
        changed.motion.reduceMotion = ctx.captureState == 2;
        facet::SetTheme(changed, false);
    }

    facet::demo::PageHeader("Motion", "Three ways to move a value to a new target. Each one is solved for the "
                                      "elapsed time, so the motion looks the same at any frame rate. Values snap "
                                      "onto the target once the rest is too small to see.");

    SectionHeader("Side by side");
    DrawControls(ctx);
    ImGui::Dummy(ImVec2(0.0f, facet::Px(facet::T().spacing.sm)));

    const ImGuiID id = ImGui::GetID("##tracks");
    const float target = motion.forward ? 1.0f : 0.0f;
    motion.values[0] = facet::Smooth(id, 0, target, SmoothSpeed(facet::T()));
    motion.values[1] = facet::Spring(id, 1, target, facet::T().motion.standard);
    motion.values[2] = facet::Tween(id, 2, target, facet::T().motion.durationSlow, facet::Ease::OutCubic);

    if (ctx.capture && ctx.captureState == 1)
    {
        // A fixed moment of the forward motion, computed on a store of its own so the image
        // does not depend on when the capture starts.
        facet::internal::AnimStore store;
        float frozen[kKinds] = {};
        for (int kind = 0; kind < kKinds; ++kind)
            Query(store, kind, 0.0f);
        for (int frame = 0; frame < kMidflightFrames; ++frame)
        {
            store.BeginFrame(kCaptureStep);
            for (int kind = 0; kind < kKinds; ++kind)
                frozen[kind] = Query(store, kind, 1.0f);
        }
        DrawTracks(frozen);
        Note("Shown %.0f ms after Replay.", static_cast<double>(kCaptureStep * kMidflightFrames * 1000.0f));
    }
    else
    {
        DrawTracks(motion.values);
    }
    if (facet::ReduceMotion())
        Paragraph("Reduce motion is on: every value jumps to its target.");
    Note("Smooth: speed %.1f. Spring: response %.2f s, damping %.2f. Tween: %.0f ms, out cubic.",
         static_cast<double>(SmoothSpeed(facet::T())), static_cast<double>(facet::T().motion.standard.response),
         static_cast<double>(facet::T().motion.standard.dampingRatio),
         static_cast<double>(facet::T().motion.durationSlow * 1000.0f));

    SectionHeader("Curves");
    Paragraph("From 0 to the target 1. Both frame rates land on the same curve.");
    DrawCurves();
}

FACET_DEMO_SELFTEST("motion")
{
    test.Do([] {
        motion = MotionPageState{};
        facet::Theme theme = facet::T();
        theme.motion.reduceMotion = false;
        theme.motion.scale = 1.0f;
        facet::SetTheme(theme, false);
    });
    test.Wait(10);

    test.Click("motion.replay");
    test.Expect("after Replay the spring and the tween are on their way",
                [] { return motion.values[1] > 0.0f && motion.values[1] < 1.0f && motion.values[2] > 0.0f &&
                            motion.values[2] < 1.0f; });
    test.Wait(90);
    test.Expect("after 1.5 s every motion rests exactly on the target",
                [] { return motion.values[0] == 1.0f && motion.values[1] == 1.0f && motion.values[2] == 1.0f; });

    test.Click("motion.reduce");
    test.Expect("the switch turns reduce motion on", [] { return facet::ReduceMotion(); });
    // Shift+Tab moves at once, also right after a click.
    test.KeyPress(ImGuiKey_Tab | ImGuiMod_Shift);
    test.ExpectFocused("motion.replay");
    test.KeyPress(ImGuiKey_Space);
    test.Expect("with reduced motion Replay jumps straight to the target",
                [] { return motion.values[0] == 0.0f && motion.values[1] == 0.0f && motion.values[2] == 0.0f; });

    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("motion.reduce");
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Space turns reduce motion off again", [] { return !facet::ReduceMotion(); });

    test.Click("motion.slow");
    test.Expect("Slow down sets the motion scale to 4", [] { return facet::T().motion.scale == 4.0f; });
    // A light or dark switch changes the colors and keeps the motion settings.
    static facet::ThemeMode savedMode = facet::ThemeMode::Light;
    static bool savedDark = false;
    test.Do([] {
        savedMode = facet::GetThemeMode();
        savedDark = facet::T().dark;
        facet::SetThemeMode(savedDark ? facet::ThemeMode::Light : facet::ThemeMode::Dark, false);
    });
    test.Expect("a theme switch keeps the slowed motion",
                [] { return facet::T().dark != savedDark && facet::T().motion.scale == 4.0f; });
    test.Do([] { facet::SetThemeMode(savedMode, false); });
    test.Click("motion.replay");
    test.Wait(30);
    test.Expect("at scale 4 the tween is still moving after 0.5 s",
                [] { return motion.values[2] < 1.0f && motion.values[2] > 0.0f; });
    test.Click("motion.slow");
    test.Expect("Slow down off restores the scale", [] { return facet::T().motion.scale == 1.0f; });
}
