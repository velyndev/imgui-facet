// Keys and shortcuts page: a chord capture prototype, three registered shortcuts that change
// a counter on the page (one global, one for this page's window, one plain letter that a text
// field keeps), the conflict message when two of them share a chord, and one key bind per
// mode with a live indicator.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "page_helpers.h"
#include "internal/keys.h"
#include "internal/row_layout.h"
#include "pages/pages.h"

namespace
{

using facet::internal::ChordCapture;
using facet::internal::ChordCaptureEvent;

// Width of the capture field in multiples of the font size: room for "Ctrl+Shift+Alt+Page Up".
constexpr float kCaptureWidthEm = 14.0f;
// Width of the practice text field in multiples of the font size.
constexpr float kFieldWidthEm = 16.0f;

constexpr ImGuiKeyChord kIncreaseChord = ImGuiMod_Ctrl | ImGuiKey_I;
constexpr ImGuiKeyChord kDecreaseChord = ImGuiMod_Ctrl | ImGuiKey_D;
constexpr ImGuiKeyChord kResetChord = ImGuiKey_R;

struct KeysPageState
{
    // Chord capture prototype.
    ChordCapture capture;
    facet::internal::ChordCaptureOptions captureOptions;
    ImGuiKeyChord captured = ImGuiKey_None;
    ChordCaptureEvent lastEvent = ChordCaptureEvent::None;

    // Shortcuts.
    int counter = 0;
    const char* lastShortcut = nullptr;
    char text[64] = "";

    // Key binds.
    facet::Keybind hold{.primary = ImGuiKey_G, .mode = facet::BindMode::Hold};
    facet::Keybind toggle{.primary = ImGuiKey_T, .mode = facet::BindMode::Toggle};
    facet::Keybind always{.mode = facet::BindMode::Always};
    facet::Keybind off{.primary = ImGuiKey_G, .mode = facet::BindMode::Off};
    bool toggleLatch = false;
    bool holdActive = false;
    bool toggleActive = false;
    bool alwaysActive = false;
    bool offActive = false;
};

KeysPageState page;

struct PageShortcuts
{
    facet::ShortcutId increase;
    facet::ShortcutId decrease;
    facet::ShortcutId reset;
};

// Registered on first use; the registry keeps them for the whole run.
const PageShortcuts& Shortcuts()
{
    static const PageShortcuts shortcuts = [] {
        PageShortcuts s;
        s.increase = facet::RegisterShortcut("keys.increase", kIncreaseChord,
                                             {.label = "Increase", .group = "Keys page"});
        s.decrease = facet::RegisterShortcut("keys.decrease", kDecreaseChord,
                                             {.label = "Decrease", .group = "Keys page",
                                              .scope = facet::ShortcutScope::Window});
        s.reset = facet::RegisterShortcut("keys.reset", kResetChord, {.label = "Reset", .group = "Keys page"});
        return s;
    }();
    return shortcuts;
}

void ResetShortcuts()
{
    const PageShortcuts& s = Shortcuts();
    for (const facet::ShortcutId id : {s.increase, s.decrease, s.reset})
    {
        facet::ResetShortcut(id);
        facet::SetShortcutEnabled(id, true);
    }
}

std::string ChordText(ImGuiKeyChord chord)
{
    char buffer[64];
    facet::ChordToText(chord, buffer, sizeof(buffer));
    return buffer;
}

std::string ConfigText(ImGuiKeyChord chord)
{
    char buffer[64];
    facet::ChordToConfigText(chord, buffer, sizeof(buffer));
    return buffer;
}

const char* ScopeName(facet::ShortcutScope scope)
{
    switch (scope)
    {
    case facet::ShortcutScope::Window:
        return "this page";
    case facet::ShortcutScope::FocusedItem:
        return "focused item";
    case facet::ShortcutScope::Global:
        break;
    }
    return "global";
}

// --- Chord capture prototype --------------------------------------------------------------------

void DrawCaptureField(const facet::Theme& theme, const facet::demo::PageCtx& ctx)
{
    const facet::Colors& c = theme.colors;
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    const ImGuiID id = ImGui::GetID("##capture");
    const float width = std::min(ImGui::CalcItemWidth(), ImGui::GetFontSize() * kCaptureWidthEm);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImRect bb(pos, ImVec2(pos.x + width, pos.y + ImGui::GetFrameHeight()));
    ImGui::ItemSize(bb, ImGui::GetStyle().FramePadding.y);
    const bool visible = ImGui::ItemAdd(bb, id);
    // Probed even when scrolled out of view, so a selftest can scroll to it.
    ctx.Probe("keys.capture");
    if (!visible && !page.capture.Listening())
        return;

    if (ctx.capture && ctx.captureState == 1 && !page.capture.Listening())
    {
        page.capture.Start(id, page.captureOptions);
        ImGui::SetActiveID(id, window);
    }

    bool hovered = false;
    if (!page.capture.Listening())
    {
        bool held = false;
        // Click, Enter or Space starts listening; the field then stays the active item.
        if (ImGui::ButtonBehavior(bb, id, &hovered, &held))
        {
            page.capture.Start(id, page.captureOptions);
            ImGui::SetActiveID(id, window);
            ImGui::SetFocusID(id, window);
        }
    }
    else
    {
        hovered = ImGui::IsItemHovered();
        const ChordCaptureEvent event = page.capture.Update();
        if (event != ChordCaptureEvent::None)
        {
            page.lastEvent = event;
            if (event == ChordCaptureEvent::Captured)
                page.captured = page.capture.Result();
            if (ImGui::GetActiveID() == id)
                ImGui::ClearActiveID();
        }
    }
    facet::ItemCursor();

    // The frame: a stock-style field, its outline in the focus ring color while listening.
    const bool listening = page.capture.Listening();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const float radius = facet::Px(theme.radii.control);
    facet::FillRect(drawList, bb.Min, bb.Max, facet::ToU32(hovered ? c.fill[0] : c.fill[1]), radius);
    if (listening)
        facet::StrokeRect(drawList, bb.Min, bb.Max, facet::ToU32(c.focusRing), radius,
                          facet::Px(theme.metrics.focusRingWidth) * 0.5f);
    else
        facet::StrokeRect(drawList, bb.Min, bb.Max, facet::ToU32(facet::ControlBoundaryColor(c)), radius,
                          facet::HairlineThickness());
    ImGui::RenderNavCursor(bb, id);

    std::string label;
    ImVec4 color = c.label;
    if (listening)
    {
        const ImGuiKeyChord held = page.capture.Held();
        label = held != 0 ? ChordText(held) + "..." : "Press a chord; Esc or a click cancels";
        color = c.labelSecondary;
    }
    else if (page.captured != ImGuiKey_None)
    {
        label = ChordText(page.captured);
    }
    else
    {
        label = "Not set: click to record";
        color = c.labelSecondary;
    }
    const ImGuiStyle& style = ImGui::GetStyle();
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::RenderTextClipped(ImVec2(bb.Min.x + style.FramePadding.x, bb.Min.y),
                             ImVec2(bb.Max.x - style.FramePadding.x, bb.Max.y), label.c_str(), nullptr, nullptr,
                             ImVec2(0.0f, 0.5f));
    ImGui::PopStyleColor();
}

void DrawCaptureSection(const facet::Theme& theme, const facet::demo::PageCtx& ctx)
{
    facet::demo::SectionHeader("Chord capture");
    facet::demo::Paragraph("A prototype of the capture that KeybindCapture will use. While it listens it owns "
                           "every key, so shortcuts, key binds and keyboard navigation stay quiet; the chord "
                           "is taken when its last key is released.");
    facet::internal::BeginRowGroup("capture");
    facet::internal::BeginRow("Chord");
    const float right = facet::demo::ContentRight();
    DrawCaptureField(theme, ctx);
    facet::demo::SameLineIfFits(right, facet::demo::ButtonWidth("Clear"));
    if (facet::Button("Clear"))
    {
        page.capture.Clear();
        page.captured = ImGuiKey_None;
        page.lastEvent = ChordCaptureEvent::Cleared;
        if (ImGui::GetActiveID() == ImGui::GetID("##capture"))
            ImGui::ClearActiveID();
    }
    ctx.Probe("keys.clear");
    facet::internal::EndRow();

    // A row that runs out of room continues under its first control, in the control column.
    facet::internal::BeginRow("Accept");
    const float rowRight = facet::demo::ContentRight();
    facet::Checkbox("Mouse buttons", &page.captureOptions.allowMouse);
    ctx.Probe("keys.allowMouse");
    facet::demo::SameLineIfFits(rowRight, facet::demo::CheckboxWidth("Gamepad buttons"));
    facet::Checkbox("Gamepad buttons", &page.captureOptions.allowGamepad);
    facet::demo::SameLineIfFits(rowRight, facet::demo::CheckboxWidth("Modifiers alone"));
    facet::Checkbox("Modifiers alone", &page.captureOptions.allowModifierOnly);
    facet::internal::EndRow();
    facet::internal::EndRowGroup();

    switch (page.lastEvent)
    {
    case ChordCaptureEvent::Captured:
        facet::demo::Note("Captured. Saved in a config file as \"%s\".", ConfigText(page.captured).c_str());
        break;
    case ChordCaptureEvent::Canceled:
        facet::demo::Note("Canceled: the chord was kept.");
        break;
    case ChordCaptureEvent::Cleared:
        facet::demo::Note("Cleared.");
        break;
    case ChordCaptureEvent::None:
        facet::demo::Note("Escape cancels, so Escape itself cannot be recorded.");
        break;
    }
}

// --- Shortcuts --------------------------------------------------------------------------------

void PollShortcuts()
{
    const PageShortcuts& s = Shortcuts();
    if (facet::ShortcutPressed(s.increase))
    {
        ++page.counter;
        page.lastShortcut = "Increase";
    }
    if (facet::ShortcutPressed(s.decrease))
    {
        --page.counter;
        page.lastShortcut = "Decrease";
    }
    if (facet::ShortcutPressed(s.reset))
    {
        page.counter = 0;
        page.lastShortcut = "Reset";
    }
}

bool conflictShown = false;

void DrawShortcutSection(const facet::Theme& theme, const facet::demo::PageCtx& ctx)
{
    const facet::Colors& c = theme.colors;
    const PageShortcuts& s = Shortcuts();
    facet::demo::SectionHeader("Shortcuts");
    facet::demo::Paragraph("Three registered shortcuts change the counter below. Increase is global, Decrease "
                           "works while this page has the keyboard focus, and Reset is the plain R key, which a "
                           "text field keeps for typing.");

    // The counter in the row's text style, so it shares the baseline of its label and note.
    facet::internal::BeginRowGroup("shortcuts");
    facet::internal::BeginRow("Counter");
    ImGui::AlignTextToFramePadding();
    facet::Text(facet::TextStyle::Body, "%d", page.counter);
    ctx.Probe("keys.counter");
    if (page.lastShortcut != nullptr)
    {
        ImGui::SameLine(0.0f, facet::Px(theme.spacing.md));
        ImGui::AlignTextToFramePadding();
        ImGui::PushStyleColor(ImGuiCol_Text, c.labelSecondary);
        facet::Text(facet::TextStyle::Body, "Last: %s", page.lastShortcut);
        ImGui::PopStyleColor();
    }
    facet::internal::EndRow();
    facet::internal::EndRowGroup();

    if (ImGui::BeginTable("##shortcuts", 4, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn("Action");
        ImGui::TableSetupColumn("Chord");
        ImGui::TableSetupColumn("Where");
        ImGui::TableSetupColumn("Enabled");
        ImGui::TableHeadersRow();
        for (int i = 0; i < facet::ShortcutCount(); ++i)
        {
            facet::ShortcutInfo info;
            if (!facet::GetShortcut(i, &info) || std::string(info.group) != "Keys page")
                continue;
            ImGui::PushID(static_cast<int>(info.id.value));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(info.label);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(ChordText(info.primary).c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(ScopeName(info.scope));
            ImGui::TableNextColumn();
            bool enabled = info.enabled;
            if (facet::Checkbox("##enabled", &enabled))
                facet::SetShortcutEnabled(info.id, enabled);
            ctx.Probe(std::string("keys.enabled.") + info.name);
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    const float right = facet::demo::ContentRight();
    if (facet::Button("Bind Decrease to Ctrl+I"))
        facet::SetShortcutChords(s.decrease, kIncreaseChord, ImGuiKey_None);
    ctx.Probe("keys.makeConflict");
    facet::demo::SameLineIfFits(right, facet::demo::ButtonWidth("Restore the chords"));
    if (facet::Button("Restore the chords"))
        ResetShortcuts();
    ctx.Probe("keys.restore");

    // The conflict message names both entries and says which one wins.
    conflictShown = false;
    facet::ShortcutInfo decrease;
    if (facet::GetShortcut(s.decrease, &decrease))
    {
        const facet::ShortcutId other = facet::FindChordConflict(decrease.primary, s.decrease);
        facet::ShortcutInfo otherInfo;
        if (other.IsValid() && facet::GetShortcut(other, &otherInfo))
        {
            conflictShown = true;
            ImGui::AlignTextToFramePadding();
            facet::IconGlyph(facet::Icon("warning"), 0.0f, facet::ToU32(c.warning));
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, c.warning);
            ImGui::TextWrapped("Conflict: %s is used by %s and %s. While this page has the focus, %s (this page) "
                               "wins; elsewhere %s (global) does.",
                               ChordText(decrease.primary).c_str(), otherInfo.label, decrease.label, decrease.label,
                               otherInfo.label);
            ImGui::PopStyleColor();
            ctx.RevealInCapture();
        }
    }

    facet::internal::BeginRow("Type here");
    ImGui::SetNextItemWidth(std::min(ImGui::CalcItemWidth(), ImGui::GetFontSize() * kFieldWidthEm));
    facet::TextFieldOptions textOptions;
    textOptions.placeholder = "R types an r here instead of resetting";
    facet::TextField("##text", page.text, sizeof(page.text), textOptions);
    ctx.Probe("keys.text");
    facet::internal::EndRow();
}

// --- Key binds ------------------------------------------------------------------------------------

void BindRow(const facet::Theme& theme, const char* mode, const facet::Keybind& bind, bool active,
             const char* probe, const facet::demo::PageCtx& ctx)
{
    const facet::Colors& c = theme.colors;
    facet::internal::BeginRow(mode);
    // The indicator: a filled dot when active, a hollow one when not, and the word, so the
    // state never depends on color. Both use neutral label colors: the accent is kept for
    // selection, the primary action and focus.
    const float size = ImGui::GetFrameHeight();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(size, size));
    ctx.Probe(probe);
    const ImVec2 center(pos.x + 0.5f * size, pos.y + 0.5f * size);
    const float radius = facet::Px(theme.spacing.xs);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    if (active)
        drawList->AddCircleFilled(center, radius, facet::ToU32(c.label));
    else
        drawList->AddCircle(center, radius, facet::ToU32(facet::ControlBoundaryColor(c)), 0,
                            facet::HairlineThickness());
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    std::string chords = bind.primary != ImGuiKey_None ? ChordText(bind.primary) : std::string("no key");
    ImGui::Text("%s  %s", active ? "Active" : "Inactive", chords.c_str());
    facet::internal::EndRow();
}

void DrawBindSection(const facet::Theme& theme, const facet::demo::PageCtx& ctx)
{
    facet::demo::SectionHeader("Key binds");
    facet::demo::Paragraph("One bind per mode. Hold is active while G is held, Toggle flips on each press of T "
                           "(holding T flips it once), Always and Off ignore the keys. While the text field above "
                           "has the keyboard, G and T type letters instead, unless the bind asks to work while "
                           "typing.");
    page.holdActive = facet::IsBindActive(page.hold, nullptr);
    page.toggleActive = facet::IsBindActive(page.toggle, &page.toggleLatch);
    page.alwaysActive = facet::IsBindActive(page.always, nullptr);
    page.offActive = facet::IsBindActive(page.off, nullptr);
    facet::internal::BeginRowGroup("binds");
    BindRow(theme, "Hold", page.hold, page.holdActive, "keys.bind.hold", ctx);
    BindRow(theme, "Toggle", page.toggle, page.toggleActive, "keys.bind.toggle", ctx);
    BindRow(theme, "Always", page.always, page.alwaysActive, "keys.bind.always", ctx);
    BindRow(theme, "Off", page.off, page.offActive, "keys.bind.off", ctx);
    facet::internal::BeginRow("Hold bind");
    facet::Checkbox("Works while typing", &page.hold.whileTextInput);
    ctx.Probe("keys.whileTyping");
    facet::internal::EndRow();
    facet::internal::EndRowGroup();
}

} // namespace

FACET_DEMO_PAGE("keys", "Keys and shortcuts", "Blocks", 90, "default", "listening", "conflict")
{
    const facet::Theme& theme = facet::T();
    const PageShortcuts& s = Shortcuts();
    if (ctx.capture)
    {
        // Every state starts from the same page.
        if (ctx.captureState != 1 && page.capture.Listening())
            page.capture.Cancel();
        page.captured = ctx.captureState == 0 ? (ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_K) : ImGuiKey_None;
        page.lastEvent = ctx.captureState == 0 ? ChordCaptureEvent::Captured : ChordCaptureEvent::None;
        page.counter = ctx.captureState == 2 ? -1 : 3;
        page.lastShortcut = ctx.captureState == 2 ? "Decrease" : "Increase";
        page.toggleLatch = ctx.captureState == 0;
        ResetShortcuts();
        if (ctx.captureState == 2)
            facet::SetShortcutChords(s.decrease, kIncreaseChord, ImGuiKey_None);
    }

    facet::demo::PageHeader("Keys and shortcuts",
                            "Key chords as text, a chord capture session, the shortcut registry with its scopes "
                            "and conflicts, and key binds in their four modes. The KeybindCapture and KeybindList "
                            "widgets build on these blocks.");
    PollShortcuts();
    DrawCaptureSection(theme, ctx);
    DrawShortcutSection(theme, ctx);
    DrawBindSection(theme, ctx);
}

FACET_DEMO_SELFTEST("keys")
{
    test.Do([] {
        if (page.capture.Listening())
            page.capture.Cancel();
        page.captured = ImGuiKey_None;
        page.lastEvent = ChordCaptureEvent::None;
        page.captureOptions = {};
        page.counter = 0;
        page.lastShortcut = nullptr;
        page.text[0] = '\0';
        page.toggleLatch = false;
        page.hold.whileTextInput = false;
        ResetShortcuts();
    });
    test.Wait(2);

    // Chord capture: click, press a chord, release.
    test.Click("keys.capture");
    test.Expect("a click starts listening", [] { return page.capture.Listening(); });
    test.KeyPress(ImGuiKey_K | ImGuiMod_Ctrl | ImGuiMod_Shift);
    test.ExpectText("Ctrl+Shift+K is captured", [] { return ChordText(page.captured); }, "Ctrl+Shift+K");
    test.Expect("the counter did not move while capturing", [] { return page.counter == 0; });
    // Escape cancels and keeps the chord.
    test.Click("keys.capture");
    test.KeyPress(ImGuiKey_Escape);
    test.Expect("Escape cancels and keeps Ctrl+Shift+K",
                [] { return !page.capture.Listening() && page.lastEvent == ChordCaptureEvent::Canceled &&
                            ChordText(page.captured) == "Ctrl+Shift+K"; });
    // A captured shortcut chord does not fire the shortcut.
    test.Click("keys.capture");
    test.KeyPress(ImGuiKey_I | ImGuiMod_Ctrl);
    test.Expect("Ctrl+I is captured without increasing the counter",
                [] { return ChordText(page.captured) == "Ctrl+I" && page.counter == 0; });
    // Keyboard start: after mouse input the first Tab shows the keyboard cursor on the focused
    // field, Enter starts listening, and Tab is then recorded instead of moving on.
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("keys.capture");
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("Enter on the focused field starts listening", [] { return page.capture.Listening(); });
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectText("Tab is recorded while listening", [] { return ChordText(page.captured); }, "Tab");
    test.Click("keys.clear");
    test.Expect("Clear removes the chord", [] { return page.captured == ImGuiKey_None; });

    // Shortcuts: the page has the focus after the clicks above.
    test.KeyPress(ImGuiKey_I | ImGuiMod_Ctrl);
    test.KeyPress(ImGuiKey_I | ImGuiMod_Ctrl);
    test.ExpectNear("Ctrl+I twice increases the counter to 2", [] { return page.counter; }, 2.0, 0.0);
    test.KeyPress(ImGuiKey_D | ImGuiMod_Ctrl);
    test.ExpectNear("Ctrl+D decreases it", [] { return page.counter; }, 1.0, 0.0);
    test.KeyPress(ImGuiKey_R);
    test.ExpectNear("R resets it", [] { return page.counter; }, 0.0, 0.0);
    // In the text field R types and Ctrl+I still works.
    test.Click("keys.text");
    test.KeyDown(ImGuiKey_R);
    test.Type("r");
    test.KeyUp(ImGuiKey_R);
    test.KeyPress(ImGuiKey_I | ImGuiMod_Ctrl);
    test.ExpectText("R typed into the field", [] { return std::string(page.text); }, "r");
    test.ExpectNear("R did not reset; Ctrl+I counted", [] { return page.counter; }, 1.0, 0.0);
    test.KeyPress(ImGuiKey_Escape);

    // Disabling a shortcut in the list.
    test.Click("keys.enabled.keys.increase");
    test.KeyPress(ImGuiKey_I | ImGuiMod_Ctrl);
    test.ExpectNear("a disabled shortcut does nothing", [] { return page.counter; }, 1.0, 0.0);
    test.Click("keys.enabled.keys.increase");

    // A conflict: both entries on Ctrl+I; the page's own shortcut wins while it has the focus.
    test.Click("keys.makeConflict");
    test.Expect("the conflict message appears", [] { return conflictShown; });
    test.KeyPress(ImGuiKey_I | ImGuiMod_Ctrl);
    test.ExpectNear("Ctrl+I now decreases (the inner scope wins)", [] { return page.counter; }, 0.0, 0.0);
    test.Click("keys.restore");
    test.Expect("restoring the chords removes the conflict", [] { return !conflictShown; });

    // Key binds.
    test.KeyDown(ImGuiKey_G);
    test.Expect("Hold is active while G is down", [] { return page.holdActive && !page.offActive; });
    test.KeyUp(ImGuiKey_G);
    test.Expect("Hold ends when G is released", [] { return !page.holdActive; });
    test.HoldKey(ImGuiKey_T, 1.0f);
    test.Expect("holding T flips Toggle once", [] { return page.toggleActive; });
    test.KeyPress(ImGuiKey_T);
    test.Expect("the next T press flips it back", [] { return !page.toggleActive; });
    test.Expect("Always is active", [] { return page.alwaysActive; });
    // While typing the plain bind is quiet unless it asks to work while typing.
    test.Click("keys.text");
    test.KeyDown(ImGuiKey_G);
    test.Expect("G in the text field does not activate Hold", [] { return !page.holdActive; });
    test.KeyUp(ImGuiKey_G);
    test.KeyPress(ImGuiKey_Escape);
    test.Click("keys.whileTyping");
    test.Click("keys.text");
    test.KeyDown(ImGuiKey_G);
    test.Expect("with Works while typing, G in the field activates Hold", [] { return page.holdActive; });
    test.KeyUp(ImGuiKey_G);
    test.KeyPress(ImGuiKey_Escape);
    test.Do([] {
        page.hold.whileTextInput = false;
        ResetShortcuts();
    });

    // A right click and the Menu key reach a listening capture that accepts mouse buttons.
    test.Click("keys.allowMouse");
    test.Click("keys.capture");
    test.Click("keys.capture", ImGuiMouseButton_Right);
    test.ExpectText("a right click is captured", [] { return ChordText(page.captured); }, "Mouse Right");
    test.Click("keys.capture");
    test.KeyPress(ImGuiKey_Menu);
    test.ExpectText("the Menu key is captured", [] { return ChordText(page.captured); }, "Menu");
    test.Click("keys.allowMouse");

    // Text editing in the text field: typed text (character input, no IME), Shift+Left
    // selection, copy and paste through the run's own clipboard, and a double click that
    // selects a word (the field's middle lies on or after the last word at every size).
    test.Do([] { page.text[0] = '\0'; });
    test.Click("keys.text");
    test.Type("hello world");
    test.ExpectText("the typed text arrives", [] { return std::string(page.text); }, "hello world");
    for (int i = 0; i < 5; ++i)
        test.KeyPress(ImGuiKey_LeftArrow | ImGuiMod_Shift);
    test.KeyPress(ImGuiKey_C | ImGuiMod_Ctrl);
    test.ExpectClipboard("Shift+Left five times and Ctrl+C copy the last word", "world");
    test.KeyPress(ImGuiKey_End);
    test.KeyPress(ImGuiKey_V | ImGuiMod_Ctrl);
    test.ExpectText("Ctrl+V pastes it at the end", [] { return std::string(page.text); }, "hello worldworld");
    test.DoubleClick("keys.text");
    test.KeyPress(ImGuiKey_C | ImGuiMod_Ctrl);
    test.ExpectClipboard("a double click selects the word under the pointer", "worldworld");
    test.SetClipboard("again");
    test.KeyPress(ImGuiKey_V | ImGuiMod_Ctrl);
    test.ExpectText("pasting replaces the selected word", [] { return std::string(page.text); }, "hello again");
    test.KeyPress(ImGuiKey_Escape);
    test.Do([] { page.text[0] = '\0'; });
}
