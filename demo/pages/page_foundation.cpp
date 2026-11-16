// Foundation page: proves the demo harness works before any Facet widget exists.
// It shows text at several sizes, a few stock ImGui widgets, the embedded asset table,
// the current UI scale and the frame time.

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

// Sample sizes in unscaled pixels; ImGui applies the UI scale on top.
constexpr float kTextSizes[] = {11.0f, 13.0f, 15.0f, 17.0f, 22.0f, 26.0f};

} // namespace

FACET_DEMO_PAGE("foundation", "Foundation", "Harness", 0, "default", "details")
{
    ImGui::PushFont(nullptr, 22.0f);
    ImGui::TextUnformatted("Foundation");
    ImGui::PopFont();
    ImGui::TextWrapped("Checks the demo harness: font sizes, stock widgets, embedded assets, "
                       "UI scale and frame timing.");

    ImGui::SeparatorText("Text sizes");
    for (const float size : kTextSizes)
    {
        ImGui::PushFont(nullptr, size);
        ImGui::Text("%.0f px  The quick brown fox jumps over the lazy dog", static_cast<double>(size));
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

    ImGui::SeparatorText("Embedded assets");
    const auto assets = facet::internal::EmbeddedAssets();
    if (assets.empty())
        ImGui::TextDisabled("No embedded assets.");
    for (const auto& asset : assets)
        ImGui::BulletText("%s  (%zu bytes)", asset.name, asset.size);

    ImGui::SeparatorText("Display");
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::Text("UI scale: %.2f", static_cast<double>(ctx.scale));
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
}
