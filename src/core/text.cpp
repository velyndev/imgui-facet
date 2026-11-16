// Fonts and text styles.
//
// Font loading does not exist yet: WeightFont returns nullptr, so every text style draws
// with the current ImGui font at the style's size, and SetFontSet keeps nothing.

#include "facet/core/text.h"

#include "internal/subsystem.h"

namespace facet
{
namespace
{

// The display faces are meant for the two largest styles.
bool UsesDisplayFace(TextStyle style)
{
    return style == TextStyle::LargeTitle || style == TextStyle::Title1;
}

} // namespace

void SetFontSet(const FontSet&)
{
}

ImFont* WeightFont(FontWeight, bool)
{
    return nullptr;
}

void PushTextStyle(TextStyle style)
{
    const TextStyleSpec& spec = T().type[style];
    ImGui::PushFont(WeightFont(spec.weight, UsesDisplayFace(style)), spec.size);
}

void PopTextStyle()
{
    ImGui::PopFont();
}

void Text(TextStyle style, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    TextV(style, fmt, args);
    va_end(args);
}

void TextV(TextStyle style, const char* fmt, va_list args)
{
    PushTextStyle(style);
    ImGui::TextV(fmt, args);
    PopTextStyle();
}

ImVec2 MeasureText(TextStyle style, const char* text, const char* textEnd, float wrapWidth)
{
    PushTextStyle(style);
    const ImVec2 size = ImGui::CalcTextSize(text, textEnd, false, wrapWidth > 0.0f ? wrapWidth : -1.0f);
    PopTextStyle();
    return size;
}

namespace internal
{

// No hooks yet: no fonts are loaded.
Subsystem FontSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "font";
    return subsystem;
}

} // namespace internal

} // namespace facet
