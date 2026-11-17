// Demo page: Buttons. Calls every widget of facet/widgets/buttons.h once, grouped by purpose.
// The widgets are planned, so each call draws its placeholder at the widget's default size;
// Phase 2 replaces them one by one without touching this page's layout.

#include <imgui.h>

#include <facet/facet.h>

#include "gallery.h"
#include "pages/pages.h"

namespace
{

using facet::demo::LabeledRow;
using facet::demo::Paragraph;
using facet::demo::SectionHeader;

struct ButtonsUi
{
    bool pinned = false;
    bool latches[3] = {true, false, false};
    int view = 0;
    int textView = 1;
};

ButtonsUi ui;

// Toolbar items named after the built-in icons they show.
const facet::ChoiceItem kTools[] = {
    {"Grid", facet::Icon("grid")},
    {"List", facet::Icon("list")},
    {"Layers", facet::Icon("layers")},
};
const char* const kToolNames[] = {"Option 1", "Option 2", "Option 3"};

} // namespace

FACET_DEMO_PAGE("buttons", "Buttons", "Widgets", 10)
{
    facet::demo::PageHeader("Buttons",
                            "Push buttons with variants, icon buttons, copy buttons, links and toggle button "
                            "groups. Every widget on this page is a placeholder until it is implemented: it "
                            "has the widget's default size, cannot be operated and returns nothing.");

    SectionHeader("Actions");
    Paragraph("One primary action per view, secondary and plain actions beside it, and a destructive one.");
    LabeledRow("Button");
    // Side by side in a regular row; stacked in the control column when the row is compact.
    const bool stack = facet::AvailableWidthClass() == facet::WidthClass::Compact;
    const float column = ImGui::GetCursorPosX();
    const auto next = [&] {
        if (stack)
            ImGui::SetCursorPosX(column);
        else
            ImGui::SameLine();
    };
    facet::Button("Option 1", {.variant = facet::ButtonVariant::Primary});
    next();
    facet::Button("Option 2", {.icon = facet::Icon("refresh")});
    next();
    facet::Button("Option 3", {.variant = facet::ButtonVariant::Destructive});
    LabeledRow("Icon button");
    facet::IconButton("Pin", facet::Icon("pin"), {.selected = ui.pinned});

    SectionHeader("Copy and links");
    LabeledRow("Copy button");
    facet::CopyButton("Copy sample text", "Sample text", {.showLabel = true});
    LabeledRow("Link");
    facet::Link("Placeholder link", {.url = "about:blank"});

    SectionHeader("Toggle groups");
    Paragraph("Latched tool buttons: independent flags, or one choice at a time.");
    LabeledRow("Independent");
    facet::ToggleButtonGroup("Views", ui.latches, kTools);
    LabeledRow("Exclusive");
    facet::ToggleButtonGroup("View", &ui.view, kTools, {.iconOnly = false});
    LabeledRow("Text only");
    facet::ToggleButtonGroup("Text view", &ui.textView, kToolNames);
}
