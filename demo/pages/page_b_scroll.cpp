// Demo page: Scroll. Prototype scroll areas on the scroll core (src/internal/scroll.h): a long
// page with edge fades and focusable prototype buttons, nested areas that hand the wheel over,
// an area that scrolls on both axes, the edge fades on every surface token and an area whose
// content fits. The lines inside the areas are generated filler, labelled as such. The capture
// states show the overlay bar hovered and dragged, and every area scrolled into its middle so
// both fades show.

#include <cstdio>

#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "page_helpers.h"
#include "internal/item.h"
#include "internal/row_layout.h"
#include "internal/scroll.h"
#include "pages/pages.h"

namespace
{

namespace fi = facet::internal;
using facet::demo::ForcedItemState;
using facet::demo::Note;
using facet::demo::SectionHeader;

enum CaptureState
{
    kDefault,
    kHovered,
    kDragging,
    kScrolled,
};

constexpr int kLongLines = 48;
constexpr int kButtonEvery = 8;
constexpr int kInnerLines = 14;
constexpr int kGridColumns = 24;
constexpr int kGridRows = 30;
constexpr int kSurfaceLines = 12;

struct ScrollPageState
{
    ImGuiID longWindow = 0;
    ImGuiID outerWindow = 0;
    ImGuiID innerWindow = 0;
    ImGuiID wideWindow = 0;
    float longScroll = 0.0f;
    float longMax = 0.0f;
    float outerScroll = 0.0f;
    float innerScroll = 0.0f;
    float innerMax = 0.0f;
    float wideX = 0.0f;
    float wideY = 0.0f;
    bool longScrolling = false;
    int pressed = 0;     // the last prototype button pressed, 0 for none
    bool reset = false;  // scroll every area back to its start in the next frame
    bool reduce = false;
};

ScrollPageState page;

ImGuiWindow* WindowOf(ImGuiID id)
{
    return id != 0 ? ImGui::FindWindowByID(id) : nullptr;
}

// A card of `surface` at the cursor, `height` pixels high and as wide as the content; returns
// where the area inside it goes (inset, so the edge fades stay inside the rounded corners).
ImRect Card(float height, const ImVec4& surface)
{
    const facet::Theme& t = facet::T();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    facet::FillRect(ImGui::GetWindowDrawList(), pos, ImVec2(pos.x + width, pos.y + height), facet::ToU32(surface),
                    facet::Px(t.radii.card));
    const float inset = facet::Px(t.spacing.sm);
    return ImRect(pos.x + inset, pos.y + inset, pos.x + width - inset, pos.y + height - inset);
}

// Places the next area on `inner` (from Card) and leaves the cursor below the card afterwards.
void PlaceIn(const ImRect& inner)
{
    ImGui::SetCursorScreenPos(inner.Min);
}

void EndCard(const ImRect& inner)
{
    const float inset = facet::Px(facet::T().spacing.sm);
    ImGui::SetCursorScreenPos(ImVec2(inner.Min.x - inset, inner.Max.y + inset));
    ImGui::Dummy(ImVec2(0.0f, 0.0f));
}

// Probes for the bar of an area, from the geometry of the latest frame.
void ProbeBar(facet::demo::PageCtx& ctx, ImGuiWindow* window, const char* name)
{
    ImRect track;
    ImRect thumb;
    if (window == nullptr || !fi::ScrollAreaBarRects(window, ImGuiAxis_Y, &track, &thumb))
        return;
    char probe[64];
    std::snprintf(probe, sizeof(probe), "%s.track", name);
    facet::demo::ProbeRect(ctx, probe, window->GetID("##facet.vbar"), track.Min, track.Max);
    std::snprintf(probe, sizeof(probe), "%s.thumb", name);
    facet::demo::ProbeRect(ctx, probe, window->GetID("##facet.vbar"), thumb.Min, thumb.Max);
}

// In "scrolled" captures every area sits in the middle of its range.
void ScrollToMiddle(facet::demo::PageCtx& ctx, bool horizontal = false)
{
    if (!ctx.capture || ctx.captureState != kScrolled)
        return;
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    fi::ScrollAreaTo(window, ImVec2(horizontal ? window->ScrollMax.x * 0.5f : -1.0f, window->ScrollMax.y * 0.5f), false);
}

void ResetIfAsked()
{
    if (page.reset)
        fi::ScrollAreaTo(ImGui::GetCurrentWindow(), ImVec2(0.0f, 0.0f), false);
}

void GeneratedLines(int count, int first = 1)
{
    for (int i = 0; i < count; ++i)
        facet::Text(facet::TextStyle::Body, "Generated line %d of %d", first + i, count);
}

void LongPage(facet::demo::PageCtx& ctx)
{
    const facet::Theme& t = facet::T();
    const ImRect inner = Card(facet::Px(t.metrics.panelHeight * 1.25f), t.colors.bgSecondary);
    PlaceIn(inner);
    if (ctx.capture)
    {
        if (ctx.captureState == kHovered)
            ctx.ForceNextItemState(ForcedItemState::Hovered, 0);
        else if (ctx.captureState == kDragging)
            ctx.ForceNextItemState(ForcedItemState::Dragging, 0);
    }
    fi::PushScrollFadeSurface(t.colors.bgSecondary);
    if (fi::BeginScrollAreaCore("long", {.size = inner.GetSize()}))
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        page.longWindow = window->ID;
        ResetIfAsked();
        ScrollToMiddle(ctx);
        char label[64];
        char probe[32];
        for (int i = 1; i <= kLongLines; ++i)
        {
            if (i % kButtonEvery == 1)
            {
                const int button = i / kButtonEvery + 1;
                std::snprintf(label, sizeof(label), "Prototype button %d", button);
                std::snprintf(probe, sizeof(probe), "scroll.button%d", button);
                if (facet::demo::ProtoButton(ctx, label, probe))
                    page.pressed = button;
            }
            facet::Text(facet::TextStyle::Body, "Generated line %d of %d", i, kLongLines);
        }
        page.longScroll = window->Scroll.y;
        page.longMax = window->ScrollMax.y;
        page.longScrolling = fi::IsScrollAreaScrolling(window);
        fi::EndScrollAreaCore();
    }
    ctx.Probe("scroll.long");
    fi::PopScrollFadeSurface();
    ProbeBar(ctx, WindowOf(page.longWindow), "scroll.long");
    EndCard(inner);
}

void Nested(facet::demo::PageCtx& ctx)
{
    const facet::Theme& t = facet::T();
    const ImRect outerRect = Card(facet::Px(t.metrics.panelHeight * 1.25f), t.colors.bgSecondary);
    PlaceIn(outerRect);
    fi::PushScrollFadeSurface(t.colors.bgSecondary);
    if (fi::BeginScrollAreaCore("outer", {.size = outerRect.GetSize()}))
    {
        ImGuiWindow* outer = ImGui::GetCurrentWindow();
        page.outerWindow = outer->ID;
        ResetIfAsked();
        ScrollToMiddle(ctx);
        Note("The outer area. The inner area below takes the wheel while it can scroll in that direction; at its end "
             "the rest of the gesture scrolls this one.");
        const ImRect innerRect = Card(facet::Px(t.metrics.panelHeight * 0.6f), t.colors.bgTertiary);
        PlaceIn(innerRect);
        fi::PushScrollFadeSurface(t.colors.bgTertiary);
        if (fi::BeginScrollAreaCore("inner", {.size = innerRect.GetSize()}))
        {
            ImGuiWindow* inner = ImGui::GetCurrentWindow();
            page.innerWindow = inner->ID;
            ResetIfAsked();
            ScrollToMiddle(ctx);
            GeneratedLines(kInnerLines);
            page.innerScroll = inner->Scroll.y;
            page.innerMax = inner->ScrollMax.y;
            fi::EndScrollAreaCore();
        }
        ctx.Probe("scroll.inner");
        fi::PopScrollFadeSurface();
        EndCard(innerRect);
        GeneratedLines(kInnerLines);
        page.outerScroll = outer->Scroll.y;
        fi::EndScrollAreaCore();
    }
    ctx.Probe("scroll.outer");
    fi::PopScrollFadeSurface();
    EndCard(outerRect);
}

void BothAxes(facet::demo::PageCtx& ctx)
{
    const facet::Theme& t = facet::T();
    const ImRect inner = Card(facet::Px(t.metrics.panelHeight), t.colors.bgSecondary);
    PlaceIn(inner);
    fi::PushScrollFadeSurface(t.colors.bgSecondary);
    if (fi::BeginScrollAreaCore("wide", {.size = inner.GetSize(), .horizontal = true}))
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        page.wideWindow = window->ID;
        ResetIfAsked();
        ScrollToMiddle(ctx, true);
        const float cell = facet::Px(t.spacing.xxxl * 2.0f);
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        char text[16];
        for (int row = 0; row < kGridRows; ++row)
        {
            for (int column = 0; column < kGridColumns; ++column)
            {
                std::snprintf(text, sizeof(text), "%d, %d", row + 1, column + 1);
                const float line = ImGui::GetTextLineHeightWithSpacing();
                ImGui::SetCursorScreenPos(ImVec2(origin.x + cell * static_cast<float>(column),
                                                 origin.y + line * static_cast<float>(row)));
                ImGui::TextUnformatted(text);
            }
        }
        page.wideX = window->Scroll.x;
        page.wideY = window->Scroll.y;
        fi::EndScrollAreaCore();
    }
    ctx.Probe("scroll.wide");
    fi::PopScrollFadeSurface();
    EndCard(inner);
}

// One small area on each surface token; the fades must disappear into every one.
void Surfaces(facet::demo::PageCtx& ctx)
{
    const facet::Theme& t = facet::T();
    struct Surface
    {
        const char* name;
        ImVec4 color;
    };
    const Surface surfaces[] = {
        {"bg", t.colors.bg},
        {"bgSecondary", t.colors.bgSecondary},
        {"bgTertiary", t.colors.bgTertiary},
        {"bgElevated", t.colors.bgElevated},
    };
    const float gap = facet::Px(t.spacing.md);
    const float available = ImGui::GetContentRegionAvail().x;
    const bool stack = available < facet::Px(t.layout.expandedFrom);
    const float width = stack ? available : (available - gap * 3.0f) / 4.0f;
    const float height = facet::Px(t.metrics.chartHeight);
    for (int i = 0; i < 4; ++i)
    {
        if (i > 0 && !stack)
            ImGui::SameLine(0.0f, gap);
        ImGui::BeginGroup();
        facet::Text(facet::TextStyle::Footnote, "%s", surfaces[i].name);
        ImGui::PushID(i);
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        facet::FillRect(ImGui::GetWindowDrawList(), pos, ImVec2(pos.x + width, pos.y + height),
                        facet::ToU32(surfaces[i].color), facet::Px(t.radii.card));
        facet::StrokeRect(ImGui::GetWindowDrawList(), pos, ImVec2(pos.x + width, pos.y + height),
                          facet::ToU32(t.colors.separator), facet::Px(t.radii.card), facet::HairlineThickness());
        const float inset = facet::Px(t.spacing.sm);
        ImGui::SetCursorScreenPos(ImVec2(pos.x + inset, pos.y + inset));
        fi::PushScrollFadeSurface(surfaces[i].color);
        if (fi::BeginScrollAreaCore("surface", {.size = ImVec2(width - inset * 2.0f, height - inset * 2.0f)}))
        {
            ScrollToMiddle(ctx);
            ResetIfAsked();
            GeneratedLines(kSurfaceLines);
            fi::EndScrollAreaCore();
        }
        fi::PopScrollFadeSurface();
        ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + height));
        ImGui::Dummy(ImVec2(width, 0.0f));
        ImGui::PopID();
        ImGui::EndGroup();
    }
}

void Fits(facet::demo::PageCtx&)
{
    const facet::Theme& t = facet::T();
    const ImRect inner = Card(facet::Px(t.metrics.chartHeight * 0.6f), t.colors.bgSecondary);
    PlaceIn(inner);
    fi::PushScrollFadeSurface(t.colors.bgSecondary);
    if (fi::BeginScrollAreaCore("fits", {.size = inner.GetSize()}))
    {
        GeneratedLines(2);
        fi::EndScrollAreaCore();
    }
    fi::PopScrollFadeSurface();
    EndCard(inner);
}

} // namespace

FACET_DEMO_PAGE("scroll", "Scroll", "Blocks", 120, "default", "bar hovered", "thumb dragging", "scrolled")
{
    facet::demo::PageHeader("Scroll",
                            "Scroll areas with an overlay bar that is dimmed until the area is scrolled or hovered, "
                            "smooth wheel scrolling that hands over between nested areas, and edge fades that show "
                            "where content continues.");
    Note("Prototype areas built on the scroll core block; the lines inside them are generated filler. The wheel moves a "
         "target that the visible position follows; dragging the thumb, pressing the track or moving the keyboard "
         "focus takes over at once. Content that scrolls ends before the bar.");

    SectionHeader("Long page with edge fades");
    LongPage(ctx);
    if (ctx.capture && (ctx.captureState == kHovered || ctx.captureState == kDragging))
        ctx.RevealInCapture(0.5f);
    Note("Offset %.0f of %.0f px%s. Tab reaches the prototype buttons inside; the area follows the focus. %s",
         static_cast<double>(page.longScroll), static_cast<double>(page.longMax), page.longScrolling ? ", moving" : "",
         page.pressed > 0 ? "A prototype button was pressed." : "");
    if (page.pressed > 0)
        Note("Last pressed: Prototype button %d.", page.pressed);

    SectionHeader("Nested areas");
    Nested(ctx);
    Note("Inner offset %.0f of %.0f px, outer offset %.0f px.", static_cast<double>(page.innerScroll),
         static_cast<double>(page.innerMax), static_cast<double>(page.outerScroll));

    SectionHeader("Both axes");
    BothAxes(ctx);
    Note("Shift turns the wheel sideways. Offsets %.0f, %.0f px.", static_cast<double>(page.wideX),
         static_cast<double>(page.wideY));

    SectionHeader("Edge fades on every surface");
    Surfaces(ctx);
    if (ctx.capture && ctx.captureState == kScrolled)
        ctx.RevealInCapture(0.5f);
    Note("Each fade blends into the surface under its area, so it reads as content running out of sight on every "
         "surface token in both themes.");

    SectionHeader("Content that fits");
    Fits(ctx);
    Note("No bar, no gutter and no fade when there is nothing more to see; the wheel passes to the page.");

    SectionHeader("Motion");
    fi::BeginRow("Reduce motion", {.description = "Wheel scrolls then jump to their target."});
    page.reduce = facet::T().motion.reduceMotion;
    if (facet::demo::ProtoToggle(ctx, "##value", &page.reduce, "scroll.reduce"))
    {
        facet::Theme theme = facet::T();
        theme.motion.reduceMotion = page.reduce;
        facet::SetTheme(theme, false);
    }
    fi::EndRow();
    page.reset = false;
}

FACET_DEMO_SELFTEST("scroll")
{
    using T = facet::demo::Target;
    test.Do([] {
        page.pressed = 0;
        page.reset = true;
    });
    test.Reveal("scroll.long");
    test.Wait(2);

    // The wheel: a smooth scroll that settles one notch per turn.
    test.Wheel(T::Item("scroll.long"), -2.0f);
    test.Expect("the long page starts moving", [] { return page.longScroll > 0.0f; });
    test.WaitSeconds(0.6f);
    test.ExpectNear("two notches scrolled",
                    [] { return static_cast<double>(page.longScroll / facet::Px(facet::T().scroll.wheelDistance)); },
                    2.0, 0.05);
    test.Expect("and the motion has ended", [] { return !page.longScrolling; });

    // The thumb: dragging it scrolls with it.
    test.Drag({T::Item("scroll.long.thumb"), T::Item("scroll.long.thumb").Offset(0.0f, 40.0f)}, 0.3f);
    test.Expect("dragging the thumb scrolled further", [] {
        return page.longScroll > 2.5f * facet::Px(facet::T().scroll.wheelDistance);
    });
    test.Expect("without a motion left over", [] { return !page.longScrolling; });

    // The track: a press below the thumb pages down.
    test.Do([] { page.reset = true; });
    test.Wait(2);
    test.Click("scroll.long.track");
    test.WaitSeconds(0.6f);
    test.Expect("a press on the track pages down", [] { return page.longScroll > 0.0f; });

    // The keyboard: the focus moves to the last button and the area follows it.
    test.Do([] { page.reset = true; });
    test.Wait(2);
    test.Click("scroll.button1");
    test.ExpectNear("the first button was pressed", [] { return page.pressed; }, 1.0, 0.0);
    test.KeyPress(ImGuiKey_End);
    test.Wait(3);
    test.ExpectFocused("scroll.button6");
    test.ExpectVisible("scroll.button6");
    test.Expect("the area scrolled to the focused button", [] { return page.longScroll > 0.0f; });
    test.KeyPress(ImGuiKey_Enter);
    test.ExpectNear("Enter pressed it", [] { return page.pressed; }, 6.0, 0.0);
    test.KeyPress(ImGuiKey_Home);
    test.Wait(3);
    test.ExpectFocused("scroll.button1");
    test.ExpectNear("and back to the top", [] { return page.longScroll; }, 0.0, 0.5);

    // Nested areas: the inner one first, then the outer one.
    test.Do([] { page.reset = true; });
    test.Reveal("scroll.outer");
    test.Wait(2);
    test.Wheel(T::Item("scroll.inner"), -1.0f);
    test.Wheel(T::Item("scroll.inner"), -1.0f);
    test.Wheel(T::Item("scroll.inner"), -1.0f);
    test.Wheel(T::Item("scroll.inner"), -1.0f);
    test.Wheel(T::Item("scroll.inner"), -1.0f);
    test.WaitSeconds(0.6f);
    test.Expect("the inner area reached its end", [] { return page.innerScroll >= page.innerMax - 0.5f; });
    test.Expect("and the outer area took the rest", [] { return page.outerScroll > 0.0f; });
    test.Wheel(T::Item("scroll.inner"), 1.0f);
    test.WaitSeconds(0.6f);
    test.Expect("scrolling back up moves the inner area first",
                [] { return page.innerScroll < page.innerMax - 0.5f; });

    // Both axes: Shift turns the wheel sideways.
    test.Do([] { page.reset = true; });
    test.Reveal("scroll.wide");
    test.Wait(2);
    test.MouseMove(T::Item("scroll.wide"));
    test.KeyDown(ImGuiMod_Shift);
    test.Wheel(T::Item("scroll.wide"), -1.0f);
    test.KeyUp(ImGuiMod_Shift);
    test.WaitSeconds(0.6f);
    test.Expect("Shift and the wheel scroll sideways", [] { return page.wideX > 0.0f && page.wideY == 0.0f; });

    // Reduced motion: the wheel jumps.
    test.Do([] { page.reset = true; });
    test.Click("scroll.reduce");
    test.Reveal("scroll.long");
    test.Wait(2);
    test.Wheel(T::Item("scroll.long"), -1.0f);
    test.ExpectNear("with reduced motion one notch arrives at once",
                    [] { return static_cast<double>(page.longScroll / facet::Px(facet::T().scroll.wheelDistance)); },
                    1.0, 0.05);
    test.Click("scroll.reduce");
    test.Expect("reduced motion is off again", [] { return !facet::T().motion.reduceMotion; });
    test.Do([] { page.reset = true; });
}
