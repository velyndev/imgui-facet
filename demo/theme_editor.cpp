#include "theme_editor.h"

#include <algorithm>
#include <cstdio>
#include <string>

#include <imgui.h>

#include <facet/facet.h>

#include "page_helpers.h"

namespace facet::demo
{
namespace
{

constexpr float kUiScaleMin = 0.75f;
constexpr float kUiScaleMax = 2.0f;

// Groups whose sliders move the layout of the pane itself: their drags apply on release, so the
// track does not move under the pointer.
bool ThemeEditorMovesLayout(ThemeEditGroup group)
{
    return group == ThemeEditGroup::Spacing || group == ThemeEditGroup::Type || group == ThemeEditGroup::Heights;
}

// Probe names per group, in ThemeEditGroup order.
constexpr const char* kThemeEditorGroupProbes[kThemeEditGroupCount] = {"radii",  "spacing", "type", "heights",
                                                                        "motion", "blur",    "shadow"};

bool ThemeEditorGroupModified(const ThemeEdits& edits, ThemeEditGroup group)
{
    if (ThemeEditValuesDiffer(edits.factor[static_cast<std::size_t>(group)], 1.0f))
        return true;
    for (int i = 0; i < kThemeEditTokenCount; ++i)
        if (ThemeEditTokens()[static_cast<std::size_t>(i)].group == group && edits.overridden[static_cast<std::size_t>(i)])
            return true;
    return group == ThemeEditGroup::Motion && edits.reduceMotion;
}

void ThemeEditorResetGroup(ThemeEdits& edits, ThemeEditGroup group)
{
    edits.factor[static_cast<std::size_t>(group)] = 1.0f;
    for (int i = 0; i < kThemeEditTokenCount; ++i)
        if (ThemeEditTokens()[static_cast<std::size_t>(i)].group == group)
            edits.overridden[static_cast<std::size_t>(i)] = false;
    if (group == ThemeEditGroup::Motion)
        edits.reduceMotion = false;
}

// A caption line above a control that takes the whole width.
void ThemeEditorCaption(const char* text)
{
    Note("%s", text);
}

// The factor slider of a group with its reset button on the same line. Returns true while the
// slider is dragged.
bool ThemeEditorFactorRow(GalleryState& state, PageCtx& ctx, ThemeEditGroup group, const char* label,
                          ThemeEditGroup resetAlso = ThemeEditGroup::Radii, bool resetPair = false)
{
    const ThemeEditGroupInfo& info = ThemeEditGroupOf(group);
    const char* probe = kThemeEditorGroupProbes[static_cast<int>(group)];
    const facet::Theme& t = facet::T();
    const float button = facet::ControlHeight(facet::ControlSize::Inherit);
    const float gap = facet::Px(t.spacing.xs);
    char name[64];

    float factor = state.edits.factor[static_cast<std::size_t>(group)];
    ImGui::SetNextItemWidth(std::max(button, ImGui::GetContentRegionAvail().x - button - gap));
    facet::SliderOptions slider;
    slider.format = "%.2f\xC3\x97";  // the factor with a multiplication sign
    slider.step = 0.05;
    if (facet::Slider(label, &factor, info.minFactor, info.maxFactor, slider))
        state.edits.factor[static_cast<std::size_t>(group)] = factor;
    const bool dragged = ImGui::IsItemActive();
    std::snprintf(name, sizeof(name), "editor.%s.factor", probe);
    ctx.Probe(name);

    ImGui::SameLine(0.0f, gap);
    const bool modified = ThemeEditorGroupModified(state.edits, group) ||
                          (resetPair && ThemeEditorGroupModified(state.edits, resetAlso));
    char resetLabel[96];
    std::snprintf(resetLabel, sizeof(resetLabel), "Reset %s###reset-%s", info.name, probe);
    ImGui::BeginDisabled(!modified);
    if (facet::IconButton(resetLabel, facet::Icon("reset")))
    {
        ThemeEditorResetGroup(state.edits, group);
        if (resetPair)
            ThemeEditorResetGroup(state.edits, resetAlso);
    }
    ImGui::EndDisabled();
    std::snprintf(name, sizeof(name), "editor.%s.reset", probe);
    ctx.Probe(name);
    return dragged;
}

// A group with a factor and, inside a disclosure, one row per token. Returns true while one of
// its sliders is dragged.
bool ThemeEditorTokenGroup(GalleryState& state, PageCtx& ctx, ThemeEditGroup group)
{
    const ThemeEditGroupInfo& info = ThemeEditGroupOf(group);
    const char* probe = kThemeEditorGroupProbes[static_cast<int>(group)];
    ImGui::PushID(probe);
    facet::SeparatorText(info.name);
    bool dragged = ThemeEditorFactorRow(state, ctx, group, "Factor");

    const bool tokensOpen = facet::BeginDisclosure("Each token###tokens", &state.editorTokensOpen[static_cast<std::size_t>(group)]);
    char name[96];
    std::snprintf(name, sizeof(name), "editor.%s.tokens", probe);
    ctx.Probe(name);
    if (tokensOpen)
    {
        const Theme base = ThemeEditorPreset(state.appliedDark);
        for (int i = 0; i < kThemeEditTokenCount; ++i)
        {
            const ThemeEditToken& token = ThemeEditTokens()[static_cast<std::size_t>(i)];
            if (token.group != group)
                continue;
            facet::SettingsRowOptions row;
            row.modified = state.edits.overridden[static_cast<std::size_t>(i)];
            row.resetButton = true;
            facet::BeginSettingsRow(token.label, row);
            float value = EditedTokenValue(base, state.edits, i);
            facet::SliderOptions slider;
            slider.format = "%.1f";
            slider.step = 0.5;
            if (facet::Slider("##value", &value, token.minValue, token.maxValue, slider))
            {
                state.edits.overridden[static_cast<std::size_t>(i)] = true;
                state.edits.value[static_cast<std::size_t>(i)] = value;
            }
            dragged = dragged || ImGui::IsItemActive();
            std::snprintf(name, sizeof(name), "editor.token.%s", token.path);
            ctx.Probe(name);
            facet::EndSettingsRow();
            if (facet::SettingsRowResetPressed())
                state.edits.overridden[static_cast<std::size_t>(i)] = false;
        }
        facet::EndDisclosure();
    }
    ImGui::PopID();
    return dragged && ThemeEditorMovesLayout(group);
}

} // namespace

Theme ThemeEditorPreset(bool dark)
{
    return dark ? Theme::Dark() : Theme::Light();
}

void GalleryUiScaleControl(GalleryState& state, const char* label, float widthUnits)
{
    float scale = state.uiScaleDrag >= 0.0f ? state.uiScaleDrag : state.uiScale;
    facet::UiScaleControlOptions options;
    options.apply = false;
    options.width = widthUnits;
    const bool written = facet::UiScaleControl(label, &scale, kUiScaleMin, kUiScaleMax, options);
    const bool active = ImGui::IsItemActive();
    if (written && active)
        state.uiScaleDrag = scale;
    else if (written)
        state.uiScale = scale;
    if (!active && state.uiScaleDrag >= 0.0f)
    {
        state.uiScale = state.uiScaleDrag;
        state.uiScaleDrag = -1.0f;
    }
}

std::string ThemeEditorExportText(const GalleryState& state)
{
    return BuildThemeExport(ThemeEditorPreset(state.appliedDark), state.edits, state.appliedDark ? "Dark" : "Light",
                            state.uiScale);
}

void DrawThemeEditor(GalleryState& state, PageCtx& ctx)
{
    bool held = false;

    // ---- Appearance: mode, accent and scale ----
    facet::SeparatorText("Appearance");
    ThemeEditorCaption("Theme");
    facet::ThemeMode mode = state.followSystem ? facet::ThemeMode::System
                                               : (state.dark ? facet::ThemeMode::Dark : facet::ThemeMode::Light);
    facet::ThemeModeControlOptions modeOptions;
    modeOptions.apply = false;
    modeOptions.width = facet::kFill;
    if (facet::ThemeModeControl("##theme", &mode, modeOptions))
    {
        state.followSystem = mode == facet::ThemeMode::System;
        if (!state.followSystem)
            state.dark = mode == facet::ThemeMode::Dark;
    }
    ctx.Probe("editor.theme");

    ThemeEditorCaption("Accent");
    facet::AccentPickerOptions accentOptions;
    accentOptions.apply = false;
    facet::AccentPicker("##accent", &state.edits.themeDefaultAccent, &state.edits.accent, accentOptions);
    ctx.Probe("editor.accent");

    ThemeEditorCaption("UI scale");
    GalleryUiScaleControl(state, "##scale", facet::kFill);
    ctx.Probe("editor.scale");

    // ---- The token groups ----
    held = ThemeEditorTokenGroup(state, ctx, ThemeEditGroup::Radii) || held;
    held = ThemeEditorTokenGroup(state, ctx, ThemeEditGroup::Spacing) || held;
    held = ThemeEditorTokenGroup(state, ctx, ThemeEditGroup::Type) || held;
    held = ThemeEditorTokenGroup(state, ctx, ThemeEditGroup::Heights) || held;

    // ---- Motion ----
    ImGui::PushID("motion");
    facet::SeparatorText("Motion");
    ThemeEditorFactorRow(state, ctx, ThemeEditGroup::Motion, "Scale");
    facet::Toggle("Reduce motion", &state.edits.reduceMotion);
    ctx.Probe("editor.reduce-motion");
    ImGui::PopID();

    // ---- Blur and shadow ----
    ImGui::PushID("effects");
    facet::SeparatorText("Blur and shadow");
    ThemeEditorFactorRow(state, ctx, ThemeEditGroup::Blur, "Blur");
    ThemeEditorFactorRow(state, ctx, ThemeEditGroup::Shadow, "Shadow");
    ImGui::PopID();

    // ---- App or overlay mode ----
    ImGui::PushID("mode");
    facet::SeparatorText("Window");
    facet::Toggle("Overlay mode", &state.overlayMode);
    ctx.Probe("editor.overlay");
    ImGui::BeginDisabled(!state.overlayMode);
    bool animate = !state.backdropPaused;
    if (facet::Toggle("Animate the backdrop", &animate))
        state.backdropPaused = !animate;
    ctx.Probe("editor.backdrop");
    ImGui::EndDisabled();
    if (state.overlayMode)
    {
        const facet::ShortcutId toggle = facet::FindShortcut("demo.overlay");
        if (toggle.IsValid())
            facet::ShortcutHint("Hide the menu", toggle);
        facet::ScreenRegion regions[8];
        Note("The overlay reports %d input region(s); the backdrop takes the rest.",
             facet::GetOverlayInputRegions(regions));
    }
    ImGui::PopID();

    // ---- Everything ----
    facet::Separator();
    // The export text is rebuilt only when what it depends on changed, so the steady state
    // allocates nothing. The copy button shows its own confirmation.
    static std::string exportText;
    static ThemeEdits exportEdits;
    static bool exportDark = false;
    static float exportScale = -1.0f;
    if (exportText.empty() || !(exportEdits == state.edits) || exportDark != state.appliedDark ||
        exportScale != state.uiScale)
    {
        exportText = ThemeEditorExportText(state);
        exportEdits = state.edits;
        exportDark = state.appliedDark;
        exportScale = state.uiScale;
    }
    facet::CopyButtonOptions copy;
    copy.variant = facet::ButtonVariant::Primary;
    copy.iconOnly = false;
    facet::CopyButton("Copy as C++", exportText.c_str(), copy);
    ctx.Probe("editor.copy");
    ImGui::SameLine();
    const bool anything = !(state.edits == ThemeEdits{}) || ThemeEditValuesDiffer(state.uiScale, 1.0f);
    ImGui::BeginDisabled(!anything);
    facet::ButtonOptions reset;
    reset.icon = facet::Icon("reset");
    if (facet::Button("Reset all", reset))
    {
        state.edits = ThemeEdits{};
        state.uiScale = 1.0f;
        state.uiScaleDrag = -1.0f;
    }
    ImGui::EndDisabled();
    ctx.Probe("editor.reset-all");
    Note("Reset all returns every token, the accent and the UI scale to the preset; the theme and window "
         "mode stay.");

    state.editsHeld = held;
}

} // namespace facet::demo
