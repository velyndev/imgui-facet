// Demo page: Popup host. Prototypes built on the popup host (src/internal/popup_host.h) and the
// interaction blocks (item behaviour, focus ring, selection pill): a dropdown-like list, a menu
// with two levels of submenus, a context menu, a glass popover with controls, tooltips, a modal
// with a confirm level over it, and placement samples at the display edges. They show the block
// working; the finished menus, popovers and dialogs are on the Navigation and Overlays pages.

#include <algorithm>
#include <cstdio>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "page_helpers.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/popup_host.h"
#include "internal/popup_menu.h"
#include "internal/selection_pill.h"
#include "pages/pages.h"

namespace
{

namespace fi = facet::internal;
using facet::demo::ForcedItemState;
using facet::demo::Note;
using facet::demo::NoteBeside;
using facet::demo::PageCtx;
using facet::demo::SectionHeader;

// The capture states, in the order of the FACET_DEMO_PAGE line below.
enum CaptureState
{
    kDefault,
    kDropdown,
    kMenu,
    kContext,
    kPopover,
    kTooltip,
    kModal,
    kPlacement,
    kClosing,
};

const char* const kSizes[] = {"Small", "Medium", "Large", "Extra large"};
constexpr int kSizeCount = 4;
constexpr int kLongRows = 40;
constexpr int kSampleCount = 8;

struct PopupPageState
{
    int size = 1;
    int longRow = -1;
    int counter = 0;
    char lastAction[48] = "none";
    bool grid = true;
    double opacity = 0.6;
    bool samples = false;
    int confirmed = 0;
    int kept = 0;
    int scrimClicks = 0;
    fi::PopupCloseReason lastReason = fi::PopupCloseReason::None;
    int align = 0;  // the alignment chosen in the third menu level
    bool closingStarted = false;
    ImGuiID toolIds[4] = {};  // the tooltip buttons, for the selftest
};

// What the host reported in the last frame the page was drawn. Selftest checks run between
// frames, where the host's calls give their neutral result, so they read this instead.
struct Observed
{
    bool dropdown = false;
    bool longList = false;
    ImRect longRect;
    bool menu[3] = {};
    bool context = false;
    bool popover = false;
    bool modal = false;
    bool confirm = false;
    ImGuiID top = 0;
    bool tip[4] = {};
    ImRect tipRect[4];
};

PopupPageState page;
Observed seen;

// Level ids, taken on the page's id stack every frame (the level windows have id stacks of their own).
struct Ids
{
    ImGuiID dropdown = 0;
    ImGuiID longList = 0;
    ImGuiID menu[3] = {};
    ImGuiID context = 0;
    ImGuiID popover = 0;
    ImGuiID modal = 0;
    ImGuiID confirm = 0;
    ImGuiID samples[kSampleCount] = {};
};

Ids ids;

void TakeIds()
{
    ids.dropdown = ImGui::GetID("dropdown level");
    ids.longList = ImGui::GetID("long list level");
    ids.menu[0] = ImGui::GetID("menu level 1");
    ids.menu[1] = ImGui::GetID("menu level 2");
    ids.menu[2] = ImGui::GetID("menu level 3");
    ids.context = ImGui::GetID("context level");
    ids.popover = ImGui::GetID("popover level");
    ids.modal = ImGui::GetID("modal level");
    ids.confirm = ImGui::GetID("confirm level");
    for (int i = 0; i < kSampleCount; ++i)
        ids.samples[i] = ImGui::GetID(i + 1000);
}

ImU32 Col(const ImVec4& color)
{
    return facet::ToU32(color);
}

bool Capturing(const PageCtx& ctx, int state)
{
    return ctx.capture && ctx.captureState == state;
}

const char* SideName(fi::PopupSide side)
{
    switch (side)
    {
    case fi::PopupSide::Below:
        return "below";
    case fi::PopupSide::Above:
        return "above";
    case fi::PopupSide::After:
        return "after";
    case fi::PopupSide::Before:
        return "before";
    }
    return "below";
}

fi::PopupAnchor AnchorOf(const ImRect& rect, fi::PopupSide side, bool matchWidth = false)
{
    fi::PopupAnchor anchor;
    anchor.rect = rect;
    anchor.side = side;
    anchor.matchWidth = matchWidth;
    return anchor;
}

// Opens `id`, or shows it for this frame when the item carries the forced Open state (captures).
void OpenOrForce(const fi::ItemState& item, ImGuiID id, const fi::PopupAnchor& anchor, const fi::PopupOptions& options)
{
    if (item.open)
        fi::ForcePopupLevelOpen(id, anchor, options);
    else
        fi::OpenPopupLevel(id, anchor, options);
}

// The fill of a control from its hover and press amounts.
ImVec4 ControlFill(const fi::ItemState& item, bool active)
{
    const facet::Colors& c = facet::T().colors;
    if (item.disabled)
        return c.fill[3];
    const ImVec4 hovered = facet::Mix(c.fill[1], c.fill[0], std::max(item.hoverAmount, active ? 1.0f : 0.0f) * 0.5f);
    return facet::Mix(hovered, c.fill[0], item.pressAmount);
}

// The width of a ProtoAnchor with this label, for row wrapping.
float AnchorWidth(const char* label, bool icon)
{
    const facet::Theme& t = facet::T();
    facet::PushTextStyle(facet::TextStyle::Body);
    const float width = ImGui::CalcTextSize(label, nullptr, true).x + facet::Px(t.spacing.md) * 2.0f +
                        (icon ? facet::Px(t.type[facet::TextStyle::Body].size) + facet::Px(t.spacing.sm) : 0.0f);
    facet::PopTextStyle();
    return width;
}

// A push button prototype on the item behaviour block, with an optional trailing icon. `active`
// keeps the pressed look while the level it opened is shown.
fi::ItemState ProtoAnchor(PageCtx& ctx, const char* label, const char* probe, bool active, const char* icon = nullptr)
{
    const facet::Theme& t = facet::T();
    const facet::Colors& c = t.colors;
    const char* end = ImGui::FindRenderedTextEnd(label);
    facet::PushTextStyle(facet::TextStyle::Body);
    const float padding = facet::Px(t.spacing.md);
    const float height = facet::ControlHeight(facet::ControlSize::Regular);
    const float iconSize = facet::Px(t.type[facet::TextStyle::Body].size);
    const float iconSpace = icon != nullptr ? iconSize + facet::Px(t.spacing.sm) : 0.0f;
    const ImVec2 size(ImGui::CalcTextSize(label, end).x + padding * 2.0f + iconSpace, height);
    const fi::ItemState item = fi::ItemBehavior(ImGui::GetID(label), size);
    ctx.Probe(probe);
    if (item.visible)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const float radius = facet::Px(t.radii.control);
        facet::FillRect(drawList, item.rect.Min, item.rect.Max, Col(ControlFill(item, active || item.open)), radius);
        facet::StrokeRect(drawList, item.rect.Min, item.rect.Max, Col(facet::ControlBoundaryColor(c)), radius,
                          facet::HairlineThickness());
        drawList->AddText(ImVec2(item.rect.Min.x + padding, facet::CenteredTextY(item.rect.Min.y, height)),
                          Col(item.disabled ? c.labelTertiary : c.label), label, end);
        if (icon != nullptr)
            facet::DrawIcon(drawList, facet::FindIcon(icon),
                            ImVec2(item.rect.Max.x - padding - iconSize, item.rect.Min.y + (height - iconSize) * 0.5f),
                            iconSize, Col(c.labelSecondary));
        fi::DrawItemFocusRing(item, radius);
    }
    facet::PopTextStyle();
    return item;
}

struct RowLook
{
    bool selected = false;  // the chosen entry: the selection pill and a semibold label
};

// A list row: full width of the level, the hover fill, the label, the focus ring inside the row
// (the level's padding is too narrow for a ring outside it). Menu rows come from popup_menu.h.
fi::ItemState ProtoRow(PageCtx& ctx, const char* label, const char* probe, const RowLook& look,
                       fi::SelectionPill* pill = nullptr)
{
    const facet::Theme& t = facet::T();
    const facet::Colors& c = t.colors;
    const char* end = ImGui::FindRenderedTextEnd(label);
    fi::PushPillLabelStyle(facet::TextStyle::Body, look.selected);
    const float padding = facet::Px(t.spacing.sm);
    const float height = facet::ControlHeight(facet::ControlSize::Regular);
    const float natural = ImGui::CalcTextSize(label, end).x + padding * 2.0f;
    const float width = std::max(natural, ImGui::GetContentRegionAvail().x);
    fi::ItemOptions options;
    options.hitTarget = false;
    const fi::ItemState item = fi::ItemBehavior(ImGui::GetID(label), ImVec2(width, height), options);
    ctx.Probe(probe);
    if (pill != nullptr && look.selected)
        pill->Target(item.rect);
    if (item.visible)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const float radius = facet::Px(t.radii.element);
        const float feedback = std::max(item.hoverAmount, item.pressAmount);
        if (feedback > 0.0f && !look.selected)
        {
            const ImVec4 fill = facet::Mix(c.fill[2], c.fill[1], item.pressAmount);
            facet::FillRect(drawList, item.rect.Min, item.rect.Max, Col(facet::WithAlpha(fill, fill.w * feedback)), radius);
        }
        drawList->AddText(ImVec2(item.rect.Min.x + padding, facet::CenteredTextY(item.rect.Min.y, height)),
                          Col(item.disabled ? c.labelTertiary : c.label), label, end);
        ImRect ring = item.rect;
        ring.Expand(-fi::FocusRingOutset());
        fi::DrawFocusRing(drawList, ring, std::max(0.0f, radius - fi::FocusRingOutset()), item.focusAmount);
    }
    facet::PopTextStyle();
    return item;
}

// Inner padding for content that needs more room than the level's own (popovers, dialogs). Its
// scope must end before EndPopupLevel, because it closes a group in the level's window.
struct Padded
{
    ImVec2 pad;
    explicit Padded(float units) : pad(facet::Px(ImVec2(units, units)))
    {
        ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPos().x + pad.x, ImGui::GetCursorPos().y + pad.y));
        ImGui::BeginGroup();
    }
    ~Padded()
    {
        ImGui::EndGroup();
        ImGui::SetCursorScreenPos(ImGui::GetItemRectMax());
        ImGui::Dummy(pad);
    }
    Padded(const Padded&) = delete;
    Padded& operator=(const Padded&) = delete;
};

void NoteReason(ImGuiID id)
{
    const fi::PopupCloseReason reason = fi::PopupLevelClosedReason(id);
    if (reason != fi::PopupCloseReason::None)
        page.lastReason = reason;
}

const char* ReasonName(fi::PopupCloseReason reason)
{
    switch (reason)
    {
    case fi::PopupCloseReason::None:
        return "none yet";
    case fi::PopupCloseReason::Escape:
        return "Escape";
    case fi::PopupCloseReason::ClickOutside:
        return "a click outside";
    case fi::PopupCloseReason::Closed:
        return "a choice or its parent";
    }
    return "none yet";
}

// ---- Dropdown ----------------------------------------------------------------------------------

void DropdownSection(PageCtx& ctx)
{
    SectionHeader("Dropdown list");
    const float right = facet::demo::ContentRight();
    char counter[48];
    std::snprintf(counter, sizeof(counter), "Counter %d###counter", page.counter);
    if (ProtoAnchor(ctx, counter, "bg.button", false).pressed)
        ++page.counter;
    char label[48];
    std::snprintf(label, sizeof(label), "Size: %s###size", kSizes[page.size]);
    facet::demo::SameLineIfFits(right, AnchorWidth(label, true));

    if (Capturing(ctx, kDropdown))
        ctx.ForceNextItemState(ForcedItemState::Open);
    const bool open = fi::IsPopupLevelOpen(ids.dropdown);
    const fi::ItemState anchor = ProtoAnchor(ctx, label, "dd.anchor", open, "chevron-down");
    const fi::PopupAnchor where = AnchorOf(anchor.rect, fi::PopupSide::Below, true);
    if (anchor.pressed || anchor.open)
        OpenOrForce(anchor, ids.dropdown, where, {});
    fi::SetPopupLevelAnchor(ids.dropdown, where);
    if (fi::BeginPopupLevel(ids.dropdown))
    {
        fi::SelectionPill pill;
        pill.Begin(ids.dropdown, ImGui::GetWindowDrawList(), ImGui::GetWindowPos());
        for (int i = 0; i < kSizeCount; ++i)
        {
            char probe[24];
            std::snprintf(probe, sizeof(probe), "dd.row.%d", i);
            RowLook look;
            look.selected = i == page.size;
            const fi::ItemState row = ProtoRow(ctx, kSizes[i], probe, look, &pill);
            if (look.selected && ImGui::IsWindowAppearing())
                ImGui::SetItemDefaultFocus();
            if (row.pressed)
            {
                page.size = i;
                fi::ClosePopupLevel(ids.dropdown);
            }
        }
        pill.End(Col(facet::T().colors.accentSubtle), facet::Px(facet::T().radii.element));
        fi::EndPopupLevel();
    }
    NoteReason(ids.dropdown);

    char longLabel[48];
    if (page.longRow >= 0)
        std::snprintf(longLabel, sizeof(longLabel), "Placeholder row %d###long", page.longRow + 1);
    else
        std::snprintf(longLabel, sizeof(longLabel), "Long list###long");
    facet::demo::SameLineIfFits(right, AnchorWidth(longLabel, true));
    const fi::ItemState longAnchor = ProtoAnchor(ctx, longLabel, "dd.long", fi::IsPopupLevelOpen(ids.longList), "chevron-down");
    const fi::PopupAnchor longWhere = AnchorOf(longAnchor.rect, fi::PopupSide::Below, true);
    if (longAnchor.pressed)
        fi::OpenPopupLevel(ids.longList, longWhere);
    fi::SetPopupLevelAnchor(ids.longList, longWhere);
    if (fi::BeginPopupLevel(ids.longList))
    {
        for (int i = 0; i < kLongRows; ++i)
        {
            char row[32];
            std::snprintf(row, sizeof(row), "Placeholder row %d", i + 1);
            RowLook look;
            look.selected = i == page.longRow;
            if (ProtoRow(ctx, row, "dd.long.row", look).pressed)
            {
                page.longRow = i;
                fi::ClosePopupLevel(ids.longList);
            }
        }
        fi::EndPopupLevel();
    }
    Note("The list is at least as wide as its field and flips above it near the bottom of the window. Click, Enter or "
         "Space opens it, the arrows move, Enter chooses, Escape closes without choosing. The long list of %d "
         "placeholder rows is taller than most windows: it shrinks to the room on the side with more space and "
         "scrolls. Last close: %s.",
         kLongRows, ReasonName(page.lastReason));
    if (Capturing(ctx, kDropdown))
        ctx.RevealInCapture(0.25f);
}

// ---- Menu --------------------------------------------------------------------------------------

struct MenuEntry
{
    const char* label;
    const char* probe;
    int child;                  // the menu level this row opens, or -1
    ImGuiKeyChord chord = 0;    // the shortcut that also runs it (the page registers it), 0 for none
};

const MenuEntry kMenu1[] = {{"Cut", "menu.cut", -1, ImGuiMod_Ctrl | ImGuiKey_X},
                            {"Copy", "menu.copy", -1, ImGuiMod_Ctrl | ImGuiKey_C},
                            {"Paste", "menu.paste", -1, ImGuiMod_Ctrl | ImGuiKey_V},
                            {"Arrange", "menu.arrange", 1}};
const MenuEntry kMenu2[] = {{"Bring forward", "menu.forward", -1}, {"Send backward", "menu.backward", -1},
                            {"Align", "menu.align", 2}};
// The alignment is a choice: the current one carries the check mark.
const MenuEntry kMenu3[] = {{"Left", "menu.left", -1}, {"Center", "menu.center", -1}, {"Right", "menu.right", -1}};

void SubmitMenuLevel(PageCtx& ctx, int depth);

void RunMenuAction(const char* label)
{
    std::snprintf(page.lastAction, sizeof(page.lastAction), "%s", label);
}

// One row of menu level `depth`, through the menu row block (popup_menu.h): it opens submenus
// after the rest delay or by key, keeps them open while the pointer aims at them, and closes the
// chain after an action.
void MenuRow(PageCtx& ctx, int depth, int index, const MenuEntry& entry)
{
    if (entry.child >= 0 && Capturing(ctx, kMenu))
        ctx.ForceNextItemState(ForcedItemState::Open);
    char shortcut[32] = "";
    if (entry.chord != 0)
        facet::ChordToText(entry.chord, shortcut, sizeof(shortcut));
    fi::MenuRowDesc desc;
    desc.shortcut = entry.chord != 0 ? shortcut : nullptr;
    desc.submenu = entry.child >= 0 ? ids.menu[entry.child] : 0;
    desc.checked = depth == 2 && page.align == index;
    const fi::MenuRowResult row = fi::MenuRow(ids.menu[depth], entry.label, desc);
    ctx.Probe(entry.probe);
    if (row.pressed)
    {
        RunMenuAction(entry.label);
        if (depth == 2)
            page.align = index;
    }
    if (entry.child >= 0)
        SubmitMenuLevel(ctx, entry.child);
}

void SubmitMenuLevel(PageCtx& ctx, int depth)
{
    if (!fi::BeginPopupLevel(ids.menu[depth]))
        return;
    fi::MenuLevelBegin(ids.menu[depth]);
    const MenuEntry* entries = depth == 0 ? kMenu1 : (depth == 1 ? kMenu2 : kMenu3);
    const int count = depth == 0 ? IM_COUNTOF(kMenu1) : (depth == 1 ? IM_COUNTOF(kMenu2) : IM_COUNTOF(kMenu3));
    for (int i = 0; i < count; ++i)
        MenuRow(ctx, depth, i, entries[i]);
    fi::EndPopupLevel();
}

// The shortcuts the menu shows run its actions while the page is shown; a focused text field or
// any other owner of the chord takes it first (global route).
void MenuShortcuts()
{
    for (const MenuEntry& entry : kMenu1)
        if (entry.chord != 0 && ImGui::Shortcut(entry.chord, ImGuiInputFlags_RouteGlobal))
            RunMenuAction(entry.label);
}

void MenuSection(PageCtx& ctx)
{
    SectionHeader("Menu with submenus");
    const bool forced = Capturing(ctx, kMenu);
    if (forced)
        ctx.ForceNextItemState(ForcedItemState::Open);
    const fi::ItemState anchor = ProtoAnchor(ctx, "Edit menu", "menu.anchor", fi::IsPopupLevelOpen(ids.menu[0]),
                                             "chevron-down");
    const fi::PopupAnchor where = AnchorOf(anchor.rect, fi::PopupSide::Below);
    const fi::PopupOptions options{.minWidth = facet::Px(facet::T().overlay.menuMinWidth)};
    if (anchor.pressed || anchor.open)
        OpenOrForce(anchor, ids.menu[0], where, options);
    // Capture of the close motion: open during the warm-up, close a few frames before the image.
    if (Capturing(ctx, kClosing) && !page.closingStarted && !fi::IsPopupLevelOpen(ids.menu[0]))
    {
        fi::OpenPopupLevel(ids.menu[0], where, options);
        page.closingStarted = true;
    }
    fi::SetPopupLevelAnchor(ids.menu[0], where);
    SubmitMenuLevel(ctx, 0);
    MenuShortcuts();
    if (Capturing(ctx, kClosing) && ctx.captureFramesLeft == 5)
        fi::ClosePopupLevel(ids.menu[0]);
    ImGui::SameLine();
    NoteBeside("Last action: %s.", page.lastAction);
    Note("Hover a row with a chevron for %.1f s, or press Enter, Space or the Right arrow on it, to open its submenu; "
         "Left closes a submenu. Escape closes one level per press and gives the focus back to the row or button that "
         "opened it. A click on a row of an open level is meant for that row; a click anywhere else closes the menu "
         "and presses nothing. The shortcuts shown run the same actions while the menu is closed.",
         facet::T().overlay.submenuDelay);
    if (forced || Capturing(ctx, kClosing))
        ctx.RevealInCapture(0.2f);
}

// ---- Context menu ------------------------------------------------------------------------------

void ContextSection(PageCtx& ctx)
{
    SectionHeader("Context menu");
    const facet::Theme& t = facet::T();
    const facet::Colors& c = t.colors;
    const float width = std::min(ImGui::GetContentRegionAvail().x, facet::Px(t.metrics.cardWidth * 1.5f));
    const ImVec2 size(width, facet::ControlHeight(facet::ControlSize::Large) * 2.0f);
    const fi::ItemState target = fi::ItemBehavior(ImGui::GetID("context target"), size, {.handCursor = false});
    ctx.Probe("ctx.target");
    if (target.visible)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const float radius = facet::Px(t.radii.card);
        facet::FillRect(drawList, target.rect.Min, target.rect.Max, Col(c.fill[3]), radius);
        facet::StrokeRect(drawList, target.rect.Min, target.rect.Max, Col(facet::ControlBoundaryColor(c)), radius,
                          facet::HairlineThickness());
        facet::PushTextStyle(facet::TextStyle::Footnote);
        const char* text = "Right click here, or focus it and press the Menu key or Shift+F10";
        const ImVec2 textSize = ImGui::CalcTextSize(text, nullptr, false, width - facet::Px(t.spacing.lg) * 2.0f);
        drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                          ImVec2(target.rect.Min.x + facet::Px(t.spacing.lg), target.rect.GetCenter().y - textSize.y * 0.5f),
                          Col(c.labelSecondary), text, nullptr, width - facet::Px(t.spacing.lg) * 2.0f);
        facet::PopTextStyle();
        fi::DrawItemFocusRing(target, radius);
    }
    ImVec2 at;
    const fi::PopupOptions options{.minWidth = facet::Px(t.overlay.menuMinWidth)};
    if (fi::ContextMenuRequested(fi::ContextRequestTarget::LastItem, &at))
        fi::OpenPopupLevel(ids.context, AnchorOf(ImRect(at, at), fi::PopupSide::Below), options);
    if (Capturing(ctx, kContext))
    {
        const ImVec2 point(target.rect.Min.x + width * 0.3f, target.rect.GetCenter().y);
        fi::ForcePopupLevelOpen(ids.context, AnchorOf(ImRect(point, point), fi::PopupSide::Below), options);
    }
    if (fi::BeginPopupLevel(ids.context))
    {
        fi::MenuLevelBegin(ids.context);
        const MenuEntry entries[] = {{"Copy", "ctx.copy", -1}, {"Rename", "ctx.rename", -1}, {"Delete", "ctx.delete", -1}};
        for (const MenuEntry& entry : entries)
        {
            const bool pressed = fi::MenuRow(ids.context, entry.label).pressed;
            ctx.Probe(entry.probe);
            if (pressed)
                RunMenuAction(entry.label);
        }
        fi::EndPopupLevel();
    }
    Note("The menu opens at the pointer for a right click and below the focused target for the keys. Last action: %s.",
         page.lastAction);
    if (Capturing(ctx, kContext))
        ctx.RevealInCapture(0.3f);
}

// ---- Popover -----------------------------------------------------------------------------------

// A band of the chart palette behind the popover, so the blur of the glass surface can be seen.
void Backdrop(float height)
{
    const facet::Theme& t = facet::T();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 min = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const float stripe = width / static_cast<float>(facet::kChartPaletteSize * 2);
    for (int i = 0; i < facet::kChartPaletteSize * 2; ++i)
    {
        const float x = min.x + stripe * static_cast<float>(i);
        drawList->AddRectFilled(ImVec2(x, min.y), ImVec2(x + stripe, min.y + height),
                                Col(t.colors.chart[i % facet::kChartPaletteSize]));
    }
    facet::PushTextStyle(facet::TextStyle::Footnote);
    const char* text = "Backdrop: chart palette stripes, here so the blur of the glass popover shows";
    const ImVec2 pos(min.x + facet::Px(t.spacing.sm), min.y + height - ImGui::GetFontSize() - facet::Px(t.spacing.sm));
    const ImVec2 textSize = ImGui::CalcTextSize(text);
    drawList->AddRectFilled(ImVec2(pos.x - facet::Px(t.spacing.xs), pos.y),
                            ImVec2(pos.x + textSize.x + facet::Px(t.spacing.xs), pos.y + textSize.y), Col(t.colors.bg),
                            facet::Px(t.radii.element));
    drawList->AddText(pos, Col(t.colors.label), text);
    facet::PopTextStyle();
}

void PopoverSection(PageCtx& ctx)
{
    SectionHeader("Popover");
    const facet::Theme& t = facet::T();
    if (Capturing(ctx, kPopover))
        ctx.ForceNextItemState(ForcedItemState::Open);
    const fi::ItemState anchor =
        ProtoAnchor(ctx, "Display options", "pop.anchor", fi::IsPopupLevelOpen(ids.popover), "chevron-down");
    const fi::PopupAnchor where = AnchorOf(anchor.rect, fi::PopupSide::Below);
    const fi::PopupOptions options{.glass = true};
    if (anchor.pressed || anchor.open)
        OpenOrForce(anchor, ids.popover, where, options);
    fi::SetPopupLevelAnchor(ids.popover, where);
    if (fi::BeginPopupLevel(ids.popover))
    {
        {
            const Padded padded(t.spacing.sm);
            facet::Text(facet::TextStyle::Headline, "Display options");
            facet::demo::ProtoToggle(ctx, "Show grid", &page.grid, "pop.toggle");
            facet::demo::ProtoSliderOptions slider;
            slider.length = t.metrics.pickerWidth * 0.75f;
            facet::Text(facet::TextStyle::Footnote, "Grid opacity %.0f %%", page.opacity * 100.0);
            facet::demo::ProtoSlider(ctx, "##opacity", &page.opacity, slider, "pop.slider");
        }
        fi::EndPopupLevel();
    }
    Backdrop(facet::Px(t.spacing.xxxl * 4.0f));
    ImGui::Dummy(ImVec2(0.0f, facet::Px(t.spacing.xxxl * 4.0f)));
    Note("A glass popover with controls inside: the focus moves into it when it opens, Escape or a click outside "
         "closes it, a click on its button toggles it. Grid %s, opacity %.0f %%.",
         page.grid ? "on" : "off", page.opacity * 100.0);
    if (Capturing(ctx, kPopover))
        ctx.RevealInCapture(0.25f);
}

// ---- Tooltips ----------------------------------------------------------------------------------

void TooltipSection(PageCtx& ctx)
{
    SectionHeader("Tooltips");
    struct Tool
    {
        const char* label;
        const char* probe;
        const char* tip;
    };
    const Tool tools[] = {
        {"Cut", "tip.0", "Moves the selection to the clipboard (Ctrl+X)"},
        {"Copy", "tip.1", "Copies the selection to the clipboard (Ctrl+C)"},
        {"Paste", "tip.2", "Inserts the clipboard at the caret (Ctrl+V)"},
        {"Find", "tip.3", "Searches the document (Ctrl+F)"},
    };
    ImGui::PushID("tools");
    const float right = facet::demo::ContentRight();
    for (int i = 0; i < 4; ++i)
    {
        if (i > 0)
            facet::demo::SameLineIfFits(right, facet::demo::ProtoButtonWidth(tools[i].label));
        const fi::ItemState item = ProtoAnchor(ctx, tools[i].label, tools[i].probe, false);
        page.toolIds[i] = item.id;
        bool show = false;
        if (Capturing(ctx, kTooltip) && i == 1)
        {
            // The tooltip as the pointer shows it, forced for the image.
            const ImVec2 point = item.rect.GetCenter();
            const ImVec2 cursor = facet::Px(ImVec2(facet::T().spacing.lg, facet::T().spacing.xl));
            const ImRect anchor(ImVec2(point.x + cursor.x, point.y), ImVec2(point.x + cursor.x, point.y + cursor.y));
            const ImGuiID tipId = fi::PopupTooltipLevel(item.id);
            fi::ForcePopupLevelOpen(tipId, AnchorOf(anchor, fi::PopupSide::Below), {.kind = fi::PopupKind::Tooltip});
            show = fi::BeginPopupLevel(tipId);
        }
        else
        {
            show = fi::BeginPopupTooltip(item.id, item.rect, item.hovered, item.focusVisible);
        }
        if (show)
        {
            facet::Text(facet::TextStyle::Footnote, "%s", tools[i].tip);
            fi::EndPopupLevel();
        }
    }
    ImGui::PopID();
    Note("Rest the pointer on a button for %.1f s; then the neighbours show theirs at once for %.1f s. A mouse press or "
         "a key hides the tooltip; the focused button shows its tooltip after keyboard navigation. Tooltips take no "
         "focus and no input.",
         facet::T().overlay.tooltipDelay, facet::T().overlay.tooltipGrace);
    if (Capturing(ctx, kTooltip))
        ctx.RevealInCapture(0.4f);
}

// ---- Modal -------------------------------------------------------------------------------------

void ModalSection(PageCtx& ctx)
{
    SectionHeader("Modal with a confirm level");
    const facet::Theme& t = facet::T();
    const bool forced = Capturing(ctx, kModal);
    if (forced)
        ctx.ForceNextItemState(ForcedItemState::Open);
    const fi::ItemState anchor = ProtoAnchor(ctx, "Open dialog", "modal.open", fi::IsPopupLevelOpen(ids.modal));
    const fi::PopupOptions modalOptions{.kind = fi::PopupKind::Modal, .size = ImVec2(facet::Px(t.overlay.modalWidth), 0.0f)};
    if (anchor.pressed || anchor.open)
        OpenOrForce(anchor, ids.modal, {}, modalOptions);
    if (fi::PopupLevelScrimClicked(ids.modal))
        ++page.scrimClicks;
    if (fi::BeginPopupLevel(ids.modal))
    {
        facet::Text(facet::TextStyle::Title3, "Dialog prototype");
        ImGui::PushTextWrapPos(0.0f);
        facet::Text(facet::TextStyle::Body,
                    "The page behind is dimmed and takes no input; the focus stays inside. A click on the dimmed page "
                    "is reported to the dialog, which decides what it means: this one counts it (%d so far).",
                    page.scrimClicks);
        ImGui::PopTextWrapPos();
        ImGui::Dummy(ImVec2(0.0f, facet::Px(t.spacing.sm)));
        if (forced)
            ctx.ForceNextItemState(ForcedItemState::Open);
        const fi::ItemState confirmButton = ProtoAnchor(ctx, "Discard changes", "modal.confirm", false);
        if (confirmButton.pressed || confirmButton.open)
            OpenOrForce(confirmButton, ids.confirm, {}, {.kind = fi::PopupKind::Modal, .size = ImVec2(facet::Px(t.overlay.modalWidth), 0.0f)});
        ImGui::SameLine();
        if (ProtoAnchor(ctx, "Close", "modal.close", false).pressed)
            fi::ClosePopupLevel(ids.modal);
        if (fi::BeginPopupLevel(ids.confirm))
        {
            facet::Text(facet::TextStyle::Title3, "Discard changes?");
            ImGui::PushTextWrapPos(0.0f);
            facet::Text(facet::TextStyle::Body, "A confirm level over the dialog. Escape closes only this level.");
            ImGui::PopTextWrapPos();
            ImGui::Dummy(ImVec2(0.0f, facet::Px(t.spacing.sm)));
            if (ProtoAnchor(ctx, "Keep", "confirm.keep", false).pressed)
            {
                ++page.kept;
                fi::ClosePopupLevel(ids.confirm);
            }
            if (ImGui::IsWindowAppearing())
                ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ProtoAnchor(ctx, "Discard", "confirm.discard", false).pressed)
            {
                ++page.confirmed;
                fi::ClosePopupLevel(ids.confirm);
            }
            fi::EndPopupLevel();
        }
        fi::EndPopupLevel();
    }
    ImGui::SameLine();
    NoteBeside("Discarded %d times, kept %d times.", page.confirmed, page.kept);
    if (forced)
        ctx.RevealInCapture(0.5f);
}

// ---- Placement samples -------------------------------------------------------------------------

void PlacementSection(PageCtx& ctx)
{
    SectionHeader("Placement near the display edges");
    facet::demo::ProtoToggle(ctx, "Show placement samples", &page.samples, "place.toggle");
    const bool show = page.samples || Capturing(ctx, kPlacement);
    Note("Eight small anchors sit at the corners and edge middles of the display. Each asks for the side that has no "
         "room, so its level flips to the opposite side and shifts along the edge to stay %.0f units inside the display.",
         facet::T().overlay.viewportMargin);
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float m = facet::Px(facet::T().overlay.viewportMargin);
    const float s = facet::Px(facet::T().spacing.lg);
    const ImVec2 lo(viewport->WorkPos.x + m, viewport->WorkPos.y + m);
    const ImVec2 hi(viewport->WorkPos.x + viewport->WorkSize.x - m - s, viewport->WorkPos.y + viewport->WorkSize.y - m - s);
    const ImVec2 mid((lo.x + hi.x) * 0.5f, (lo.y + hi.y) * 0.5f);
    struct Sample
    {
        ImVec2 at;
        fi::PopupSide asked;
    };
    const Sample samples[kSampleCount] = {
        {{lo.x, lo.y}, fi::PopupSide::Above},  {{hi.x, lo.y}, fi::PopupSide::After},
        {{lo.x, hi.y}, fi::PopupSide::Below},  {{hi.x, hi.y}, fi::PopupSide::Below},
        {{mid.x, lo.y}, fi::PopupSide::Above}, {{mid.x, hi.y}, fi::PopupSide::Below},
        {{lo.x, mid.y}, fi::PopupSide::Before}, {{hi.x, mid.y}, fi::PopupSide::After},
    };
    ImDrawList* foreground = ImGui::GetForegroundDrawList();
    for (int i = 0; i < kSampleCount; ++i)
    {
        const ImRect rect(samples[i].at, ImVec2(samples[i].at.x + s, samples[i].at.y + s));
        const fi::PopupAnchor anchor = AnchorOf(rect, samples[i].asked);
        const fi::PopupOptions options{.kind = fi::PopupKind::Tooltip};
        if (!show)
        {
            if (fi::IsPopupLevelOpen(ids.samples[i]))
                fi::ClosePopupLevel(ids.samples[i]);
            continue;
        }
        if (ctx.capture)
            fi::ForcePopupLevelOpen(ids.samples[i], anchor, options);
        else if (!fi::IsPopupLevelOpen(ids.samples[i]))
            fi::OpenPopupLevel(ids.samples[i], anchor, options);
        fi::SetPopupLevelAnchor(ids.samples[i], anchor);
        facet::FillRect(foreground, rect.Min, rect.Max, Col(facet::T().colors.focusRing), facet::Px(facet::T().radii.element));
        if (fi::BeginPopupLevel(ids.samples[i]))
        {
            facet::Text(facet::TextStyle::Footnote, "Asked %s, placed %s", SideName(samples[i].asked),
                        SideName(fi::PopupLevelSide(ids.samples[i])));
            fi::EndPopupLevel();
        }
    }
}

void Observe()
{
    seen.dropdown = fi::IsPopupLevelOpen(ids.dropdown);
    seen.longList = fi::IsPopupLevelOpen(ids.longList);
    seen.longRect = fi::PopupLevelRect(ids.longList);
    for (int i = 0; i < 3; ++i)
        seen.menu[i] = fi::IsPopupLevelOpen(ids.menu[i]);
    seen.context = fi::IsPopupLevelOpen(ids.context);
    seen.popover = fi::IsPopupLevelOpen(ids.popover);
    seen.modal = fi::IsPopupLevelOpen(ids.modal);
    seen.confirm = fi::IsPopupLevelOpen(ids.confirm);
    seen.top = fi::TopPopupLevel();
    for (int i = 0; i < 4; ++i)
    {
        const ImGuiID tip = fi::PopupTooltipLevel(page.toolIds[i]);
        seen.tip[i] = fi::IsPopupLevelOpen(tip);
        seen.tipRect[i] = fi::PopupLevelRect(tip);
    }
}

void StatusLine()
{
    const fi::PopupHostStats stats = fi::GetPopupHostStats();
    const ImGuiID top = fi::TopPopupLevel();
    const char* name = "none";
    if (top == ids.dropdown || top == ids.longList)
        name = "a dropdown list";
    else if (top == ids.menu[0] || top == ids.menu[1] || top == ids.menu[2])
        name = "a menu level";
    else if (top == ids.context)
        name = "the context menu";
    else if (top == ids.popover)
        name = "the popover";
    else if (top == ids.modal || top == ids.confirm)
        name = "a modal level";
    Note("Topmost level: %s. Escape belongs to the popup host in this frame: %s. Levels playing their close motion: %d.",
         name, stats.ownsEscape ? "yes" : "no", stats.closingLevels);
}

} // namespace

FACET_DEMO_PAGE("popup", "Popup host", "Blocks", 110, "default", "dropdown", "menu", "context", "popover", "tooltip",
                "modal", "placement", "closing")
{
    TakeIds();
    if (ctx.capture && ctx.captureFramesLeft > 50)
        page.closingStarted = false;
    facet::demo::PageHeader("Popup host",
                            "Every floating layer goes through one host: it places the layer next to its anchor, keeps "
                            "the stack of nested levels, closes them on Escape and on a click outside, moves the focus "
                            "in and back, draws the surface and plays the open and close motion.");
    Note("Prototypes: built on the popup host and the item behaviour, focus ring and selection pill blocks. The finished "
         "menus, popovers and dialogs are on the Navigation and Overlays pages.");
    StatusLine();
    DropdownSection(ctx);
    MenuSection(ctx);
    ContextSection(ctx);
    PopoverSection(ctx);
    TooltipSection(ctx);
    ModalSection(ctx);
    PlacementSection(ctx);
    Observe();
}

FACET_DEMO_SELFTEST("popup")
{
    using T = facet::demo::Target;
    test.Do([] {
        page = {};
        seen = {};
    });

    // Dropdown by mouse.
    test.Click("dd.anchor");
    test.Wait(2);
    test.Expect("a click on the field opens the list", [] { return seen.dropdown; });
    test.Click("dd.row.2");
    test.Expect("a click on a row chooses it and closes the list", [] { return page.size == 2 && !seen.dropdown; });
    test.ExpectFocused("dd.anchor");
    // By keyboard: after a click the first Tab shows the focus; Enter opens with the focus on the
    // chosen row, Down moves, Enter chooses.
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Enter);
    test.Wait(2);
    test.Expect("Enter opens the list", [] { return seen.dropdown; });
    test.ExpectFocused("dd.row.2");
    test.KeyPress(ImGuiKey_DownArrow);
    test.ExpectFocused("dd.row.3");
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("Enter chooses the focused row", [] { return page.size == 3 && !seen.dropdown; });
    test.ExpectFocused("dd.anchor");
    // Escape closes without choosing and gives the focus back.
    test.KeyPress(ImGuiKey_Space);
    test.Wait(2);
    test.Expect("Space opens the list", [] { return seen.dropdown; });
    test.KeyPress(ImGuiKey_DownArrow);
    test.KeyPress(ImGuiKey_Escape);
    test.Expect("Escape closes the list without choosing", [] {
        return page.size == 3 && !seen.dropdown && page.lastReason == fi::PopupCloseReason::Escape;
    });
    test.ExpectFocused("dd.anchor");
    // A click outside closes and presses nothing; a click on the field toggles.
    test.Click("dd.anchor");
    test.Wait(2);
    test.Click("bg.button");
    test.Expect("a click outside closes the list", [] { return !seen.dropdown; });
    test.Expect("the click that closed it pressed nothing", [] { return page.counter == 0; });
    test.Click("bg.button");
    test.Expect("the next click presses the button", [] { return page.counter == 1; });
    test.Click("dd.anchor");
    test.Wait(2);
    test.Click("dd.anchor");
    test.Wait(2);
    test.Expect("a click on the open field closes it", [] { return !seen.dropdown; });
    test.Click("dd.anchor");
    test.Wait(2);
    test.Expect("the next click opens it again", [] { return seen.dropdown; });
    test.KeyPress(ImGuiKey_Escape);

    // The long list stays inside the display and scrolls.
    test.Click("dd.long");
    test.Wait(3);
    test.Expect("the long list stays inside the display", [] {
        const ImGuiViewport* v = ImGui::GetMainViewport();
        const ImRect& r = seen.longRect;
        return seen.longList && r.Min.y >= v->WorkPos.y && r.Max.y <= v->WorkPos.y + v->WorkSize.y &&
               r.Min.x >= v->WorkPos.x && r.Max.x <= v->WorkPos.x + v->WorkSize.x;
    });
    test.KeyPress(ImGuiKey_Escape);

    // Menu by mouse: resting on a row opens its submenu, a click on a leaf runs it and closes all.
    test.Click("menu.anchor");
    test.Wait(2);
    test.Expect("a click opens the menu", [] { return seen.menu[0]; });
    test.Hover(T::Item("menu.arrange"), 0.4f);
    test.Expect("resting on a row with a chevron opens its submenu", [] { return seen.menu[1]; });
    test.Hover(T::Item("menu.align"), 0.4f);
    test.Expect("and its submenu, three levels deep", [] { return seen.menu[2]; });
    test.Click("menu.center");
    test.Expect("a click on a leaf runs it and closes the whole menu", [] {
        return std::string(page.lastAction) == "Center" && !seen.menu[0] && !seen.menu[1] && !seen.menu[2];
    });
    test.Expect("the chosen alignment is the checked row", [] { return page.align == 1; });
    test.ExpectFocused("menu.anchor");
    // By keyboard, then Escape level by level with the focus going back each time.
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Enter);
    test.Wait(2);
    test.ExpectFocused("menu.cut");
    test.KeyPress(ImGuiKey_UpArrow);
    test.ExpectFocused("menu.arrange");
    test.KeyPress(ImGuiKey_RightArrow);
    test.Wait(2);
    test.Expect("the Right arrow opens the submenu", [] { return seen.menu[1]; });
    test.ExpectFocused("menu.forward");
    test.KeyPress(ImGuiKey_UpArrow);
    test.ExpectFocused("menu.align");
    test.KeyPress(ImGuiKey_Enter);
    test.Wait(2);
    test.ExpectFocused("menu.left");
    test.KeyPress(ImGuiKey_Escape);
    test.Expect("the first Escape closes only the third level", [] { return !seen.menu[2] && seen.menu[1]; });
    test.ExpectFocused("menu.align");
    test.KeyPress(ImGuiKey_LeftArrow);
    test.Expect("the Left arrow closes the second level", [] { return !seen.menu[1] && seen.menu[0]; });
    test.ExpectFocused("menu.arrange");
    test.KeyPress(ImGuiKey_Escape);
    test.Expect("the next Escape closes the menu", [] { return !seen.menu[0]; });
    test.ExpectFocused("menu.anchor");
    test.Wait(2);
    test.Expect("nothing owns Escape once every level is closed",
                [] { return ImGui::GetKeyOwner(ImGuiKey_Escape) == ImGuiKeyOwner_NoOwner; });
    // The shortcut a row shows runs its action with the menu closed.
    test.KeyPress(ImGuiKey_X | ImGuiMod_Ctrl);
    test.Expect("Ctrl+X runs Cut", [] { return std::string(page.lastAction) == "Cut"; });

    // Context menu: right click, then the Menu key and Shift+F10 on the focused target.
    test.Click("ctx.target", ImGuiMouseButton_Right);
    test.Wait(2);
    test.Expect("a right click opens the context menu", [] { return seen.context; });
    test.Click("ctx.rename");
    test.Expect("a click on a row runs it", [] { return std::string(page.lastAction) == "Rename" && !seen.context; });
    test.Click("ctx.target");
    test.KeyPress(ImGuiKey_Menu);
    test.Wait(2);
    test.Expect("the Menu key opens it", [] { return seen.context; });
    test.ExpectFocused("ctx.copy");
    test.KeyPress(ImGuiKey_Escape);
    test.Expect("Escape closes it", [] { return !seen.context; });
    test.ExpectFocused("ctx.target");
    test.KeyPress(ImGuiKey_F10 | ImGuiMod_Shift);
    test.Wait(2);
    test.Expect("Shift+F10 opens it", [] { return seen.context; });
    test.KeyPress(ImGuiKey_Escape);

    // Popover with controls inside.
    test.Click("pop.anchor");
    test.Wait(2);
    test.Expect("a click opens the popover", [] { return seen.popover; });
    test.Click("pop.toggle");
    test.Expect("its toggle works and the popover stays open", [] { return !page.grid && seen.popover; });
    test.KeyPress(ImGuiKey_Escape);
    test.Expect("Escape closes the popover", [] { return !seen.popover; });
    test.ExpectFocused("pop.anchor");

    // Tooltips.
    test.MouseMove(T::Item("tip.0"));
    test.WaitSeconds(0.25f);
    test.Expect("no tooltip before the delay", [] { return !seen.tip[0]; });
    test.WaitSeconds(0.4f);
    test.Expect("resting on a button shows its tooltip after the delay", [] { return seen.tip[0]; });
    test.Expect("the tooltip sits below and after the pointer, so the cursor does not cover it", [] {
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        return seen.tipRect[0].Min.x > mouse.x && seen.tipRect[0].Min.y > mouse.y;
    });
    test.MouseMove(T::Item("tip.1"));
    test.Wait(1);
    test.Expect("the neighbour shows its tooltip at once", [] { return seen.tip[1] && !seen.tip[0]; });
    test.MouseDown();
    test.Expect("a mouse press hides it", [] { return !seen.tip[1]; });
    test.MouseUp();
    test.Click("tip.0");
    test.KeyPress(ImGuiKey_Tab);
    test.WaitSeconds(0.7f);
    test.Expect("the focused button shows its tooltip after keyboard navigation", [] { return seen.tip[0]; });
    test.ExpectFocused("tip.0");

    // Modal: the background takes no input, a confirm level stacks over it, Escape one level at a time.
    test.Do([] { page.counter = 0; });
    test.Click("modal.open");
    test.Wait(2);
    test.Expect("a click opens the dialog", [] { return seen.modal; });
    test.ExpectFocused("modal.confirm");
    // The display edge lies inside the margin the dialog keeps, so the click lands on the scrim.
    test.MouseMove(T::Display(0.005f, 0.5f));
    test.MouseDown();
    test.MouseUp();
    test.Expect("a click on the dimmed page presses nothing and is reported",
                [] { return page.counter == 0 && page.scrimClicks == 1 && seen.modal; });
    test.Click("modal.confirm");
    test.Wait(2);
    test.Expect("the confirm level opens over the dialog", [] { return seen.confirm && seen.top == ids.confirm; });
    test.ExpectFocused("confirm.keep");
    test.KeyPress(ImGuiKey_Escape);
    test.Expect("Escape closes only the confirm level", [] { return !seen.confirm && seen.modal; });
    test.ExpectFocused("modal.confirm");
    test.Click("modal.confirm");
    test.Wait(2);
    test.Click("confirm.discard");
    test.Expect("a choice in the confirm level is taken", [] { return page.confirmed == 1 && seen.modal && !seen.confirm; });
    test.KeyPress(ImGuiKey_Escape);
    test.Expect("the next Escape closes the dialog", [] { return !seen.modal; });
    test.ExpectFocused("modal.open");

    // Leaving the page while a level is open leaves nothing behind.
    test.Click("dd.anchor");
    test.Wait(2);
    test.Expect("the list is open before the page changes", [] { return seen.dropdown; });
    test.SelectPage("item");
    test.Wait(3);
    test.Expect("leaving the page closed the level and emptied Dear ImGui's popup stack",
                [] { return GImGui->OpenPopupStack.Size == 0 && GImGui->BeginPopupStack.Size == 0; });
    test.Wait(2);
    test.Expect("and nothing owns Escape", [] { return ImGui::GetKeyOwner(ImGuiKey_Escape) == ImGuiKeyOwner_NoOwner; });
    test.SelectPage("popup");
    test.Wait(2);
    test.ExpectPage("popup");
}