// TagInput: a field frame holding the tags as chips (facet::Chip from selection.h) followed by a
// frameless text field on the text field core for the next entry. The chips wrap into more rows
// and the frame grows with them. The text field keeps the keyboard: Enter or a comma adds the
// typed text, Backspace in the empty field selects and then removes the last chip, Left and Right
// move between chips, Delete removes the selected chip, and the suggestions that match the typed
// text open in a completion list under the frame (text_inputs_shared.h).

#include "facet/widgets/inputs.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string_view>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/anim.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "facet/widgets/selection.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/fuzzy.h"
#include "internal/item.h"
#include "internal/state_storage.h"
#include "internal/text_field.h"
#include "widgets/inputs/text_inputs_shared.h"

namespace facet
{
namespace
{

// The narrowest the text part may get before it moves to a row of its own, in control heights.
constexpr float kTagInputMinTextHeights = 3.0f;

struct TagInputMatch
{
    int index = 0;
    int score = 0;
    int rangeStart = 0;
    int rangeCount = 0;
};

// Kept under the widget id. The vectors grow to the largest size seen and are reused.
struct TagInputState
{
    std::string text;                      // the entry being typed
    int selected = -1;                     // the selected chip, -1 for none
    ImVector<float> chipWidths;            // pixels, measured in the last frame (0: not measured yet)
    ImVector<TagInputMatch> matches;       // suggestions that match the text, best first
    ImVector<internal::TextRange> ranges;  // their matched ranges
    bool active = false;                   // the text had the keyboard in the last frame
};

// ASCII case-insensitive equality.
bool TagInputSameIgnoringCase(std::string_view a, std::string_view b)
{
    if (a.size() != b.size())
        return false;
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        const char x = (a[i] >= 'A' && a[i] <= 'Z') ? static_cast<char>(a[i] - 'A' + 'a') : a[i];
        const char y = (b[i] >= 'A' && b[i] <= 'Z') ? static_cast<char>(b[i] - 'A' + 'a') : b[i];
        if (x != y)
            return false;
    }
    return true;
}

std::string_view TagInputTrim(std::string_view text)
{
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t'))
        text.remove_prefix(1);
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t'))
        text.remove_suffix(1);
    return text;
}

bool TagInputHasTag(const std::vector<std::string>& tags, std::string_view tag)
{
    for (const std::string& existing : tags)
        if (TagInputSameIgnoringCase(existing, tag))
            return true;
    return false;
}


// Adds `entry` (trimmed) to `tags` when the options allow it. Returns true when a tag was added.
bool TagInputAdd(std::vector<std::string>& tags, TagInputState& state, std::string_view entry,
                 const TagInputOptions& options)
{
    entry = TagInputTrim(entry);
    if (entry.empty())
        return false;
    if (options.maxTags > 0 && static_cast<int>(tags.size()) >= options.maxTags)
        return false;
    if (!options.allowCreate)
    {
        // Only a suggestion may become a tag, in the suggestion's spelling.
        std::string_view found;
        for (const char* suggestion : options.suggestions)
            if (suggestion != nullptr && TagInputSameIgnoringCase(suggestion, entry))
                found = suggestion;
        if (found.empty())
            return false;
        entry = found;
    }
    if (!options.allowDuplicates && TagInputHasTag(tags, entry))
        return false;
    try
    {
        tags.emplace_back(entry);
        state.chipWidths.push_back(0.0f);
    }
    catch (...)
    {
        return false;  // the old tags stay
    }
    return true;
}

// The suggestions that match the typed text (fuzzy, best first), without the tags already there
// unless duplicates are allowed.
void TagInputFilter(TagInputState& state, const std::vector<std::string>& tags, const TagInputOptions& options)
{
    state.matches.resize(0);
    state.ranges.resize(0);
    const std::string_view query = TagInputTrim(state.text);
    if (query.empty())
        return;
    internal::TextRange ranges[internal::kFuzzyMaxRanges];
    for (int i = 0; i < static_cast<int>(options.suggestions.size()); ++i)
    {
        const char* suggestion = options.suggestions[static_cast<std::size_t>(i)];
        if (suggestion == nullptr || (!options.allowDuplicates && TagInputHasTag(tags, suggestion)))
            continue;
        const internal::FuzzyMatch scored = internal::FuzzyScore(query, suggestion, ranges);
        if (!scored.matched)
            continue;
        TagInputMatch match;
        match.index = i;
        match.score = scored.score;
        match.rangeStart = state.ranges.Size;
        match.rangeCount = std::min(scored.rangeCount, internal::kFuzzyMaxRanges);
        for (int r = 0; r < match.rangeCount; ++r)
            state.ranges.push_back(ranges[r]);
        state.matches.push_back(match);
    }
    std::stable_sort(state.matches.begin(), state.matches.end(),
                     [](const TagInputMatch& a, const TagInputMatch& b) { return a.score > b.score; });
}

struct TagInputRows
{
    const TagInputState* state = nullptr;
    std::span<const char* const> suggestions;
};

void TagInputRowEntry(void* user, int row, internal::text_inputs::CompletionEntry* out)
{
    const TagInputRows& rows = *static_cast<const TagInputRows*>(user);
    const TagInputMatch& match = rows.state->matches[row];
    out->label = rows.suggestions[static_cast<std::size_t>(match.index)];
    if (match.rangeCount > 0)
        out->matches = std::span<const internal::TextRange>(rows.state->ranges.Data + match.rangeStart,
                                                            static_cast<std::size_t>(match.rangeCount));
}

// The width of a chip before it was measured: its text, its paddings and the remove button.
float TagInputChipEstimate(const std::string& tag, ControlSize chipSize, bool removable)
{
    const internal::ControlMetrics m = internal::GetControlMetrics(chipSize);
    const internal::ControlTextScope text(m);
    const float remove = removable ? text.IconSize() + m.iconGap : 0.0f;
    return std::round(m.paddingX * 2.0f + ImGui::CalcTextSize(tag.c_str()).x + remove);
}

} // namespace

bool TagInput(const char* label, std::vector<std::string>* tags, const TagInputOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    ImGuiContext& g = *GImGui;
    static const std::vector<std::string> kNoTags;
    const std::vector<std::string>& shownTags = tags != nullptr ? *tags : kNoTags;
    const ImGuiID id = window->GetID(label);
    TagInputState& state = internal::GetState<TagInputState>(id);
    const internal::NextItemData next = internal::TakeNextItemData();
    const bool disabled = (next.itemFlags & ImGuiItemFlags_Disabled) != 0;
    const bool readOnly = options.readOnly || tags == nullptr || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    const int count = static_cast<int>(shownTags.size());
    const bool full = options.maxTags > 0 && count >= options.maxTags;
    if (state.chipWidths.Size != count)
    {
        state.chipWidths.resize(count);
        std::fill(state.chipWidths.begin(), state.chipWidths.end(), 0.0f);
    }
    if (state.selected >= count)
        state.selected = -1;

    // ---- Geometry ----
    const Theme& t = T();
    const internal::ControlMetrics metrics = internal::GetControlMetrics(options.size);
    const ControlSize chipSize = metrics.size == ControlSize::Large ? ControlSize::Regular : ControlSize::Small;
    const float chipHeight = ControlHeight(chipSize);
    const float gap = Px(t.spacing.xs);
    const float padX = metrics.fieldPadding;
    const float padY = std::max(0.0f, std::round((metrics.height - chipHeight) * 0.5f));
    const float totalWidth = internal::ResolveItemWidth(options.width, next, kFill);
    const internal::FrontLabel front = internal::MeasureFrontLabel(label, metrics.size, totalWidth);
    const ImVec2 origin = window->DC.CursorPos;
    const float frameLeft = origin.x + front.width;
    const float contentWidth = std::max(1.0f, origin.x + totalWidth - frameLeft - padX * 2.0f);

    // Flow the chips, then the text part, into rows.
    const bool removable = !readOnly && !disabled;
    float x = 0.0f;
    int row = 0;
    for (int i = 0; i < count; ++i)
    {
        float& width = state.chipWidths[i];
        if (width <= 0.0f)
            width = TagInputChipEstimate(shownTags[static_cast<std::size_t>(i)], chipSize, removable);
        if (x > 0.0f && x + width > contentWidth)
        {
            ++row;
            x = 0.0f;
        }
        x += std::min(width, contentWidth) + gap;
    }
    const float minText = std::min(contentWidth, metrics.height * kTagInputMinTextHeights);
    if (x > 0.0f && x + minText > contentWidth)
    {
        ++row;
        x = 0.0f;
    }
    const int rows = row + 1;
    const float frameHeight = padY * 2.0f + chipHeight * static_cast<float>(rows) + gap * static_cast<float>(rows - 1);
    const ImRect frame(ImVec2(frameLeft, origin.y), ImVec2(origin.x + totalWidth, origin.y + frameHeight));
    auto rowTop = [&](int r) { return frame.Min.y + padY + (chipHeight + gap) * static_cast<float>(r); };

    ImGui::BeginGroup();
    ImDrawList* drawList = window->DrawList;
    const bool hovered = !disabled && ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(frame.Min, frame.Max);
    const float hoverAmount = Tween(ImGui::GetIDWithSeed("frame", nullptr, id), 0, hovered ? 1.0f : 0.0f,
                                    t.motion.durationFast);
    internal::DrawFieldFrame(drawList, frame,
                             {.hoverAmount = hoverAmount, .active = state.active, .disabled = disabled,
                              .readOnly = readOnly, .error = options.error != nullptr && options.error[0] != '\0'});
    if (front.text != nullptr)
        internal::DrawFrontLabel(drawList, front, ImVec2(origin.x, CenteredTextY(frame.Min.y, metrics.height)),
                                 metrics.size, disabled);

    // ---- Chips ----
    int removeIndex = -1;
    int pickedIndex = -1;
    x = 0.0f;
    row = 0;
    ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);  // the text keeps the keyboard; the keys select chips
    for (int i = 0; i < count; ++i)
    {
        const float width = std::min(state.chipWidths[i], contentWidth);
        if (x > 0.0f && x + width > contentWidth)
        {
            ++row;
            x = 0.0f;
        }
        ImGui::SetCursorScreenPos(ImVec2(frame.Min.x + padX + x, rowTop(row)));
        // A chip wider than the frame is cut to it (the chip shortens its label); its own width is
        // kept, so it grows back when the frame does.
        const bool constrained = state.chipWidths[i] > contentWidth;
        if (constrained)
            ImGui::SetNextItemWidth(contentWidth);
        // Whatever width the chip takes, it never draws or reaches past the frame.
        const ImVec2 maxBefore = window->DC.CursorMaxPos;
        ImGui::PushClipRect(frame.Min, frame.Max, true);
        ImGui::PushID(i);
        if (Chip(shownTags[static_cast<std::size_t>(i)].c_str(),
                 {.selected = i == state.selected, .removable = removable, .size = chipSize}))
            pickedIndex = i;
        if (ChipRemovePressed())
            removeIndex = i;
        ImGui::PopID();
        ImGui::PopClipRect();
        window->DC.CursorMaxPos.x = std::max(maxBefore.x, std::min(window->DC.CursorMaxPos.x, frame.Max.x));
        const float measured = ImGui::GetItemRectSize().x;
        if (measured > 0.0f && !constrained)
            state.chipWidths[i] = measured;
        x += width + gap;
    }
    ImGui::PopItemFlag();
    if (x > 0.0f && x + minText > contentWidth)
    {
        ++row;
        x = 0.0f;
    }

    // ---- The text part ----
    internal::text_inputs::CompletionList list(id);
    list.BeforeField();
    int caretBefore = 0;
    int selStart = 0;
    int selEnd = 0;
    const bool hadSelection = internal::TextFieldGetSelection(id, &selStart, &selEnd, &caretBefore);
    const bool wasEmpty = state.text.empty();
    internal::TextFieldParams params;
    params.placeholder = count == 0 ? options.placeholder : nullptr;
    params.frameless = true;
    params.labelHidden = true;
    params.completionPopup = list.Level();
    params.size = options.size;
    params.width = std::max(1.0f, contentWidth - x) / Px(1.0f);
    params.readOnly = readOnly || full;
    const float textRowCenter = rowTop(row) + chipHeight * 0.5f;
    ImGui::SetCursorScreenPos(ImVec2(frame.Min.x + padX + x, std::round(textRowCenter - metrics.height * 0.5f)));
    const internal::TextFieldResult field = internal::TextFieldBehavior(label, &state.text, params);
    state.active = field.active;

    // ---- Keys ----
    bool written = false;
    std::vector<std::string>* target = readOnly ? nullptr : tags;
    if (target != nullptr && field.active)
    {
        const bool caretAtStart = !hadSelection || (selStart == selEnd && caretBefore == 0);
        if (ImGui::IsKeyPressed(ImGuiKey_Backspace, true) && wasEmpty)
        {
            if (state.selected >= 0)
                removeIndex = state.selected;
            else if (count > 0)
                state.selected = count - 1;
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_Delete, true) && state.selected >= 0)
        {
            removeIndex = state.selected;
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, true) && count > 0)
        {
            if (state.selected > 0)
                state.selected -= 1;
            else if (state.selected < 0 && caretAtStart)
                state.selected = count - 1;
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, true) && state.selected >= 0)
        {
            state.selected = state.selected + 1 < count ? state.selected + 1 : -1;
        }
        if (g.IO.InputQueueCharacters.Size > 0 || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            state.selected = -1;
    }
    else if (!field.active && g.NavId != id && pickedIndex < 0)
    {
        state.selected = -1;
    }

    // Enter or a comma adds the typed text; Enter ends the session, so the keyboard comes back.
    // Every comma in the text (typed or pasted) ends an entry. An entry the options refuse stays
    // in the text, without its comma, so the user sees it was not added.
    if (target != nullptr && field.edited && state.text.find(',') != std::string::npos)
    {
        try
        {
            std::string kept;
            std::size_t start = 0;
            for (std::size_t comma = state.text.find(','); comma != std::string::npos;
                 comma = state.text.find(',', start))
            {
                const std::string_view entry(state.text.data() + start, comma - start);
                if (TagInputAdd(*target, state, entry, options))
                    written = true;
                else if (kept.empty())
                    kept.assign(TagInputTrim(entry));
                start = comma + 1;
            }
            kept.append(state.text, start, std::string::npos);
            state.text.swap(kept);
        }
        catch (...)
        {
            // Allocation failed: the text stays as typed.
        }
    }

    // Enter with nothing typed ends the session as in any field.
    if (target != nullptr && field.submitted)
    {
        const bool hadEntry = !TagInputTrim(state.text).empty();
        if (TagInputAdd(*target, state, state.text, options))
        {
            state.text.clear();
            written = true;
        }
        if (field.submitted && hadEntry)
            internal::TextFieldRequestFocus(id, internal::TextFieldFocus::End);
    }

    // The suggestions under the frame while typing.
    if (field.active)
        TagInputFilter(state, shownTags, options);
    TagInputRows rowsData{&state, options.suggestions};
    internal::text_inputs::CompletionDesc desc;
    desc.count = state.matches.Size;
    desc.entry = TagInputRowEntry;
    desc.user = &rowsData;
    desc.wanted = field.active && target != nullptr && !full && state.matches.Size > 0;
    desc.size = options.size;
    desc.forcedOpen = Has(field.forced, internal::ForcedItemState::Open) && state.matches.Size > 0;
    internal::TextFieldResult anchor = field;
    anchor.frame = frame;
    const int accepted = list.Run(anchor, desc);
    if (accepted >= 0 && accepted < state.matches.Size && target != nullptr)
    {
        const char* suggestion = options.suggestions[static_cast<std::size_t>(state.matches[accepted].index)];
        if (TagInputAdd(*target, state, suggestion, options))
        {
            state.text.clear();
            written = true;
        }
    }

    // A chip removed by its button or by the keys; a click on a chip selects it.
    if (target != nullptr && removeIndex >= 0 && removeIndex < static_cast<int>(target->size()))
    {
        target->erase(target->begin() + removeIndex);
        state.chipWidths.erase(state.chipWidths.begin() + removeIndex);
        state.selected = -1;
        written = true;
        internal::TextFieldRequestFocus(id, internal::TextFieldFocus::End);
    }
    else if (pickedIndex >= 0 && !disabled)
    {
        state.selected = pickedIndex;
        internal::TextFieldRequestFocus(id, internal::TextFieldFocus::End);
    }

    // A click on the frame between the chips and the text goes to the text, as in one field.
    if (hovered && !readOnly && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && g.HoveredId == 0 && !field.active)
        internal::TextFieldRequestFocus(id, internal::TextFieldFocus::End);

    const bool forcedFocus = Has(field.forced, internal::ForcedItemState::Focused);
    if (((g.NavId == id && internal::KeyboardModality()) || forcedFocus) && !disabled)
        internal::DrawFocusRing(drawList, frame, metrics.radius);

    // The whole frame is the widget's space; the message line goes under it.
    ImGui::SetCursorScreenPos(origin);
    ImGui::ItemSize(ImRect(origin, frame.Max));
    const internal::ControlMessage message = internal::PickControlMessage(options.error, options.helpText);
    const float bottom = internal::DrawControlMessage(message, ImVec2(frame.Min.x, frame.Max.y), frame.GetWidth(),
                                                      disabled, id, frame);
    window->DC.CursorMaxPos.y = std::max(window->DC.CursorMaxPos.y, bottom);
    ImGui::EndGroup();
    internal::FinishWidgetGroup(id);
    return written;
}

} // namespace facet
