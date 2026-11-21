// Pagination (navigation.h): previous and next buttons around page buttons, with gaps where pages
// are left out ("1 ... 4 5 6 ... 20"), and the selection pill on the current page, sliding with
// the spring of SegmentedControl when the page changes.
//
// Which pages show: all of them up to kPaginationSlots; otherwise always seven slots, the first
// and the last page, the current page with one neighbour on each side, and a gap for each run of
// pages left out (so the row keeps its width while the page changes). A gap is a button that opens
// a small popup with a number field (Strings::goToPage); Enter there writes the page and closes it.
// The compact form shows previous, Strings::pageOf and next, and the widget takes it by itself when
// the full row does not fit the available width.
//
// Keys: the row is one Tab stop; Left and Right, Home and End move the focus between its buttons
// (previous and next are skipped while they cannot move), Enter or Space activates. Writes *page at
// once and returns true then.

#include "facet/widgets/navigation.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "facet/widgets/inputs.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/popup_host.h"
#include "internal/selection_pill.h"
#include "internal/state_storage.h"
#include "widgets/navigation/navigation_parts.h"

namespace facet
{
namespace
{

using namespace internal;

// Slots of page buttons and gaps in the full form when pages are left out.
constexpr int kPaginationSlots = 7;
// The gap's field with its label in front: this many number field widths (metrics.numberFieldWidth).
constexpr float kGoToFieldShare = 2.0f;

struct PaginationState
{
    NavigationRoving roving;
    int goTo = 1;              // the value of the gap field, from 1
    bool fieldFocus = false;   // the gap field takes the keyboard focus in its next frame
};

// One slot of the full form: a page (0-based) or a gap (-1 before the current page, -2 after it).
struct PageSlot
{
    int page = 0;
};

int PageSlots(int page, int pageCount, PageSlot (&slots)[kPaginationSlots])
{
    if (pageCount <= kPaginationSlots)
    {
        for (int i = 0; i < pageCount; ++i)
            slots[i].page = i;
        return pageCount;
    }
    const int last = pageCount - 1;
    int n = 0;
    if (page <= 3)
    {
        for (int i = 0; i < 5; ++i)
            slots[n++].page = i;
        slots[n++].page = -2;
        slots[n++].page = last;
    }
    else if (page >= last - 3)
    {
        slots[n++].page = 0;
        slots[n++].page = -1;
        for (int i = last - 4; i <= last; ++i)
            slots[n++].page = i;
    }
    else
    {
        slots[n++].page = 0;
        slots[n++].page = -1;
        for (int i = page - 1; i <= page + 1; ++i)
            slots[n++].page = i;
        slots[n++].page = -2;
        slots[n++].page = last;
    }
    return n;
}

// Strings::pageOf with its tokens replaced.
void FormatPageOf(int page, int count, char* out, int size)
{
    char pageText[16];
    char countText[16];
    std::snprintf(pageText, sizeof(pageText), "%d", page);
    std::snprintf(countText, sizeof(countText), "%d", count);
    const char* src = GetStrings().pageOf != nullptr ? GetStrings().pageOf : "";
    int n = 0;
    while (*src != '\0' && n < size - 1)
    {
        const char* token = nullptr;
        int skip = 0;
        if (std::strncmp(src, "{page}", 6) == 0)
        {
            token = pageText;
            skip = 6;
        }
        else if (std::strncmp(src, "{count}", 7) == 0)
        {
            token = countText;
            skip = 7;
        }
        if (token != nullptr)
        {
            for (const char* p = token; *p != '\0' && n < size - 1; ++p)
                out[n++] = *p;
            src += skip;
        }
        else
        {
            out[n++] = *src++;
        }
    }
    out[n] = '\0';
}

struct PageButton
{
    ImGuiID id = 0;
    ImRect rect;
    int target = -1;      // the page a press goes to; -1 for a gap
    bool enabled = true;
    int part = 0;
};

} // namespace

bool Pagination(const char* id, int* page, int pageCount, const PaginationOptions& options)
{
    ImGuiWindow* window = ControlWindow();
    if (window == nullptr)
        return false;
    if (id == nullptr)
        id = "";
    const NextItemData next = TakeNextItemData();
    const ForcedItemRequest forced = TakeForcedItemState();
    const ControlMetrics m = GetControlMetrics(options.size);
    const Theme& t = T();
    const Colors& c = t.colors;
    const ImGuiID rowId = window->GetID(id);
    PaginationState& state = GetState<PaginationState>(rowId);
    const bool readOnly = page == nullptr || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    const bool groupDisabled = (next.itemFlags & ImGuiItemFlags_Disabled) != 0;
    const int count = std::max(0, pageCount);
    // Shown clamped; written only by a user edit (rule 14).
    const int current = count > 0 && page != nullptr ? std::clamp(*page, 0, count - 1) : 0;

    ControlTextScope text(m);
    const float iconSize = text.IconSize();
    const float gap = Px(t.spacing.xs);
    const float button = m.height;

    // ---- Layout: the full form when it fits, else the compact one ----
    PageSlot slots[kPaginationSlots];
    const int slotCount = count > 0 ? PageSlots(current, count, slots) : 0;
    char label[32];
    float fullWidth = button * 2.0f + gap * 2.0f;
    PushPillLabelStyle(m.textStyle, true);
    float pageWidths[kPaginationSlots] = {};
    for (int i = 0; i < slotCount; ++i)
    {
        float w = button;
        if (slots[i].page >= 0)
        {
            std::snprintf(label, sizeof(label), "%d", slots[i].page + 1);
            w = std::max(button, std::ceil(ImGui::CalcTextSize(label).x + m.paddingX));
        }
        pageWidths[i] = w;
        fullWidth += w + (i > 0 ? gap : 0.0f);
    }
    PopTextStyle();
    char pageOf[96];
    FormatPageOf(count > 0 ? current + 1 : 0, count, pageOf, sizeof(pageOf));
    const float pageOfWidth = std::ceil(ImGui::CalcTextSize(pageOf).x + m.paddingX * 2.0f);
    const float compactWidth = button * 2.0f + pageOfWidth;
    const float available = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    const bool compact = options.compact || count == 0 || fullWidth > available + 0.5f;
    const float width = ResolveItemWidth(0.0f, next, compact ? std::min(compactWidth, available) : fullWidth);

    const ImVec2 origin(std::round(window->DC.CursorPos.x), std::round(window->DC.CursorPos.y));
    ImGui::BeginGroup();
    ImGui::ItemSize(ImVec2(width, m.height), CenteredTextY(origin.y, m.height) - origin.y);
    ImDrawList* drawList = window->DrawList;
    state.roving.BeginFrame();

    PageButton buttons[kPaginationSlots + 2];
    int buttonCount = 0;
    float x = origin.x;
    const auto add = [&](ImGuiID buttonId, float w, int target, bool enabled, int part) {
        PageButton& b = buttons[buttonCount++];
        b.id = buttonId;
        b.rect = ImRect(std::round(x), origin.y, std::round(x + w), origin.y + m.height);
        b.target = target;
        b.enabled = enabled && !groupDisabled;
        b.part = part;
        x = b.rect.Max.x + gap;
    };
    add(ImGui::GetIDWithSeed("previous", nullptr, rowId), button, current - 1, current > 0, kNavigationPartPrevious);
    float pageOfX = 0.0f;
    if (compact)
    {
        x -= gap;
        pageOfX = x;
        x += std::max(0.0f, width - button * 2.0f);
    }
    else
    {
        for (int i = 0; i < slotCount; ++i)
        {
            const int p = slots[i].page;
            if (p >= 0)
                add(ImHashData(&p, sizeof(p), rowId), pageWidths[i], p, true, p);
            else
                add(ImGui::GetIDWithSeed(p == -1 ? "gap before" : "gap after", nullptr, rowId), pageWidths[i], -1, true,
                    kNavigationPartGapBase + (p == -1 ? 0 : 1));
        }
    }
    add(ImGui::GetIDWithSeed("next", nullptr, rowId), button, current + 1, count > 0 && current < count - 1,
        kNavigationPartNext);

    SelectionPill pill;
    pill.Begin(rowId, drawList, ImVec2(window->Pos.x - window->Scroll.x, window->Pos.y - window->Scroll.y));
    int chosen = -1;
    ImGuiID editor = rowId;
    int openedGap = -1;
    ImRect gapRect;
    for (int i = 0; i < buttonCount; ++i)
    {
        const PageButton& b = buttons[i];
        ItemOptions itemOptions;
        itemOptions.disabled = !b.enabled;
        itemOptions.readOnly = readOnly;
        itemOptions.tabStop = state.roving.IsTabStop(b.id);
        itemOptions.part = i;
        itemOptions.forced = &forced;
        itemOptions.repeat = b.part == kNavigationPartPrevious || b.part == kNavigationPartNext;
        const ItemState item = PartBehavior(b.id, b.rect, itemOptions);
        state.roving.Add(b.id, !item.disabled, b.target == current && b.part < kNavigationPartMore);
        NavigationRecordPart(rowId, b.part, b.rect);
        if (item.focused && item.forced == ForcedItemState::None)
            (void)state.roving.Step(b.id, false);
        const bool live = item.forced == ForcedItemState::None;
        if (item.pressed && live && !item.readOnly)
        {
            if (b.target >= 0)
            {
                chosen = b.target;
                editor = b.id;
            }
            else
            {
                openedGap = i;
            }
        }
        const bool isPage = b.part < kNavigationPartMore;
        const bool on = isPage && b.target == current && !compact;
        if (on)
            pill.Target(b.rect);
        if (b.target < 0)
            gapRect = b.rect;
        if (!item.visible)
            continue;
        const float feedback = item.disabled || item.readOnly ? 0.0f : std::max(item.hoverAmount, item.pressAmount);
        if (feedback > 0.0f)
        {
            const ImVec4 tint = Mix(c.fill[2], c.fill[1], item.pressAmount);
            FillRect(drawList, b.rect.Min, b.rect.Max, ToU32(WithAlpha(tint, tint.w * feedback)), m.radius);
        }
        const ImU32 ink = ToU32(ControlLabelColor(item.disabled));
        if (b.part == kNavigationPartPrevious || b.part == kNavigationPartNext)
        {
            DrawControlIcon(drawList, Icon(b.part == kNavigationPartPrevious ? "chevron-left" : "chevron-right"),
                            std::round(b.rect.GetCenter().x - iconSize * 0.5f), b.rect.GetCenter().y, iconSize, ink);
        }
        else if (b.target < 0)
        {
            DrawControlIcon(drawList, Icon("more-horizontal"), std::round(b.rect.GetCenter().x - iconSize * 0.5f),
                            b.rect.GetCenter().y, iconSize, ToU32(ControlSecondaryColor(item.disabled)));
        }
        else
        {
            std::snprintf(label, sizeof(label), "%d", b.target + 1);
            PushPillLabelStyle(m.textStyle, on);
            const float w = ImGui::CalcTextSize(label).x;
            drawList->AddText(ImVec2(std::round(b.rect.GetCenter().x - w * 0.5f), CenteredTextY(b.rect.Min.y, m.height)), ink, label);
            PopTextStyle();
        }
        DrawFocusRing(drawList, b.rect, m.radius, item.focusAmount);
        // Previous, next and the gaps are glyphs: their names show as tooltips.
        const char* name = b.part == kNavigationPartPrevious ? GetStrings().previous
                         : (b.part == kNavigationPartNext ? GetStrings().next : (b.target < 0 ? GetStrings().goToPage : nullptr));
        if (name != nullptr && BeginPopupTooltip(b.id, b.rect, item.hovered, item.focusVisible))
        {
            ImGui::TextUnformatted(name);
            EndPopupTooltip();
        }
    }
    pill.End(ToU32(groupDisabled ? c.fill[1] : c.accentSubtle), m.radius);
    if (compact)
    {
        const float w = ImGui::CalcTextSize(pageOf).x;
        const float room = std::max(0.0f, width - button * 2.0f);
        const FittedText shown = FitText(pageOf, nullptr, room);
        drawList->AddText(ImVec2(std::round(pageOfX + (room - std::min(w, room)) * 0.5f), CenteredTextY(origin.y, m.height)),
                          ToU32(ControlLabelColor(groupDisabled)), shown.text);
    }

    // ---- The gap's field ----
    const ImGuiID fieldLevel = ImGui::GetIDWithSeed("go to", nullptr, rowId);
    if (openedGap >= 0)
    {
        state.goTo = current + 1;
        state.fieldFocus = true;
        OpenPopupLevel(fieldLevel, {buttons[openedGap].rect, PopupSide::Below, PopupAlign::Center});
    }
    if (BeginPopupLevel(fieldLevel))
    {
        if (state.fieldFocus)
        {
            ImGui::SetKeyboardFocusHere();
            state.fieldFocus = false;
        }
        NumberInputOptions field;
        field.min = 1.0;
        field.max = static_cast<double>(std::max(1, count));
        field.step = 0.0;
        field.size = options.size;
        field.width = t.metrics.numberFieldWidth * kGoToFieldShare;
        char fieldLabel[96];
        std::snprintf(fieldLabel, sizeof(fieldLabel), "%s##field", GetStrings().goToPage);
        if (NumberInput(fieldLabel, &state.goTo, field) && !readOnly && count > 0)
        {
            chosen = std::clamp(state.goTo - 1, 0, count - 1);
            ClosePopupLevel(fieldLevel);
        }
        NavigationRecordPart(rowId, kNavigationPartFilter, GImGui->LastItemData.Rect);
        EndPopupLevel();
    }
    (void)gapRect;

    window->DC.CursorMaxPos = ImMax(window->DC.CursorMaxPos, ImVec2(origin.x + width, origin.y + m.height));
    ImGui::EndGroup();
    FinishWidgetGroup(rowId);
    if (readOnly || groupDisabled || chosen < 0 || chosen == current || page == nullptr)
        return false;
    *page = chosen;
    ImGui::MarkItemEdited(editor);
    return true;
}

} // namespace facet
