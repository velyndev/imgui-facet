// The public API of the controls group (buttons, selection, sliders, inputs, pickers) while its
// widgets are planned: every options struct follows the conventions, and every function is
// safe to call, submits one placeholder item that cannot be operated, leaves the Dear ImGui
// stacks as it found them, returns the neutral result and writes nothing. Also the placeholder
// helper the stubs share (src/widgets/controls_stub.h).

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "support/options_check.h"
#include "support/session.h"
#include "widgets/controls_stub.h"

namespace
{

using facet::test::OptionsFollowConventions;

static_assert(OptionsFollowConventions<facet::ButtonOptions>());
static_assert(OptionsFollowConventions<facet::IconButtonOptions>());
static_assert(OptionsFollowConventions<facet::CopyButtonOptions>());
static_assert(OptionsFollowConventions<facet::LinkOptions>());
static_assert(OptionsFollowConventions<facet::ToggleButtonGroupOptions>());

static_assert(OptionsFollowConventions<facet::ToggleOptions>());
static_assert(OptionsFollowConventions<facet::CheckboxOptions>());
static_assert(OptionsFollowConventions<facet::CheckboxGroupOptions>());
static_assert(OptionsFollowConventions<facet::RadioGroupOptions>());
static_assert(OptionsFollowConventions<facet::SegmentedControlOptions>());
static_assert(OptionsFollowConventions<facet::ChipOptions>());
static_assert(OptionsFollowConventions<facet::ChipGroupOptions>());
static_assert(OptionsFollowConventions<facet::CycleSelectorOptions>());
static_assert(OptionsFollowConventions<facet::KeybindModeSelectorOptions>());
static_assert(OptionsFollowConventions<facet::OptionCardGroupOptions>());

static_assert(OptionsFollowConventions<facet::SliderOptions>());
static_assert(OptionsFollowConventions<facet::SliderSteppedOptions>());
static_assert(OptionsFollowConventions<facet::SliderWithInputOptions>());
static_assert(OptionsFollowConventions<facet::RangeSliderOptions>());
static_assert(OptionsFollowConventions<facet::RangeInputOptions>());
static_assert(OptionsFollowConventions<facet::RangeSliderHistogramOptions>());
static_assert(OptionsFollowConventions<facet::MultiSliderOptions>());
static_assert(OptionsFollowConventions<facet::KnobOptions>());
static_assert(OptionsFollowConventions<facet::XYPadOptions>());
static_assert(OptionsFollowConventions<facet::JoystickPadOptions>());
static_assert(OptionsFollowConventions<facet::ArcRangeSliderOptions>());
static_assert(OptionsFollowConventions<facet::BoxRangePadOptions>());
static_assert(OptionsFollowConventions<facet::AllocationBarOptions>());

static_assert(OptionsFollowConventions<facet::TextFieldOptions>());
static_assert(OptionsFollowConventions<facet::SecureFieldOptions>());
static_assert(OptionsFollowConventions<facet::SearchFieldOptions>());
static_assert(OptionsFollowConventions<facet::FilterFieldOptions>());
static_assert(OptionsFollowConventions<facet::AutocompleteOptions>());
static_assert(OptionsFollowConventions<facet::TagInputOptions>());
static_assert(OptionsFollowConventions<facet::TextAreaOptions>());
static_assert(OptionsFollowConventions<facet::InlineRenameOptions>());
static_assert(OptionsFollowConventions<facet::NumberInputOptions>());
static_assert(OptionsFollowConventions<facet::DragNumberOptions>());
static_assert(OptionsFollowConventions<facet::UnitInputOptions>());
static_assert(OptionsFollowConventions<facet::VectorInputOptions>());
static_assert(OptionsFollowConventions<facet::SizeInputOptions>());
static_assert(OptionsFollowConventions<facet::HexInputOptions>());
static_assert(OptionsFollowConventions<facet::MaskedInputOptions>());
static_assert(OptionsFollowConventions<facet::PinCodeInputOptions>());
static_assert(OptionsFollowConventions<facet::DurationInputOptions>());
static_assert(OptionsFollowConventions<facet::PathFieldOptions>());
static_assert(OptionsFollowConventions<facet::CopyFieldOptions>());
static_assert(OptionsFollowConventions<facet::KeybindCaptureOptions>());
static_assert(OptionsFollowConventions<facet::RatingOptions>());
static_assert(OptionsFollowConventions<facet::FieldValidationOptions>());
static_assert(OptionsFollowConventions<facet::ValidationSummaryOptions>());

static_assert(OptionsFollowConventions<facet::ColorWellOptions>());
static_assert(OptionsFollowConventions<facet::ColorPickerOptions>());
static_assert(OptionsFollowConventions<facet::ColorInputsOptions>());
static_assert(OptionsFollowConventions<facet::SwatchPaletteOptions>());
static_assert(OptionsFollowConventions<facet::SatValSquareOptions>());
static_assert(OptionsFollowConventions<facet::ColorChannelSliderOptions>());
static_assert(OptionsFollowConventions<facet::ColorModeEditorOptions>());
static_assert(OptionsFollowConventions<facet::DatePickerOptions>());
static_assert(OptionsFollowConventions<facet::DateRangePickerOptions>());
static_assert(OptionsFollowConventions<facet::TimePickerOptions>());
static_assert(OptionsFollowConventions<facet::AnglePickerOptions>());
static_assert(OptionsFollowConventions<facet::IconPickerOptions>());
static_assert(OptionsFollowConventions<facet::ThumbnailPickerOptions>());
static_assert(OptionsFollowConventions<facet::FileBrowserOptions>());

// The zero member of every choice enum is the default (rule 12).
static_assert(facet::ButtonVariant{} == facet::ButtonVariant::Secondary);
static_assert(facet::HandleCollision{} == facet::HandleCollision::Clamp);
static_assert(facet::KnobVariant{} == facet::KnobVariant::Arc);
static_assert(facet::SecureReveal{} == facet::SecureReveal::Toggle);
static_assert(facet::ColorPickerMode{} == facet::ColorPickerMode::Square);
static_assert(facet::ColorModel{} == facet::ColorModel::Rgb);
static_assert(facet::ColorChannel{} == facet::ColorChannel::Hue);
static_assert(facet::ColorAnimation{} == facet::ColorAnimation::Static);
static_assert(facet::Weekday{} == facet::Weekday::Monday);
static_assert(facet::DatePrecision{} == facet::DatePrecision::Day);
static_assert(facet::FileBrowserMode{} == facet::FileBrowserMode::Open);

// Sample data. Labels name nothing real: "Option n" and built-in icon names.
const char* const kLabels[] = {"Option 1", "Option 2", "Option 3"};
const facet::ChoiceItem kChoices[] = {
    {"Option 1", facet::Icon("grid")},
    {"Option 2", facet::Icon("list")},
    {"Option 3", facet::Icon("layers")},
};
const facet::OptionCard kCards[] = {{"Option 1", "Placeholder description"}, {"Option 2"}};
const facet::UnitItem kUnits[] = {{"ms", 0.001}, {"s", 1.0}};
const float kStops[] = {0.0f, 0.25f, 1.0f};
const float kBins[] = {1.0f, 3.0f, 2.0f};
const ImVec4 kSwatches[] = {{0.2f, 0.4f, 0.8f, 1.0f}, {0.8f, 0.3f, 0.2f, 1.0f}};
const facet::ThumbnailItem kThumbnails[] = {{"Option 1"}, {"Option 2"}};

// Every value a stub could write, set to values a stub would never produce.
struct Values
{
    bool flag = true;
    facet::CheckState check = facet::CheckState::Mixed;
    int index = 1;
    std::array<bool, 3> flags = {true, false, true};
    float number = 0.25f;
    int integer = 7;
    double real = 1.5;
    float low = 0.2f;
    float high = 0.8f;
    int lowInt = 2;
    int highInt = 8;
    std::array<float, 3> handles = {0.1f, 0.5f, 0.9f};
    std::array<int, 2> handlesInt = {3, 6};
    std::vector<float> handleVector = {0.2f, 0.7f};
    ImVec2 point = {0.3f, 0.4f};
    ImVec2 boxLow = {0.1f, 0.2f};
    ImVec2 boxHigh = {0.6f, 0.7f};
    std::array<float, 3> shares = {0.5f, 0.3f, 0.2f};
    std::string text = "Sample";
    std::array<char, 32> buffer = {'S', 'a', 'm', 'p', 'l', 'e'};
    facet::TextFilter filter = {"Sample", true, false};
    std::vector<std::string> tags = {"Option 1"};
    bool editing = false;
    int unit = 1;
    std::array<float, 3> vector = {1.0f, 2.0f, 3.0f};
    std::array<int, 2> vectorInt = {4, 5};
    ImVec2 size = {640.0f, 480.0f};
    int sizeX = 640;
    int sizeY = 480;
    std::uint32_t hex32 = 0xA1B2C3D4u;
    std::uint64_t hex64 = 0x0123456789ABCDEFull;
    double seconds = 90.0;
    facet::Keybind bind = {ImGuiKey_F5, ImGuiMod_Ctrl | ImGuiKey_K, facet::BindMode::Toggle};
    facet::BindMode mode = facet::BindMode::Always;
    float rating = 3.5f;
    int ratingInt = 2;
    ImVec4 color = {0.1f, 0.2f, 0.3f, 0.4f};
    facet::ColorHistory history = {{{0.5f, 0.5f, 0.5f, 1.0f}}, 1};
    std::vector<ImVec4> swatches = {{0.9f, 0.8f, 0.7f, 1.0f}};
    facet::AnimatedColor animated = {facet::ColorAnimation::Pulse, {0.1f, 0.2f, 0.3f, 1.0f}, {0.4f, 0.5f, 0.6f, 1.0f}, 3.0f};
    facet::Date date = {2001, 2, 3};
    facet::Date start = {2001, 2, 3};
    facet::Date end = {2001, 3, 4};
    facet::TimeOfDay time = {10, 30, 15};
    float radians = 1.0f;
    facet::Icon icon = facet::Icon("star");
    std::array<bool, 2> thumbnailFlags = {false, true};
};

void Append(std::string& out, const char* name, double value)
{
    char line[96];
    std::snprintf(line, sizeof(line), "%s=%.9g;", name, value);
    out += line;
}

void Append(std::string& out, const char* name, ImVec4 value)
{
    Append(out, name, value.x);
    Append(out, name, value.y);
    Append(out, name, value.z);
    Append(out, name, value.w);
}

void Append(std::string& out, const char* name, ImVec2 value)
{
    Append(out, name, value.x);
    Append(out, name, value.y);
}

// Every member as text, so a changed value shows up in the failure message.
std::string Describe(const Values& v)
{
    std::string out;
    Append(out, "flag", v.flag);
    Append(out, "check", static_cast<int>(v.check));
    Append(out, "index", v.index);
    for (bool f : v.flags)
        Append(out, "flags", f);
    Append(out, "number", v.number);
    Append(out, "integer", v.integer);
    Append(out, "real", v.real);
    Append(out, "low", v.low);
    Append(out, "high", v.high);
    Append(out, "lowInt", v.lowInt);
    Append(out, "highInt", v.highInt);
    for (float h : v.handles)
        Append(out, "handles", h);
    for (int h : v.handlesInt)
        Append(out, "handlesInt", h);
    Append(out, "handleVector.size", static_cast<double>(v.handleVector.size()));
    for (float h : v.handleVector)
        Append(out, "handleVector", h);
    Append(out, "point", v.point);
    Append(out, "boxLow", v.boxLow);
    Append(out, "boxHigh", v.boxHigh);
    for (float s : v.shares)
        Append(out, "shares", s);
    out += "text=" + v.text + ";buffer=" + std::string(v.buffer.data()) + ";";
    out += "filter=" + v.filter.query + ";";
    Append(out, "filter.matchCase", v.filter.matchCase);
    Append(out, "filter.wholeWord", v.filter.wholeWord);
    for (const std::string& tag : v.tags)
        out += "tag=" + tag + ";";
    Append(out, "editing", v.editing);
    Append(out, "unit", v.unit);
    for (float c : v.vector)
        Append(out, "vector", c);
    for (int c : v.vectorInt)
        Append(out, "vectorInt", c);
    Append(out, "size", v.size);
    Append(out, "sizeX", v.sizeX);
    Append(out, "sizeY", v.sizeY);
    Append(out, "hex32", v.hex32);
    Append(out, "hex64.high", static_cast<double>(v.hex64 >> 32));
    Append(out, "hex64.low", static_cast<double>(v.hex64 & 0xFFFFFFFFull));
    Append(out, "seconds", v.seconds);
    Append(out, "bind.primary", v.bind.primary);
    Append(out, "bind.alternate", v.bind.alternate);
    Append(out, "bind.mode", static_cast<int>(v.bind.mode));
    Append(out, "mode", static_cast<int>(v.mode));
    Append(out, "rating", v.rating);
    Append(out, "ratingInt", v.ratingInt);
    Append(out, "color", v.color);
    Append(out, "history.count", v.history.count);
    for (const ImVec4& c : v.history.colors)
        Append(out, "history", c);
    for (const ImVec4& c : v.swatches)
        Append(out, "swatches", c);
    Append(out, "animated.animation", static_cast<int>(v.animated.animation));
    Append(out, "animated.color", v.animated.color);
    Append(out, "animated.second", v.animated.second);
    Append(out, "animated.period", v.animated.period);
    Append(out, "date", v.date.year * 10000 + v.date.month * 100 + v.date.day);
    Append(out, "start", v.start.year * 10000 + v.start.month * 100 + v.start.day);
    Append(out, "end", v.end.year * 10000 + v.end.month * 100 + v.end.day);
    Append(out, "time", v.time.hour * 10000 + v.time.minute * 100 + v.time.second);
    Append(out, "radians", v.radians);
    Append(out, "icon", v.icon.id);
    for (bool f : v.thumbnailFlags)
        Append(out, "thumbnailFlags", f);
    return out;
}

// The Dear ImGui stacks a widget call must leave as it found them.
struct Stacks
{
    int ids = 0;
    int windows = 0;
    int groups = 0;
    int itemWidths = 0;
    int itemFlags = 0;
    int colors = 0;
    int styleVars = 0;
    int fonts = 0;
    int disabled = 0;

    friend bool operator==(const Stacks&, const Stacks&) = default;
};

Stacks Snapshot()
{
    ImGuiContext& g = *GImGui;
    return {g.CurrentWindow->IDStack.Size, g.CurrentWindowStack.Size, g.GroupStack.Size,
            g.CurrentWindow->DC.ItemWidthStack.Size, g.ItemFlagsStack.Size, g.ColorStack.Size,
            g.StyleVarStack.Size, g.FontStack.Size, g.DisabledStackSize};
}

// Counts the calls made, so a test can check that the list below reaches every function.
int g_calls = 0;

template <class Result>
void CheckCall(const char* call, const Stacks& before, ImGuiID itemBefore, Result result, Result neutral)
{
    ++g_calls;
    INFO(call);
    CHECK(result == neutral);
    CHECK(Snapshot() == before);
    // One new item, with a size, was submitted.
    ImGuiContext& g = *GImGui;
    CHECK(g.LastItemData.ID != itemBefore);
    CHECK(g.LastItemData.Rect.GetWidth() >= 1.0f);
    CHECK(g.LastItemData.Rect.GetHeight() >= 1.0f);
}

#define FACET_CHECK_STUB(neutral, ...)                                                             \
    do                                                                                             \
    {                                                                                              \
        const Stacks facetBefore = Snapshot();                                                     \
        const ImGuiID facetItemBefore = GImGui->LastItemData.ID;                                   \
        const auto facetResult = (__VA_ARGS__);                                                    \
        CheckCall(#__VA_ARGS__, facetBefore, facetItemBefore, facetResult, neutral);               \
    } while (false)

// A void call that must submit one item and leave the stacks alone.
#define FACET_CHECK_VOID_STUB(...)                                                                 \
    do                                                                                             \
    {                                                                                              \
        const Stacks facetBefore = Snapshot();                                                     \
        const ImGuiID facetItemBefore = GImGui->LastItemData.ID;                                   \
        __VA_ARGS__;                                                                               \
        CheckCall(#__VA_ARGS__, facetBefore, facetItemBefore, 0, 0);                               \
    } while (false)

// Calls every function of the five families once, with the overloads, as an app would.
void CallEveryStub(Values& v)
{
    using namespace facet;
    const std::span<bool> flags(v.flags);
    char* const buffer = v.buffer.data();
    const std::size_t bufferSize = v.buffer.size();

    // Buttons
    FACET_CHECK_STUB(false, Button("Button 1", {.variant = ButtonVariant::Primary, .icon = Icon("check")}));
    FACET_CHECK_STUB(false, IconButton("Icon button 1", Icon("settings"), {.selected = true}));
    FACET_CHECK_STUB(false, CopyButton("Copy 1", "Sample", {.showLabel = true}));
    FACET_CHECK_STUB(false, Link("Link 1", {.url = "about:blank"}));
    FACET_CHECK_STUB(false, ToggleButtonGroup("Toggles 1", flags, kChoices));
    FACET_CHECK_STUB(false, ToggleButtonGroup("Toggles 2", &v.index, kChoices, {.allowEmpty = true}));

    // Selection
    FACET_CHECK_STUB(false, Toggle("Toggle 1", &v.flag, {.helpText = "Help"}));
    FACET_CHECK_STUB(false, Checkbox("Checkbox 1", &v.flag));
    FACET_CHECK_STUB(false, Checkbox("Checkbox 2", &v.check));
    FACET_CHECK_STUB(false, CheckboxGroup("Checkboxes 1", flags, kChoices, {.selectAll = true}));
    FACET_CHECK_STUB(false, CheckboxGroup("Checkboxes 2", flags, kLabels));
    FACET_CHECK_STUB(false, RadioGroup("Radio 1", &v.index, kChoices));
    FACET_CHECK_STUB(false, RadioGroup("Radio 2", &v.index, kLabels, {.orientation = Orientation::Horizontal}));
    FACET_CHECK_STUB(false, SegmentedControl("Segments 1", &v.index, kChoices));
    FACET_CHECK_STUB(false, SegmentedControl("Segments 2", &v.index, kLabels, {.width = kFill}));
    FACET_CHECK_STUB(false, Chip("Chip 1", {.selected = true, .removable = true}));
    CHECK_FALSE(ChipRemovePressed());
    FACET_CHECK_STUB(false, ChipGroup("Chips 1", flags, kChoices, {.clearAll = true}));
    FACET_CHECK_STUB(false, ChipGroup("Chips 2", flags, kLabels));
    FACET_CHECK_STUB(false, ChipGroup("Chips 3", &v.index, kChoices));
    FACET_CHECK_STUB(false, ChipGroup("Chips 4", &v.index, kLabels, {.removable = true}));
    CHECK(ChipGroupRemovedItem() == -1);
    FACET_CHECK_STUB(false, CycleSelector("Cycle 1", &v.index, kChoices));
    FACET_CHECK_STUB(false, CycleSelector("Cycle 2", &v.index, kLabels, {.pageDots = true}));
    FACET_CHECK_STUB(false, KeybindModeSelector("Mode 1", &v.mode, {.allowOff = false}));
    FACET_CHECK_STUB(false, OptionCardGroup("Cards 1", &v.index, kCards));
    FACET_CHECK_STUB(false, OptionCardGroup("Cards 2", std::span<bool>(v.thumbnailFlags), kCards, {.columns = 2}));

    // Sliders
    FACET_CHECK_STUB(false, Slider("Slider 1", &v.number, 0.0f, 1.0f, {.valueBubble = true}));
    FACET_CHECK_STUB(false, Slider("Slider 2", &v.integer, 0, 10, {.orientation = Orientation::Vertical}));
    FACET_CHECK_STUB(false, SliderStepped("Stepped 1", &v.number, kStops, {.stopLabels = std::span(kLabels)}));
    FACET_CHECK_STUB(false, SliderWithInput("With input 1", &v.number, 0.0f, 1.0f));
    FACET_CHECK_STUB(false, SliderWithInput("With input 2", &v.integer, 0, 10));
    FACET_CHECK_STUB(false, RangeSlider("Range 1", &v.low, &v.high, 0.0f, 1.0f, {.minGap = 0.1, .mirror = true}));
    FACET_CHECK_STUB(false, RangeSlider("Range 2", &v.lowInt, &v.highInt, 0, 10, {.collision = HandleCollision::Push}));
    FACET_CHECK_STUB(false, RangeInput("Range input 1", &v.low, &v.high, 0.0f, 1.0f));
    FACET_CHECK_STUB(false, RangeInput("Range input 2", &v.lowInt, &v.highInt, 0, 10, {.slider = false}));
    FACET_CHECK_STUB(false, RangeSliderHistogram("Histogram 1", &v.low, &v.high, 0.0f, 1.0f, kBins));
    FACET_CHECK_STUB(false, RangeSliderHistogram("Histogram 2", &v.lowInt, &v.highInt, 0, 10, kBins));
    FACET_CHECK_STUB(false, MultiSlider("Multi 1", std::span<float>(v.handles), 0.0f, 1.0f));
    FACET_CHECK_STUB(false, MultiSlider("Multi 2", std::span<int>(v.handlesInt), 0, 10));
    FACET_CHECK_STUB(false, MultiSlider("Multi 3", &v.handleVector, 0.0f, 1.0f, {.allowAdd = true, .allowRemove = true}));
    CHECK(MultiSliderAddedHandle() == -1);
    CHECK(MultiSliderRemovedHandle() == -1);
    FACET_CHECK_STUB(false, Knob("Knob 1", &v.number, 0.0f, 1.0f, {.variant = KnobVariant::Dot}));
    FACET_CHECK_STUB(false, Knob("Knob 2", &v.integer, 0, 10));
    FACET_CHECK_STUB(false, XYPad("Pad 1", &v.point, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), {.grid = true}));
    FACET_CHECK_STUB(false, JoystickPad("Stick 1", &v.point, {.deadzone = 0.1f}));
    FACET_CHECK_STUB(false, ArcRangeSlider("Arc 1", &v.low, &v.high, 0.0f, 360.0f, {.wrap = true, .sweep = 360.0f}));
    FACET_CHECK_STUB(false, BoxRangePad("Box 1", &v.boxLow, &v.boxHigh, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f)));
    FACET_CHECK_STUB(false, AllocationBar("Allocation 1", std::span<float>(v.shares), kChoices));
    FACET_CHECK_STUB(false, AllocationBar("Allocation 2", std::span<float>(v.shares), kLabels, {.minGap = 0.1}));

    // Inputs
    FACET_CHECK_STUB(false, TextField("Text 1", &v.text, {.placeholder = "Placeholder", .clearButton = true}));
    FACET_CHECK_STUB(false, TextField("Text 2", buffer, bufferSize));
    FACET_CHECK_STUB(false, SecureField("Secure 1", &v.text, {.strength = 0.5f}));
    FACET_CHECK_STUB(false, SecureField("Secure 2", buffer, bufferSize));
    FACET_CHECK_STUB(false, SearchField("Search 1", &v.text, {.recent = std::span(kLabels)}));
    FACET_CHECK_STUB(false, SearchField("Search 2", buffer, bufferSize, {.loading = true}));
    FACET_CHECK_STUB(false, FilterField("Filter 1", &v.filter));
    CHECK(PassesFilter(v.filter, "Sample text"));
    FACET_CHECK_STUB(false, Autocomplete("Complete 1", &v.text, kChoices));
    FACET_CHECK_STUB(false, Autocomplete("Complete 2", &v.text, kLabels, {.strict = true}));
    FACET_CHECK_STUB(false, Autocomplete("Complete 3", &v.text, 0, nullptr, nullptr, {.loading = true}));
    FACET_CHECK_STUB(false, Autocomplete("Complete 4", buffer, bufferSize, kChoices));
    FACET_CHECK_STUB(false, Autocomplete("Complete 5", buffer, bufferSize, kLabels));
    FACET_CHECK_STUB(false, Autocomplete("Complete 6", buffer, bufferSize, 0, nullptr, nullptr));
    CHECK(AutocompleteAcceptedItem() == -1);
    FACET_CHECK_STUB(false, TagInput("Tags 1", &v.tags, {.suggestions = std::span(kLabels)}));
    FACET_CHECK_STUB(false, TextArea("Area 1", &v.text, {.maxLines = 6, .counter = true}));
    FACET_CHECK_STUB(false, TextArea("Area 2", buffer, bufferSize));
    FACET_CHECK_STUB(false, InlineRename("Rename 1", &v.text, &v.editing));
    FACET_CHECK_STUB(false, InlineRename("Rename 2", buffer, bufferSize, nullptr, {.selectStem = true}));
    FACET_CHECK_STUB(false, NumberInput("Number 1", &v.integer));
    FACET_CHECK_STUB(false, NumberInput("Number 2", &v.number, {.min = 0.0, .max = 1.0, .step = 0.1}));
    FACET_CHECK_STUB(false, NumberInput("Number 3", &v.real, {.unit = "ms"}));
    FACET_CHECK_STUB(false, DragNumber("Drag 1", &v.integer));
    FACET_CHECK_STUB(false, DragNumber("Drag 2", &v.number, {.speed = 0.01}));
    FACET_CHECK_STUB(false, DragNumber("Drag 3", &v.real, {.wrap = true}));
    FACET_CHECK_STUB(false, UnitInput("Unit 1", &v.real, &v.unit, kUnits));
    FACET_CHECK_STUB(false, VectorInput("Vector 1", std::span<float>(v.vector), {.linkToggle = true}));
    FACET_CHECK_STUB(false, VectorInput("Vector 2", std::span<int>(v.vectorInt)));
    FACET_CHECK_STUB(false, SizeInput("Size 1", &v.size));
    FACET_CHECK_STUB(false, SizeInput("Size 2", &v.sizeX, &v.sizeY, {.swapButton = true}));
    FACET_CHECK_STUB(false, HexInput("Hex 1", &v.hex32, {.prefix = true}));
    FACET_CHECK_STUB(false, HexInput("Hex 2", &v.hex64));
    FACET_CHECK_STUB(false, MaskedInput("Masked 1", &v.text, "9999-9999"));
    FACET_CHECK_STUB(false, MaskedInput("Masked 2", buffer, bufferSize, nullptr));
    FACET_CHECK_STUB(false, PinCodeInput("Pin 1", &v.text, {.masked = true}));
    FACET_CHECK_STUB(false, PinCodeInput("Pin 2", buffer, bufferSize, {.length = 4}));
    FACET_CHECK_STUB(false, DurationInput("Duration 1", &v.seconds, {.showMilliseconds = true}));
    FACET_CHECK_STUB(false, PathField("Path 1", &v.text));
    FACET_CHECK_STUB(false, PathField("Path 2", buffer, bufferSize, {.browseButton = false}));
    CHECK_FALSE(PathFieldBrowsePressed());
    FACET_CHECK_STUB(false, CopyField("Copy field 1", "Sample", {.masked = true}));
    FACET_CHECK_STUB(false, KeybindCapture("Keybind 1", &v.bind, {.showMode = true, .showAlternate = true}));
    FACET_CHECK_STUB(false, Rating("Rating 1", &v.rating, {.halfSteps = true}));
    FACET_CHECK_STUB(false, Rating("Rating 2", &v.ratingInt));
    FACET_CHECK_VOID_STUB(FieldValidation("Message", {.tone = Tone::Warning}));
    FACET_CHECK_STUB(false, ValidationSummary("Summary 1"));

    // Pickers
    FACET_CHECK_STUB(false, ColorWell("Well 1", &v.color, {.alpha = true, .showHex = true}));
    FACET_CHECK_STUB(false, ColorPicker("Picker 1", &v.color, {.original = &v.color, .swatches = kSwatches}));
    FACET_CHECK_STUB(false, ColorInputs("Inputs 1", &v.color, {.model = ColorModel::Hsv}));
    FACET_CHECK_STUB(false, SwatchPalette("Palette 1", &v.color, std::span(kSwatches), &v.history));
    FACET_CHECK_STUB(false, SwatchPalette("Palette 2", &v.color, &v.swatches, nullptr));
    FACET_CHECK_STUB(false, SatValSquare("Square 1", &v.color));
    FACET_CHECK_STUB(false, ColorChannelSlider("Channel 1", &v.color, {.channel = ColorChannel::Alpha}));
    FACET_CHECK_STUB(false, ColorModeEditor("Mode editor 1", &v.animated));
    CHECK(SampleAnimatedColor(v.animated, 1.0).x == v.animated.color.x);
    FACET_CHECK_STUB(false, DatePicker("Date 1", &v.date, {.inlineCalendar = true}));
    FACET_CHECK_STUB(false, DateRangePicker("Dates 1", &v.start, &v.end));
    FACET_CHECK_STUB(false, TimePicker("Time 1", &v.time, {.showSeconds = true}));
    FACET_CHECK_STUB(false, AnglePicker("Angle 1", &v.radians));
    FACET_CHECK_STUB(false, IconPicker("Icon 1", &v.icon));
    FACET_CHECK_STUB(false, IconPicker("Icon 2", &v.icon, kChoices));
    FACET_CHECK_STUB(false, ThumbnailPicker("Thumbnails 1", &v.index, kThumbnails));
    FACET_CHECK_STUB(false, ThumbnailPicker("Thumbnails 2", std::span<bool>(v.thumbnailFlags), kThumbnails,
                                            {.state = DataState::Loading}));
    FACET_CHECK_STUB(false, FileBrowser("Browser 1", &v.text, {.mode = FileBrowserMode::Folder}));
}

// Number of FACET_CHECK_STUB and FACET_CHECK_VOID_STUB calls in CallEveryStub.
constexpr int kStubCalls = 108;

} // namespace

TEST_CASE("Every controls stub returns the neutral result, writes nothing and keeps the stacks")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values values;
    const std::string pristine = Describe(values);
    int frames = 0;
    session.SetUi([&] {
        g_calls = 0;
        CallEveryStub(values);
        CHECK(g_calls == kStubCalls);
        // Again inside a disabled scope, with every id pushed under a scope of its own.
        ImGui::BeginDisabled();
        ImGui::PushID("disabled");
        CallEveryStub(values);
        ImGui::PopID();
        ImGui::EndDisabled();
        ++frames;
    });
    session.Frames(2);
    CHECK(frames == 2);
    CHECK(Describe(values) == pristine);
    // A stub never makes an item active, so Dear ImGui sees no keyboard or mouse capture.
    CHECK_FALSE(ImGui::IsAnyItemActive());
}

TEST_CASE("Controls stubs called outside a frame return the neutral result")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values values;
    const std::string pristine = Describe(values);
    // Between frames: a context exists but no frame and no window.
    CHECK_FALSE(facet::Button("Button 1"));
    CHECK_FALSE(facet::Slider("Slider 1", &values.number, 0.0f, 1.0f));
    CHECK_FALSE(facet::RangeSlider("Range 1", &values.low, &values.high, 0.0f, 1.0f));
    CHECK_FALSE(facet::TextField("Text 1", &values.text));
    CHECK_FALSE(facet::ColorPicker("Picker 1", &values.color));
    facet::FieldValidation("Message");
    CHECK(Describe(values) == pristine);
}

TEST_CASE("A controls stub is one item under the label's id that cannot be operated")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    facet::SetTheme(facet::Theme::Light(), false);
    float value = 0.5f;
    ImGuiID expected = 0;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        expected = ImGui::GetID("Volume##value");
        facet::Slider("Volume##value", &value, 0.0f, 1.0f);
        session.Track("slider");
        facet::Button("Apply", {.variant = facet::ButtonVariant::Primary});
        session.Track("button");
        ImGui::Button("after");
        session.Track("after");
    });
    session.Frame();

    CHECK(session.ItemId("slider") == expected);
    CHECK(session.ItemRect("slider").GetHeight() == doctest::Approx(facet::ControlHeight(facet::ControlSize::Regular)));
    // Tab goes from the stock button before straight to the one after: no stub is a Tab stop.
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("before"));
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("after"));
    // Clicks and drags on the placeholders activate nothing and write nothing.
    session.Click(session.ItemCenter("button"));
    CHECK_FALSE(ImGui::IsAnyItemActive());
    const ImRect slider = session.ItemRect("slider");
    session.Drag({ImVec2(slider.Min.x + 4.0f, slider.GetCenter().y), ImVec2(slider.Max.x - 4.0f, slider.GetCenter().y)});
    CHECK(value == 0.5f);
}

TEST_CASE("Controls stubs take their width in the order of the conventions")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float units = 0.0f;
    float available = 0.0f;
    session.SetUi([&] {
        available = ImGui::GetContentRegionAvail().x;
        units = facet::Px(1.0f);
        facet::TextField("Default", static_cast<std::string*>(nullptr));
        session.Track("default");
        facet::TextField("Units", static_cast<std::string*>(nullptr), {.width = 200.0f});
        session.Track("units");
        facet::TextField("Fill", static_cast<std::string*>(nullptr), {.width = facet::kFill});
        session.Track("fill");
        facet::TextField("Minus", static_cast<std::string*>(nullptr), {.width = -50.0f});
        session.Track("minus");
        ImGui::SetNextItemWidth(150.0f);
        facet::TextField("Next", static_cast<std::string*>(nullptr));
        session.Track("next");
        // The option wins over SetNextItemWidth, and the request does not leak to the next item.
        ImGui::SetNextItemWidth(150.0f);
        facet::TextField("Both", static_cast<std::string*>(nullptr), {.width = 100.0f});
        session.Track("both");
        facet::TextField("After", static_cast<std::string*>(nullptr));
        session.Track("after");
        ImGui::PushItemWidth(120.0f);
        facet::TextField("Pushed", static_cast<std::string*>(nullptr));
        session.Track("pushed");
        ImGui::PopItemWidth();
        // A compact control fits its content: a button is narrower than the row.
        facet::Button("OK");
        session.Track("button");
    });
    session.Frame();

    CHECK(session.ItemRect("default").GetWidth() == doctest::Approx(available));
    CHECK(session.ItemRect("units").GetWidth() == doctest::Approx(200.0f * units));
    CHECK(session.ItemRect("fill").GetWidth() == doctest::Approx(available));
    CHECK(session.ItemRect("minus").GetWidth() == doctest::Approx(available - 50.0f * units));
    CHECK(session.ItemRect("next").GetWidth() == doctest::Approx(150.0f));
    CHECK(session.ItemRect("both").GetWidth() == doctest::Approx(100.0f * units));
    CHECK(session.ItemRect("after").GetWidth() == doctest::Approx(available));
    CHECK(session.ItemRect("pushed").GetWidth() == doctest::Approx(120.0f));
    CHECK(session.ItemRect("button").GetWidth() < available / 2.0f);
}

TEST_CASE("The placeholder text drops the label, then shortens with an ellipsis")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::string wide;
    std::string medium;
    std::string narrow;
    std::string none;
    bool shortened[4] = {};
    float shortWidth = 0.0f;
    session.SetUi([&] {
        facet::PushTextStyle(facet::TextStyle::Footnote);
        char text[facet::internal::kStubCaptionSize];
        const float fullWidth = ImGui::CalcTextSize("RangeSlider: planned (Price)").x;
        shortWidth = ImGui::CalcTextSize("RangeSlider: planned").x;
        shortened[0] = facet::internal::StubCaption("RangeSlider", "Price##range", fullWidth + 1.0f, text, sizeof(text));
        wide = text;
        shortened[1] = facet::internal::StubCaption("RangeSlider", "Price##range", shortWidth + 1.0f, text, sizeof(text));
        medium = text;
        shortened[2] = facet::internal::StubCaption("RangeSlider", "Price##range", shortWidth / 2.0f, text, sizeof(text));
        narrow = text;
        shortened[3] = facet::internal::StubCaption("RangeSlider", "Price##range", 0.0f, text, sizeof(text));
        none = text;
        facet::PopTextStyle();
    });
    session.Frame();

    CHECK(wide == "RangeSlider: planned (Price)");
    CHECK(medium == "RangeSlider: planned");
    // Shortened: a prefix of the name followed by the ellipsis, never the cut-off full text.
    CHECK(narrow.size() > 0);
    CHECK(narrow.size() < medium.size());
    CHECK(narrow.compare(0, 3, "Ran") == 0);
    CHECK(narrow.find("planned") == std::string::npos);
    CHECK(none.empty());
    CHECK_FALSE(shortened[0]);
    CHECK(shortened[1]);
    CHECK(shortened[2]);
    CHECK(shortened[3]);
}

TEST_CASE("A shortened placeholder shows its whole text in a tooltip on hover")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    session.SetUi([&] {
        // An icon button is one control height wide: far too narrow for its text.
        facet::IconButton("Pin##narrow", facet::Icon("pin"));
        session.Track("narrow");
        facet::TextField("Name##wide", static_cast<std::string*>(nullptr));
        session.Track("wide");
    });
    session.Frame();
    const auto tooltipShown = [] {
        ImGuiWindow* tooltip = ImGui::FindWindowByName("##Tooltip_00");
        return tooltip != nullptr && tooltip->Active;
    };
    session.MouseMove(session.ItemCenter("wide"));
    session.Frame();
    CHECK_FALSE(tooltipShown());
    session.MouseMove(session.ItemCenter("narrow"));
    session.Frame();
    CHECK(tooltipShown());
}
