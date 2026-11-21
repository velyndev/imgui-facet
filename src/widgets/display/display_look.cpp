// Looks shared by the display widgets (see display_look.h).

#include "widgets/display/display_look.h"

#include <algorithm>
#include <cmath>

#include "facet/core/anim.h"
#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/input.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "internal/control.h"
#include "internal/popup_host.h"
#include "internal/state_storage.h"

namespace facet::internal
{

// ---- Tones ------------------------------------------------------------------------------------

DisplayToneColors DisplayToneLook(Tone tone, bool disabled)
{
    const Colors& c = T().colors;
    DisplayToneColors look;
    if (disabled)
    {
        look.text = c.labelTertiary;
        look.glyph = c.labelTertiary;
        look.wash = c.fill[3];
        look.solid = c.fill[3];
        look.onSolid = c.labelTertiary;
        return look;
    }
    switch (tone)
    {
    case Tone::Accent:
        look = {c.accentText, c.accentText, c.accentSubtle, c.accent, c.onAccent};
        break;
    case Tone::Success:
        look = {c.success, c.success, c.successSubtle, c.success, c.onSuccess};
        break;
    case Tone::Warning:
        look = {c.warning, c.warning, c.warningSubtle, c.warning, c.onWarning};
        break;
    case Tone::Destructive:
        look = {c.destructive, c.destructive, c.destructiveSubtle, c.destructive, c.onDestructive};
        break;
    case Tone::Neutral:
    default:
        // The neutral wash is the control fill; label reads on every fill (test_contrast.cpp).
        look = {c.label, c.labelSecondary, c.fill[2], c.fill[0], c.label};
        break;
    }
    return look;
}

Icon DisplayToneIcon(Tone tone)
{
    switch (tone)
    {
    case Tone::Success:
        return Icon("success");
    case Tone::Warning:
        return Icon("warning");
    case Tone::Destructive:
        return Icon("error");
    case Tone::Neutral:
    case Tone::Accent:
    default:
        return Icon("info");
    }
}

ImVec4 DisplayProgressColor(Tone tone)
{
    const Colors& c = T().colors;
    switch (tone)
    {
    case Tone::Neutral:
        return c.labelSecondary;
    case Tone::Success:
        return c.success;
    case Tone::Warning:
        return c.warning;
    case Tone::Destructive:
        return c.destructive;
    case Tone::Accent:
    default:
        return c.focusRing;
    }
}

// ---- Text lines -------------------------------------------------------------------------------

float DisplayFontAscent()
{
    ImFontBaked* baked = ImGui::GetFontBaked();
    if (baked == nullptr || baked->Size <= 0.0f)
        return ImGui::GetFontSize() * 0.8f;  // no baked font yet: the usual ascent share of a Latin face
    return baked->Ascent * ImGui::GetFontSize() / baked->Size;
}

float DisplayTextTop(float baseline)
{
    return std::round(baseline - DisplayFontAscent());
}

float DisplayCapsCenter(float textTop)
{
    // CenteredTextY places the capitals of a line in the middle of a box; turned around, it
    // gives where the middle of the capitals of a line at `textTop` is.
    const float box = ImGui::GetTextLineHeight();
    return textTop + box * 0.5f - (CenteredTextY(0.0f, box) - 0.0f);
}

DisplayInlineBox DisplayPlaceInline(ImGuiWindow* window, ImGuiID id, ImVec2 size, float baselineInBox,
                                    TextStyle lineStyle, ImGuiItemFlags itemFlags)
{
    DisplayInlineBox box = DisplayLayoutInline(window, size, baselineInBox, lineStyle);
    box.visible = ImGui::ItemAdd(box.rect, id, nullptr, itemFlags);
    return box;
}

DisplayInlineBox DisplayLayoutInline(ImGuiWindow* window, ImVec2 size, float baselineInBox, TextStyle lineStyle)
{
    DisplayInlineBox box;
    PushTextStyle(lineStyle);
    const float lineAscent = DisplayFontAscent();
    PopTextStyle();
    // Dear ImGui keeps the offset of the line's text top from the line top; text items draw at
    // CursorPos.y + that offset. The mark's own text top offset, as Dear ImGui counts it, is where a
    // line of `lineStyle` text would start for its baseline to meet the mark's.
    const float textOffset = baselineInBox - lineAscent;
    const float lineOffset = window->DC.CurrLineTextBaseOffset;
    float shift = lineOffset - textOffset;
    if (!window->DC.IsSameLine)
        shift = std::max(0.0f, shift);  // never reach above a line of its own
    const ImVec2 cursor = window->DC.CursorPos;
    const float top = std::round(cursor.y + shift);
    box.rect = ImRect(ImVec2(cursor.x, top), ImVec2(cursor.x + size.x, top + size.y));
    box.baseline = top + baselineInBox;
    // Tell Dear ImGui where the mark's text top lies relative to the cursor. A mark moved up keeps
    // the line's text offset as it is, because the text before it stays where it was drawn; a mark
    // moved down lets ItemSize add the move to the line height. An offset below 0, which ItemSize
    // does not take, goes into the height instead.
    const float offset = textOffset + std::min(shift, 0.0f);
    const float reported = std::max(0.0f, offset);
    ImGui::ItemSize(ImVec2(size.x, size.y + (reported - offset)), reported);
    return box;
}

ImGuiID DisplayAnonymousId(ImGuiWindow* window, const char* name)
{
    return window->GetID(name);
}

// ---- Status dots --------------------------------------------------------------------------------

namespace
{

// How far the pulse ring grows: its diameter at the end of a pulse, in dot diameters.
constexpr float kPulseGrowth = 2.25f;
// The opacity of the pulse ring as it leaves the dot; it fades to nothing as it grows.
constexpr float kPulseRingOpacity = 0.45f;

} // namespace

void DrawDisplayDot(ImDrawList* drawList, ImVec2 center, float diameter, ImU32 color, bool pulse)
{
    const float radius = diameter * 0.5f;
    if (pulse && !ReduceMotion())
    {
        const Motion& motion = T().motion;
        const double period = std::max(1e-3, static_cast<double>(motion.pulsePeriod) * std::max(motion.scale, 1e-3f));
        const double cycles = ImGui::GetTime() / period;
        const float phase = static_cast<float>(cycles - std::floor(cycles));
        const float ringRadius = radius + (radius * kPulseGrowth - radius) * phase;
        drawList->AddCircleFilled(center, ringRadius, ScaleAlpha(color, kPulseRingOpacity * (1.0f - phase)));
    }
    drawList->AddCircleFilled(center, radius, color);
}

// ---- Key caps ---------------------------------------------------------------------------------

namespace
{

bool IsModifierKey(ImGuiKey key, ImGuiKeyChord* mod)
{
    switch (key)
    {
    case ImGuiKey_LeftCtrl:
    case ImGuiKey_RightCtrl:
        *mod = ImGuiMod_Ctrl;
        return true;
    case ImGuiKey_LeftShift:
    case ImGuiKey_RightShift:
        *mod = ImGuiMod_Shift;
        return true;
    case ImGuiKey_LeftAlt:
    case ImGuiKey_RightAlt:
        *mod = ImGuiMod_Alt;
        return true;
    case ImGuiKey_LeftSuper:
    case ImGuiKey_RightSuper:
        *mod = ImGuiMod_Super;
        return true;
    default:
        return false;
    }
}

} // namespace

int DisplayChordCaps(ImGuiKeyChord chord, const char* names[kDisplayMaxCaps])
{
    ImGuiKeyChord mods = ChordMods(chord);
    ImGuiKey key = ChordKey(chord);
    ImGuiKeyChord folded = 0;
    if (IsModifierKey(key, &folded))
    {
        mods |= folded;
        key = ImGuiKey_None;
    }
    int count = 0;
    for (const ImGuiKeyChord mod : {ImGuiKeyChord(ImGuiMod_Ctrl), ImGuiKeyChord(ImGuiMod_Shift),
                                    ImGuiKeyChord(ImGuiMod_Alt), ImGuiKeyChord(ImGuiMod_Super)})
        if ((mods & mod) != 0)
            names[count++] = KeyNameText(static_cast<ImGuiKey>(mod));
    if (key != ImGuiKey_None)
    {
        const char* name = KeyNameText(key);
        if (name[0] != '\0')
            names[count++] = name;
    }
    return count;
}

DisplayMarkMetrics DisplayMarkLook(ControlSize size)
{
    const Theme& t = T();
    DisplayMarkMetrics m;
    m.paddingX = Px(t.spacing.xs);
    m.gap = Px(t.spacing.xs);
    m.radius = Px(t.radii.element);
    switch (ResolveControlSize(size))
    {
    case ControlSize::Small:
        m.textStyle = TextStyle::Caption;
        m.lineStyle = TextStyle::Footnote;
        break;
    case ControlSize::Large:
        m.textStyle = TextStyle::Body;
        m.lineStyle = TextStyle::Body;
        break;
    default:
        m.textStyle = TextStyle::Footnote;
        m.lineStyle = TextStyle::Body;
        break;
    }
    // A mark is its text line with room above and below: two xs steps for Regular, one for Small,
    // and a regular control height for Large. Its text is centered in it and sits on the baseline
    // of the line around it (DisplayPlaceInline), so the mark may reach past that line a little.
    PushTextStyle(m.textStyle);
    const float textLine = std::round(ImGui::GetTextLineHeight());
    PopTextStyle();
    switch (ResolveControlSize(size))
    {
    case ControlSize::Small:
        m.height = textLine + Px(t.spacing.xs);
        break;
    case ControlSize::Large:
        m.height = ControlHeight(ControlSize::Regular);
        break;
    default:
        m.height = textLine + Px(t.spacing.xs) * 2.0f;
        break;
    }
    m.height = std::round(m.height);
    return m;
}

float DisplayMarkBaseline(const DisplayMarkMetrics& m)
{
    PushTextStyle(m.textStyle);
    const float baseline = CenteredTextY(0.0f, m.height) + std::round(DisplayFontAscent());
    PopTextStyle();
    return baseline;
}

float DisplayCapsWidth(ImGuiKeyChord chord, const DisplayMarkMetrics& m)
{
    PushTextStyle(m.textStyle);
    float width = 0.0f;
    const char* names[kDisplayMaxCaps] = {};
    const int count = DisplayChordCaps(chord, names);
    if (count == 0)
    {
        width = ImGui::CalcTextSize(GetStrings().unbound).x;
    }
    else
    {
        for (int i = 0; i < count; ++i)
            width += std::max(m.height, std::ceil(ImGui::CalcTextSize(names[i]).x) + m.paddingX * 2.0f);
        width += m.gap * static_cast<float>(count - 1);
    }
    PopTextStyle();
    return std::ceil(width);
}

void DrawDisplayCaps(ImDrawList* drawList, ImVec2 pos, float baseline, ImGuiKeyChord chord, const DisplayMarkMetrics& m,
                     bool pressed, bool disabled)
{
    const Colors& c = T().colors;
    PushTextStyle(m.textStyle);
    const float textTop = DisplayTextTop(baseline);
    const char* names[kDisplayMaxCaps] = {};
    const int count = DisplayChordCaps(chord, names);
    if (count == 0)
    {
        drawList->AddText(ImVec2(pos.x, textTop), ToU32(ControlSecondaryColor(disabled)), GetStrings().unbound);
        PopTextStyle();
        return;
    }
    // A cap: the control fill with a boundary hairline (3:1, so the cap reads as a key), the held
    // look one fill step stronger.
    const ImU32 fill = ToU32(disabled ? c.fill[3] : (pressed ? c.fill[0] : c.fill[2]));
    const ImU32 outline = ToU32(disabled ? c.separator : (pressed ? c.focusRing : ControlBoundaryColor(c)));
    const ImU32 text = ToU32(ControlLabelColor(disabled));
    float x = pos.x;
    for (int i = 0; i < count; ++i)
    {
        const float textWidth = ImGui::CalcTextSize(names[i]).x;
        const float width = std::max(m.height, std::ceil(textWidth) + m.paddingX * 2.0f);
        const ImVec2 a(x, pos.y);
        const ImVec2 b(x + width, pos.y + m.height);
        FillRect(drawList, a, b, fill, m.radius);
        StrokeRect(drawList, a, b, outline, m.radius, HairlineThickness());
        drawList->AddText(ImVec2(std::round(x + (width - textWidth) * 0.5f), textTop), text, names[i]);
        x += width + m.gap;
    }
    PopTextStyle();
}

// ---- Copy feedback ----------------------------------------------------------------------------

namespace
{

struct DisplayCopiedState
{
    double until = -1.0;  // ImGui::GetTime() at which the feedback ends
};

ImGuiID CopiedLevel(ImGuiID id)
{
    return ImHashStr("##display copied", 0, id);
}

} // namespace

void StartDisplayCopiedFeedback(ImGuiID id, const ImRect& anchor)
{
    const Motion& motion = T().motion;
    GetState<DisplayCopiedState>(id).until = ImGui::GetTime() + motion.feedbackHold * std::max(motion.scale, 0.0f);
    OpenPopupLevel(CopiedLevel(id), {.rect = anchor, .side = PopupSide::Above, .align = PopupAlign::Center},
                   {.kind = PopupKind::Tooltip, .takeFocus = false});
}

bool DisplayCopiedFeedback(ImGuiID id, const ImRect& anchor)
{
    const ImGuiID level = CopiedLevel(id);
    if (!IsPopupLevelOpen(level))
        return false;
    DisplayCopiedState& state = GetState<DisplayCopiedState>(id);
    if (ImGui::GetTime() >= state.until)
    {
        ClosePopupLevel(level);
        return false;
    }
    SetPopupLevelAnchor(level, {.rect = anchor, .side = PopupSide::Above, .align = PopupAlign::Center});
    if (BeginPopupLevel(level))
    {
        const Colors& c = T().colors;
        PushTextStyle(TextStyle::Footnote);
        const float size = std::round(ImGui::GetFontSize());
        const ImVec2 at = ImGui::GetCursorScreenPos();
        const float textTop = at.y;
        DrawControlIcon(ImGui::GetWindowDrawList(), Icon("check"), at.x, DisplayCapsCenter(textTop), size,
                        ToU32(c.success));
        ImGui::SetCursorScreenPos(ImVec2(at.x + size + Px(T().spacing.xs), at.y));
        ImGui::PushStyleColor(ImGuiCol_Text, c.label);
        ImGui::TextUnformatted(GetStrings().copied);
        ImGui::PopStyleColor();
        PopTextStyle();
        EndPopupLevel();
    }
    return true;
}

// ---- Dismissing alerts --------------------------------------------------------------------------

bool DisplayDismissKey(ImGuiID itemId)
{
    if (itemId == 0 || !ImGui::IsItemFocused())
        return false;
    // Owning Escape keeps Dear ImGui's navigation from clearing the focus on the press, so the
    // press reaches the alert; an active edit or an open popup owns the key first.
    const bool pressed = ImGui::IsKeyPressed(ImGuiKey_Escape, ImGuiInputFlags_None, itemId);
    if (ImGui::GetKeyOwner(ImGuiKey_Escape) == ImGuiKeyOwner_NoOwner || ImGui::GetKeyOwner(ImGuiKey_Escape) == itemId)
        ImGui::SetKeyOwner(ImGuiKey_Escape, itemId);
    return pressed;
}

} // namespace facet::internal
