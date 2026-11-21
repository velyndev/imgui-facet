// ColorPicker: an inline picker panel. From the top: the saturation and value square (or, in the
// Wheel mode, a hue ring around a saturation and value triangle), the hue bar (Square mode), the
// alpha bar (with `alpha`), the compare strip (with `original`: the current color beside the
// original one, which restores it when activated), ColorInputs and a SwatchPalette. Each part is
// its own Tab stop. The square and the bars are the shared parts of color_parts.h, the triangle and
// the ring run the two-axis input of color_plane.h, and the inputs and swatches are the public
// widgets, so every part of the panel behaves as it does on its own. The hue memory under the
// picker's id keeps the hue and the saturation while the color passes through grey and black.

#include "facet/widgets/pickers.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/popup_host.h"
#include "widgets/pickers/color_model.h"
#include "widgets/pickers/color_parts.h"
#include "widgets/pickers/color_plane.h"
#include "widgets/sliders/slider_core.h"

namespace facet
{
namespace
{

constexpr float kTwoPi = 6.28318530718f;
// The hue ring in this many stretches: eight per sixth of the circle, so each stretch lies inside
// one sixth, where the gamma-encoded channels change linearly and the colors between the sample
// points are exact.
constexpr int kWheelStretches = 48;
// Parts of the panel, for forced capture states: the square or triangle, the hue bar or ring, the
// alpha bar, the compare swatch, then the inputs and the swatches.
constexpr int kPartSquare = 0;
constexpr int kPartHue = 1;
constexpr int kPartAlpha = 2;
constexpr int kPartCompare = 3;
constexpr int kPartInputs = 4;
constexpr int kPartSwatches = 5;

// The geometry of the wheel: the ring band and the triangle inside it. Hue 0 points up, and the
// hue grows clockwise; the triangle's corners are the pure hue (right), white (lower left) and
// black (upper left).
struct WheelGeometry
{
    ImVec2 center;
    float outer = 0.0f;
    float inner = 0.0f;
    ImVec2 hue;
    ImVec2 white;
    ImVec2 black;
};

WheelGeometry MakeWheel(const ImRect& rect, float ringWidth, float gap)
{
    WheelGeometry w;
    w.center = ImVec2(std::round(rect.GetCenter().x), std::round(rect.GetCenter().y));
    w.outer = std::floor(std::min(rect.GetWidth(), rect.GetHeight()) * 0.5f);
    w.inner = std::max(1.0f, w.outer - ringWidth);
    const float r = std::max(1.0f, w.inner - gap);
    const auto corner = [&](float angle) { return ImVec2(w.center.x + std::cos(angle) * r, w.center.y + std::sin(angle) * r); };
    w.hue = corner(0.0f);
    w.white = corner(kTwoPi / 3.0f);
    w.black = corner(kTwoPi * 2.0f / 3.0f);
    return w;
}

// The point of saturation s and value v in the triangle: black + v * ((white - black) + s * (hue - white)).
ImVec2 TrianglePoint(const WheelGeometry& w, double s, double v)
{
    const float fs = static_cast<float>(s);
    const float fv = static_cast<float>(v);
    return ImVec2(w.black.x + fv * ((w.white.x - w.black.x) + fs * (w.hue.x - w.white.x)),
                  w.black.y + fv * ((w.white.y - w.black.y) + fs * (w.hue.y - w.white.y)));
}

ImVec2 TriangleMap(const void* user, ImVec2 point)
{
    const WheelGeometry& w = *static_cast<const WheelGeometry*>(user);
    // Solve point - black = a * (white - black) + b * (hue - white) for v = a and s = b / a.
    const double ux = w.white.x - w.black.x, uy = w.white.y - w.black.y;
    const double vx = w.hue.x - w.white.x, vy = w.hue.y - w.white.y;
    const double px = point.x - w.black.x, py = point.y - w.black.y;
    const double det = ux * vy - uy * vx;
    if (std::fabs(det) < 1e-9)
        return ImVec2(0.0f, 0.0f);
    double a = (px * vy - py * vx) / det;
    double b = (ux * py - uy * px) / det;
    a = std::clamp(a, 0.0, 1.0);
    b = std::clamp(b, 0.0, a);
    const double s = a > 1e-9 ? b / a : 0.0;
    return ImVec2(static_cast<float>(s), static_cast<float>(a));
}

ImVec2 RingMap(const void* user, ImVec2 point)
{
    const WheelGeometry& w = *static_cast<const WheelGeometry*>(user);
    const double angle = std::atan2(point.y - w.center.y, point.x - w.center.x) + kTwoPi * 0.25;
    double hue = angle / kTwoPi;
    hue -= std::floor(hue);
    return ImVec2(static_cast<float>(hue), 0.0f);
}

ImVec2 RingPoint(const WheelGeometry& w, double hue, float radius)
{
    const float angle = static_cast<float>(hue) * kTwoPi - kTwoPi * 0.25f;
    return ImVec2(w.center.x + std::cos(angle) * radius, w.center.y + std::sin(angle) * radius);
}

ImU32 PickerDim(const ImVec4& color, bool disabled)
{
    return disabled ? ToU32(WithAlpha(color, color.w * internal::kColorDisabledOpacity)) : ToU32(color);
}

void DrawWheelRing(ImDrawList* drawList, const WheelGeometry& w, bool disabled)
{
    const ImVec2 uv = drawList->_Data->TexUvWhitePixel;
    for (int i = 0; i < kWheelStretches; ++i)
    {
        const double h0 = static_cast<double>(i) / kWheelStretches;
        const double h1 = static_cast<double>(i + 1) / kWheelStretches;
        const ImU32 c0 = PickerDim(internal::ColorHueColor(h0), disabled);
        const ImU32 c1 = PickerDim(internal::ColorHueColor(h1), disabled);
        const ImVec2 a = RingPoint(w, h0, w.inner);
        const ImVec2 b = RingPoint(w, h0, w.outer);
        const ImVec2 c = RingPoint(w, h1, w.outer);
        const ImVec2 d = RingPoint(w, h1, w.inner);
        drawList->PrimReserve(6, 4);
        const ImDrawIdx base = static_cast<ImDrawIdx>(drawList->_VtxCurrentIdx);
        drawList->PrimWriteVtx(a, uv, c0);
        drawList->PrimWriteVtx(b, uv, c0);
        drawList->PrimWriteVtx(c, uv, c1);
        drawList->PrimWriteVtx(d, uv, c1);
        drawList->PrimWriteIdx(base);
        drawList->PrimWriteIdx(static_cast<ImDrawIdx>(base + 1));
        drawList->PrimWriteIdx(static_cast<ImDrawIdx>(base + 2));
        drawList->PrimWriteIdx(base);
        drawList->PrimWriteIdx(static_cast<ImDrawIdx>(base + 2));
        drawList->PrimWriteIdx(static_cast<ImDrawIdx>(base + 3));
    }
    // Hairlines along both edges smooth the edges of the band, which the geometry draws without
    // anti-aliasing.
    const ImU32 edge = ToU32(T().colors.separator);
    drawList->AddCircle(w.center, w.outer, edge, 0, HairlineThickness());
    drawList->AddCircle(w.center, w.inner + HairlineThickness(), edge, 0, HairlineThickness());
}

void DrawWheelTriangle(ImDrawList* drawList, const WheelGeometry& w, double hue, bool disabled)
{
    // The corners are the pure hue, white and black; a triangle interpolates its corner colors
    // affinely, which is exactly HSV over gamma-encoded channels.
    const ImVec2 uv = drawList->_Data->TexUvWhitePixel;
    drawList->PrimReserve(3, 3);
    const ImDrawIdx base = static_cast<ImDrawIdx>(drawList->_VtxCurrentIdx);
    drawList->PrimWriteVtx(w.hue, uv, PickerDim(internal::ColorHueColor(hue), disabled));
    drawList->PrimWriteVtx(w.white, uv, PickerDim(internal::ColorFromHsv({hue, 0.0, 1.0}, 1.0f), disabled));
    drawList->PrimWriteVtx(w.black, uv, PickerDim(internal::ColorFromHsv({hue, 0.0, 0.0}, 1.0f), disabled));
    drawList->PrimWriteIdx(base);
    drawList->PrimWriteIdx(static_cast<ImDrawIdx>(base + 1));
    drawList->PrimWriteIdx(static_cast<ImDrawIdx>(base + 2));
    // The same hairline edge as the ring, which also smooths the triangle's sides.
    const ImU32 edge = ToU32(T().colors.separator);
    drawList->AddTriangle(w.hue, w.white, w.black, edge, HairlineThickness());
}

} // namespace

bool ColorPicker(const char* label, ImVec4* color, const ColorPickerOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    ImGuiContext& g = *GImGui;
    const Theme& t = T();
    const internal::NextItemData next = internal::TakeNextItemData();
    const internal::ForcedItemRequest forced = internal::TakeForcedItemState();
    const ImGuiID id = window->GetID(label);
    const bool readOnly = options.readOnly || color == nullptr || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    const ImVec4 start = color != nullptr ? *color : t.colors.accent;
    const float available = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    const float width = std::round(internal::ResolveItemWidth(options.width, next, std::min(available, Px(t.metrics.pickerWidth))));
    const float gap = std::round(Px(t.spacing.sm));
    const internal::SliderMetrics m = internal::GetSliderMetrics(ControlSize::Inherit);
    const float knob = std::min(m.knob, std::max(2.0f, std::floor(width * 0.125f) * 2.0f));

    ImVec4 edited = start;
    internal::ColorHsv hsv = internal::ColorTrackHsv(id, start);
    bool written = false;
    bool anyFocusVisible = false;
    const auto commit = [&](const internal::ColorPartResult& part, bool alphaPart) {
        anyFocusVisible = anyFocusVisible || part.item.focusVisible;
        if (!part.changed || readOnly)
            return;
        const ImVec4 out = internal::ColorForWrite(part.color, edited, options.alpha || alphaPart, true);
        if (!internal::ColorSame(out, edited, true))
        {
            edited = out;
            written = true;
        }
        internal::ColorRememberHsv(id, edited, hsv);
    };
    internal::ColorPartParams params;
    params.readOnly = readOnly;
    params.mixed = options.mixed;
    params.forced = &forced;

    const ImVec2 origin(std::round(window->DC.CursorPos.x), std::round(window->DC.CursorPos.y));
    ImGui::BeginGroup();
    ImGui::PushOverrideID(id);
    internal::ApplyNextItemData(next);
    float y = origin.y;

    // ---- The square, or the ring and the triangle ----
    const ImRect plane(origin.x, y, origin.x + width, y + width);
    ImGui::ItemSize(plane.GetSize());
    if (options.mode == ColorPickerMode::Wheel)
    {
        const float ringWidth = std::min(std::round(Px(t.picker.hueRingWidth)), std::floor(width * 0.25f));
        const WheelGeometry wheel = MakeWheel(plane, ringWidth, std::round(Px(t.spacing.xs)) + knob * 0.5f);
        const ImGuiID ringId = ImGui::GetID("##hue");
        const ImGuiID triangleId = ImGui::GetID("##sv");

        // The ring first: a press inside the ring's inner circle belongs to the triangle, which
        // takes the press over before it runs its own behaviour.
        internal::ItemOptions ringOptions;
        ringOptions.press = internal::PressMode::Click;
        ringOptions.readOnly = readOnly;
        ringOptions.mixed = options.mixed;
        ringOptions.forced = &forced;
        ringOptions.part = kPartHue;
        ringOptions.handCursor = !readOnly;
        internal::ItemState ring = internal::PartBehavior(ringId, plane, ringOptions);
        bool handOver = false;
        if (ring.activated && g.ActiveId == ringId && g.ActiveIdSource == ImGuiInputSource_Mouse)
        {
            const ImVec2 press = g.IO.MouseClickedPos[ImGuiMouseButton_Left];
            const float dx = press.x - wheel.center.x;
            const float dy = press.y - wheel.center.y;
            handOver = dx * dx + dy * dy < wheel.inner * wheel.inner;
        }
        if (handOver)
        {
            ImGui::ClearActiveID();
            ring.held = false;
            ring.active = false;
        }
        double hue = hsv.h;
        double unusedY = 0.0;
        internal::ColorPlaneOptions ringPlane;
        ringPlane.map = RingMap;
        ringPlane.user = &wheel;
        ringPlane.wrapX = true;
        ringPlane.twoAxes = false;
        ringPlane.readOnly = readOnly;
        const internal::ColorPlaneResult ringEdit = internal::ColorPlaneBehavior(ring, &hue, &unusedY, ringPlane);
        internal::ColorPartResult ringPart;
        ringPart.item = ring;
        if (ringEdit.changed && !readOnly)
        {
            hsv.h = hue;
            ringPart.changed = true;
            ringPart.color = internal::ColorFromHsv(hsv, edited.w);
        }
        commit(ringPart, false);

        const ImRect triangleRect(std::min({wheel.hue.x, wheel.white.x, wheel.black.x}),
                                  std::min({wheel.hue.y, wheel.white.y, wheel.black.y}),
                                  std::max({wheel.hue.x, wheel.white.x, wheel.black.x}),
                                  std::max({wheel.hue.y, wheel.white.y, wheel.black.y}));
        if (handOver)
        {
            ImGui::SetActiveID(triangleId, window);
            ImGui::SetFocusID(triangleId, window);
            g.ActiveIdSource = ImGuiInputSource_Mouse;
            g.ActiveIdMouseButton = ImGuiMouseButton_Left;
        }
        internal::ItemOptions triangleOptions = ringOptions;
        triangleOptions.part = kPartSquare;
        const internal::ItemState triangle = internal::PartBehavior(triangleId, triangleRect, triangleOptions);
        double s = std::clamp(hsv.s, 0.0, 1.0);
        double v = std::clamp(hsv.v, 0.0, 1.0);
        internal::ColorPlaneOptions trianglePlane;
        trianglePlane.map = TriangleMap;
        trianglePlane.user = &wheel;
        trianglePlane.readOnly = readOnly;
        const internal::ColorPlaneResult triangleEdit = internal::ColorPlaneBehavior(triangle, &s, &v, trianglePlane);
        internal::ColorPartResult trianglePart;
        trianglePart.item = triangle;
        if (triangleEdit.changed && !readOnly)
        {
            hsv.s = s;
            hsv.v = v;
            trianglePart.changed = true;
            trianglePart.color = internal::ColorFromHsv(hsv, edited.w);
        }
        commit(trianglePart, false);

        if (ImGui::IsRectVisible(plane.Min, plane.Max))
        {
            ImDrawList* drawList = window->DrawList;
            const bool disabled = ring.disabled;
            DrawWheelRing(drawList, wheel, disabled);
            DrawWheelTriangle(drawList, wheel, hsv.h, disabled);
            const bool mixed = options.mixed && !written;
            const float r = knob * 0.5f;
            const ImVec2 ringAt = RingPoint(wheel, hsv.h, (wheel.inner + wheel.outer) * 0.5f);
            const ImVec2 ringCenter(std::round(ringAt.x), std::round(ringAt.y));
            const ImVec2 triAt = TrianglePoint(wheel, std::clamp(hsv.s, 0.0, 1.0), std::clamp(hsv.v, 0.0, 1.0));
            const ImVec2 triCenter(std::round(triAt.x), std::round(triAt.y));
            const auto growth = [](const internal::ItemState& item) {
                return std::min(1.0f, item.pressAmount + item.hoverAmount * 0.35f);
            };
            if (mixed)
            {
                internal::DrawFocusRing(drawList, ImRect(ringCenter.x - r, ringCenter.y - r, ringCenter.x + r, ringCenter.y + r),
                                        r, ring.focusAmount, 0, internal::RingShape::Round);
                internal::DrawFocusRing(drawList, ImRect(triCenter.x - r, triCenter.y - r, triCenter.x + r, triCenter.y + r), r,
                                        triangle.focusAmount, 0, internal::RingShape::Round);
            }
            else
            {
                internal::DrawColorHandle(drawList, ringCenter, r, internal::ColorHueColor(hsv.h),
                                          readOnly ? 0.0f : growth(ring), ring.focusAmount, disabled);
                internal::DrawColorHandle(drawList, triCenter, r,
                                          internal::ColorFromHsv({hsv.h, std::clamp(hsv.s, 0.0, 1.0), std::clamp(hsv.v, 0.0, 1.0)}, 1.0f),
                                          readOnly ? 0.0f : growth(triangle), triangle.focusAmount, disabled);
            }
        }
    }
    else
    {
        params.part = kPartSquare;
        const internal::ColorPartResult square =
            internal::ColorSquarePart(ImGui::GetID("##sv"), plane, knob, edited, &hsv, params);
        commit(square, false);
    }
    y = plane.Max.y + gap;

    // ---- The hue bar (Square mode) and the alpha bar ----
    const auto bar = [&](const char* barId, ColorChannel channel, int part) {
        ImGui::SetCursorScreenPos(ImVec2(origin.x, y));
        const internal::SliderFrame frame =
            internal::LayoutSliderFrame(ImVec2(origin.x, y), "", width, 0.0f, m, Orientation::Horizontal, 0.0f);
        ImGui::ItemSize(frame.widget.GetSize());
        params.part = part;
        const internal::ColorPartResult result = internal::ColorBarPart(ImGui::GetID(barId), frame.control, frame.track,
                                                                        knob, Orientation::Horizontal, channel, edited,
                                                                        &hsv, params);
        commit(result, channel == ColorChannel::Alpha);
        y = frame.widget.Max.y + gap;
    };
    if (options.mode == ColorPickerMode::Square)
        bar("##hue", ColorChannel::Hue, kPartHue);
    if (options.alpha)
        bar("##alpha", ColorChannel::Alpha, kPartAlpha);

    // ---- The compare strip: the color now beside the original, which restores it ----
    if (options.original != nullptr)
    {
        const ImGuiID compareId = ImGui::GetID("##original");
        const float height = std::round(ControlHeight(ControlSize::Inherit));
        const ImRect strip(origin.x, y, origin.x + width, y + height);
        ImGui::SetCursorScreenPos(strip.Min);
        ImGui::ItemSize(strip.GetSize());
        const float mid = std::round(strip.GetCenter().x);
        const ImRect now(strip.Min, ImVec2(mid, strip.Max.y));
        const ImRect before(ImVec2(mid, strip.Min.y), strip.Max);
        internal::ItemOptions compareOptions;
        compareOptions.readOnly = readOnly;
        compareOptions.forced = &forced;
        compareOptions.part = kPartCompare;
        compareOptions.handCursor = !readOnly;
        const internal::ItemState compare = internal::PartBehavior(compareId, before, compareOptions);
        anyFocusVisible = anyFocusVisible || compare.focusVisible;
        if (compare.pressed && !readOnly)
        {
            const ImVec4 out = internal::ColorForWrite(*options.original, edited, options.alpha, options.hdr);
            if (!internal::ColorSame(out, edited, true))
            {
                edited = out;
                written = true;
                hsv = internal::ColorTrackHsv(id, edited);
                ImGui::MarkItemEdited(compareId);
            }
        }
        if (ImGui::IsRectVisible(strip.Min, strip.Max))
        {
            ImDrawList* drawList = window->DrawList;
            const float radius = Px(t.radii.element);
            const ImVec4 shownNow = options.alpha ? edited : WithAlpha(edited, 1.0f);
            const ImVec4 shownBefore = options.alpha ? *options.original : WithAlpha(*options.original, 1.0f);
            if (compare.disabled)
            {
                FillRect(drawList, now.Min, now.Max, PickerDim(internal::ColorOnChecker(shownNow), true), radius,
                         ImDrawFlags_RoundLeft);
                FillRect(drawList, before.Min, before.Max, PickerDim(internal::ColorOnChecker(shownBefore), true), radius,
                         ImDrawFlags_RoundRight);
            }
            else
            {
                internal::DrawColorSwatch(drawList, now, shownNow, radius, false, ImDrawFlags_RoundLeft);
                internal::DrawColorSwatch(drawList, before, shownBefore, radius, false, ImDrawFlags_RoundRight);
            }
            StrokeRect(drawList, strip.Min, strip.Max, ToU32(t.colors.separator), radius, HairlineThickness());
            // The original half shows that it can be pressed: a hairline in the label color on
            // hover and a slight inset while held.
            if (!readOnly && !compare.disabled && (compare.hoverAmount > 0.0f || compare.pressAmount > 0.0f))
            {
                ImRect edge = before;
                edge.Expand(-HairlineThickness() * compare.pressAmount);
                StrokeRect(drawList, edge.Min, edge.Max,
                           ToU32(WithAlpha(t.colors.label, t.colors.label.w * std::max(compare.hoverAmount, compare.pressAmount))),
                           radius, HairlineThickness(), ImDrawFlags_RoundRight);
            }
            internal::DrawFocusRing(drawList, before, radius, compare.focusAmount, ImDrawFlags_RoundRight);
        }
        if (internal::BeginPopupTooltip(compareId, before, compare.hovered, compare.focusVisible))
        {
            Text(TextStyle::Footnote, "%s", GetStrings().original);
            internal::EndPopupTooltip();
        }
        y = strip.Max.y + gap;
    }

    // ---- The inputs and the swatches: the public widgets ----
    const auto forceFor = [&](int part) {
        if (forced.state != internal::ForcedItemState::None && forced.part == part)
            internal::ForceNextItemState(forced.state);
    };
    if (options.inputs)
    {
        ImGui::SetCursorScreenPos(ImVec2(origin.x, y));
        ColorInputsOptions inputs;
        inputs.alpha = options.alpha;
        inputs.hdr = options.hdr;
        inputs.readOnly = readOnly;
        inputs.mixed = options.mixed && !written;
        inputs.width = width / Px(1.0f);
        ImVec4 value = edited;
        forceFor(kPartInputs);
        if (ColorInputs("##inputs", &value, inputs) && !readOnly)
        {
            if (!internal::ColorSame(value, edited, true))
            {
                edited = value;
                written = true;
                hsv = internal::ColorTrackHsv(id, edited);
            }
        }
        anyFocusVisible = anyFocusVisible || (ImGui::IsItemFocused() && g.NavCursorVisible);
        y = ImGui::GetItemRectMax().y + gap;
    }
    float bottom = y - gap;
    if (!options.swatches.empty())
    {
        ImGui::SetCursorScreenPos(ImVec2(origin.x, y));
        SwatchPaletteOptions palette;
        palette.alpha = options.alpha;
        palette.readOnly = readOnly;
        palette.width = width / Px(1.0f);
        ImVec4 value = edited;
        forceFor(kPartSwatches);
        if (SwatchPalette("##swatches", &value, options.swatches, nullptr, palette) && !readOnly)
        {
            if (!internal::ColorSame(value, edited, true))
            {
                edited = value;
                written = true;
                hsv = internal::ColorTrackHsv(id, edited);
            }
        }
        anyFocusVisible = anyFocusVisible || (ImGui::IsItemFocused() && g.NavCursorVisible);
        bottom = ImGui::GetItemRectMax().y;
    }
    ImGui::PopID();

    if (written && color != nullptr)
        *color = edited;

    // A panel shows its label as its tooltip on hover and keyboard focus (rule 6).
    const char* labelEnd = ImGui::FindRenderedTextEnd(label);
    // The tooltip belongs to the whole panel and shows below it; the pointer brings it up over the
    // square or wheel, where no part has a tooltip of its own.
    const ImRect panel(origin, ImVec2(origin.x + width, std::max(bottom, plane.Max.y)));
    const bool hovered = ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(plane.Min, plane.Max);
    internal::CutTextTooltip(id, panel, hovered, anyFocusVisible, label, labelEnd, labelEnd != label);
    ImGui::EndGroup();
    internal::FinishWidgetGroup(id);
    return written;
}

} // namespace facet
