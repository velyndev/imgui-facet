// Foundation page: proves the demo harness works before any Facet widget exists.
// It shows text at several sizes, a few stock ImGui widgets, the theme surfaces with the
// contrast of their text tokens, the embedded asset table, the UI scale and the frame time.

#include <cstdio>
#include <iterator>

#include <imgui.h>

#include <facet/facet.h>

#include "internal/embedded_assets.h"
#include "pages/pages.h"

namespace
{

struct FoundationState
{
    bool checked = false;
    int counter = 0;
    float value = 0.5f;
    char name[64] = "Facet";
};

FoundationState foundation;
bool themeBeforeSwitch = false;  // selftest: theme before the switch was operated

// Sample sizes in unscaled pixels; ImGui applies the UI scale on top.
constexpr float kTextSizes[] = {11.0f, 13.0f, 15.0f, 17.0f, 22.0f, 26.0f};

// Width of a theme swatch in multiples of the font size.
constexpr float kSwatchWidthEm = 13.0f;

struct Swatch
{
    const char* name;
    ImVec4 fill;
    const char* primaryName;
    ImVec4 primary;
    const char* secondaryName;
    ImVec4 secondary;
};

// One rounded tile per surface: its name and two text tokens drawn on it, each with the
// measured contrast ratio, so both themes can be checked by eye and by number.
void DrawSwatches(const facet::Theme& theme)
{
    const facet::Colors& c = theme.colors;
    const Swatch swatches[] = {
        {"bg", c.bg, "label", c.label, "labelSecondary", c.labelSecondary},
        {"bgSecondary", c.bgSecondary, "label", c.label, "labelSecondary", c.labelSecondary},
        {"bgTertiary", c.bgTertiary, "label", c.label, "labelSecondary", c.labelSecondary},
        {"bgElevated", c.bgElevated, "label", c.label, "labelSecondary", c.labelSecondary},
        {"accent", c.accent, "onAccent", c.onAccent, nullptr, c.onAccent},
        {"accentPressed", c.accentPressed, "onAccent", c.onAccent, nullptr, c.onAccent},
        {"bgTertiary", c.bgTertiary, "accentText", c.accentText, nullptr, c.accentText},
        {"bg", c.bg, "destructive", c.destructive, "success", c.success},
        {"bg", c.bg, "warning", c.warning, nullptr, c.warning},
    };

    const ImGuiStyle& style = ImGui::GetStyle();
    const float padding = facet::Px(theme.spacing.sm);
    const float lineHeight = ImGui::GetTextLineHeightWithSpacing();
    const ImVec2 size(ImGui::GetFontSize() * kSwatchWidthEm, padding * 2.0f + lineHeight * 3.0f);
    const float radius = facet::Px(theme.radii.card);
    const float rightEdge = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    for (size_t i = 0; i < std::size(swatches); ++i)
    {
        const Swatch& swatch = swatches[i];
        ImGui::Dummy(size);
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        facet::FillRect(drawList, min, max, facet::ToU32(swatch.fill), radius);
        facet::StrokeRect(drawList, min, max, facet::ToU32(c.separator), radius, facet::HairlineThickness());

        char line[96];
        ImVec2 cursor(min.x + padding, min.y + padding);
        drawList->AddText(cursor, facet::ToU32(swatch.primary), swatch.name);
        cursor.y += lineHeight;
        std::snprintf(line, sizeof(line), "%s %.1f:1", swatch.primaryName,
                      static_cast<double>(facet::ContrastRatio(swatch.primary, swatch.fill)));
        drawList->AddText(cursor, facet::ToU32(swatch.primary), line);
        if (swatch.secondaryName != nullptr)
        {
            cursor.y += lineHeight;
            std::snprintf(line, sizeof(line), "%s %.1f:1", swatch.secondaryName,
                          static_cast<double>(facet::ContrastRatio(swatch.secondary, swatch.fill)));
            drawList->AddText(cursor, facet::ToU32(swatch.secondary), line);
        }

        const float nextRight = max.x + style.ItemSpacing.x + size.x;
        if (i + 1 < std::size(swatches) && nextRight <= rightEdge)
            ImGui::SameLine();
    }
}

} // namespace

FACET_DEMO_PAGE("foundation", "Foundation", "Harness", 0, "default", "details")
{
    ImGui::PushFont(nullptr, 22.0f);
    ImGui::TextUnformatted("Foundation");
    ImGui::PopFont();
    ImGui::TextWrapped("Checks the demo harness: font sizes, stock widgets, theme surfaces, "
                       "embedded assets, UI scale and frame timing.");

    ImGui::SeparatorText("Text sizes");
    for (const float size : kTextSizes)
    {
        ImGui::PushFont(nullptr, size);
        // Wrapped, so the large samples stay readable on narrow targets and at scale 2.
        ImGui::TextWrapped("%.0f px  The quick brown fox jumps over the lazy dog",
                           static_cast<double>(size));
        ImGui::PopFont();
    }

    ImGui::SeparatorText("Widgets");
    ImGui::Checkbox("Enable option", &foundation.checked);
    ctx.Probe("checkbox");
    if (ImGui::Button("Increment"))
        ++foundation.counter;
    ctx.Probe("increment");
    ImGui::SameLine();
    ImGui::Text("Count: %d", foundation.counter);
    ImGui::SliderFloat("Value", &foundation.value, 0.0f, 1.0f);
    ctx.Probe("slider");
    ImGui::InputText("Name", foundation.name, sizeof(foundation.name));
    ctx.Probe("name");

    ImGui::SeparatorText("Theme surfaces");
    ImGui::TextWrapped("%s theme. Each tile shows a surface token and the contrast of the text "
                       "tokens drawn on it (4.5:1 or more is readable body text).",
                       ctx.dark ? "Dark" : "Light");
    DrawSwatches(facet::T());

    ImGui::SeparatorText("Embedded assets");
    const auto assets = facet::internal::EmbeddedAssets();
    if (assets.empty())
        ImGui::TextDisabled("No embedded assets.");
    for (const auto& asset : assets)
    {
        ImGui::Bullet();
        ImGui::TextWrapped("%s  (%zu bytes)", asset.name, asset.size);
    }

    ImGui::SeparatorText("Display");
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::Text("UI scale: %.2f (DPI %.2f x user %.2f)", static_cast<double>(facet::Scale()),
                static_cast<double>(facet::DpiScale()), static_cast<double>(facet::UiScale()));
    ImGui::Text("Frame time: %.2f ms (%.0f fps)", static_cast<double>(io.DeltaTime * 1000.0f),
                static_cast<double>(io.Framerate));
    ImGui::Text("Facet version: %s", facet::Version());

    // Capture state "details" shows the section expanded, "default" collapsed.
    if (ctx.capture)
        ImGui::SetNextItemOpen(ctx.captureState == 1, ImGuiCond_Always);
    if (ImGui::TreeNode("Details"))
    {
        ImGui::Text("Display size: %.0f x %.0f", static_cast<double>(io.DisplaySize.x),
                    static_cast<double>(io.DisplaySize.y));
        ImGui::Text("Facet frames: %llu", static_cast<unsigned long long>(facet::FrameIndex()));
        ImGui::Text("Pages registered: %zu", facet::demo::Pages().size());
        ImGui::TreePop();
        // The section is the last thing on the page; keep it in view on small targets.
        ctx.RevealInCapture(1.0f);
    }
}

FACET_DEMO_SELFTEST("foundation")
{
    test.Do([] { foundation = FoundationState{}; });

    test.Click("checkbox");
    test.Expect("clicking the checkbox turns it on", [] { return foundation.checked; });
    test.Click("checkbox");
    test.Expect("clicking the checkbox again turns it off", [] { return !foundation.checked; });
    test.ExpectFocused("checkbox");

    // After mouse input the keyboard cursor is hidden. The first Tab only shows it on the
    // item that already has focus; the next Tab moves on.
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("checkbox");
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("increment");
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Space on the focused button increments the counter",
                [] { return foundation.counter == 1; });

    test.Click("increment");
    test.Expect("clicking the button increments the counter", [] { return foundation.counter == 2; });

    test.KeyPress(ImGuiKey_Tab | ImGuiMod_Shift);
    test.ExpectFocused("checkbox");

    // Theme switch in the title strip: by mouse, then by keyboard back.
    test.Do([] { themeBeforeSwitch = facet::T().dark; });
    test.Click("gallery.dark");
    test.Expect("clicking the theme switch changes the theme",
                [] { return facet::T().dark != themeBeforeSwitch; });
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("gallery.dark");
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Space on the theme switch changes the theme back",
                [] { return facet::T().dark == themeBeforeSwitch; });

    // UI scale list: opened by mouse, operated with the arrow keys and Enter.
    test.Click("gallery.scale");
    test.KeyPress(ImGuiKey_DownArrow);
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("Down and Enter in the UI scale list select 125%",
                [] { return facet::UiScale() == 1.25f; });
    test.Click("gallery.scale");
    test.KeyPress(ImGuiKey_UpArrow);
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("Up and Enter in the UI scale list return to 100%",
                [] { return facet::UiScale() == 1.0f; });
}
