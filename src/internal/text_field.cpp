// Text field core (text_field.h): wraps Dear ImGui's text editing (IME, undo, clipboard stay
// Dear ImGui's) and draws the Facet frame around it.
//
// One call is: the visible label, the frame drawn under the text, Dear ImGui's InputTextEx on
// the text area with its own frame made invisible, the text effects Dear ImGui cannot draw (the
// edge fade, password dots, a display text), then the adornments as parts, the focus ring and
// the message line, all inside one group. Every style change made for InputTextEx is undone by
// a scope object, so early returns leave nothing behind.

#include "internal/text_field.h"

#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <cstring>
#include <new>

#include "facet/core/anim.h"
#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/icons.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/focus.h"
#include "internal/loading.h"
#include "internal/state_storage.h"
#include "internal/subsystem.h"
#include "internal/text_field_detail.h"
#include "internal/value_drag.h"

namespace facet::internal
{

namespace
{

// Animation sub keys under the field id (below kItemAnimSubBase).
constexpr int kFieldAnimHover = 1;
constexpr int kFieldAnimFocus = 2;
constexpr int kFieldAnimFloat = 3;

// The spinner stroke as a share of the spinner size.
constexpr float kSpinnerStrokeShare = 0.125f;

// Pending selection, insert and focus requests: a few at once is plenty (one field has the
// keyboard at a time), and a request waits this many frames for its field.
constexpr int kTextFieldRequestSlots = 8;
constexpr int kTextFieldRequestFrames = 2;

// The bullet that stands for one masked character.
constexpr ImWchar kMaskBullet = 0x2022;

struct TextFieldRequest
{
    ImGuiID id = 0;
    int frame = 0;  // frame of the latest request
    bool focus = false;
    TextFieldFocus focusMode = TextFieldFocus::SelectAll;
    bool selection = false;
    int selectionStart = 0;
    int selectionEnd = 0;
    bool insert = false;
    std::string text;
};

std::array<TextFieldRequest, kTextFieldRequestSlots>& TextFieldRequests()
{
    static std::array<TextFieldRequest, kTextFieldRequestSlots> requests;
    return requests;
}

bool TextFieldRequestExpired(const TextFieldRequest& request)
{
    return request.id == 0 || ImGui::GetFrameCount() > request.frame + kTextFieldRequestFrames;
}

void ClearTextFieldRequest(TextFieldRequest& request)
{
    request.id = 0;
    request.focus = request.selection = request.insert = false;
    request.text.clear();  // keeps the capacity: no allocation the next time
}

// The live request of field `id`, or null.
TextFieldRequest* FindTextFieldRequest(ImGuiID id)
{
    if (id == 0)
        return nullptr;
    for (TextFieldRequest& request : TextFieldRequests())
    {
        if (request.id != id)
            continue;
        if (TextFieldRequestExpired(request))
        {
            ClearTextFieldRequest(request);
            return nullptr;
        }
        return &request;
    }
    return nullptr;
}

// The request of field `id`, created in a free (or the oldest) slot when there is none.
TextFieldRequest* TakeTextFieldRequest(ImGuiID id)
{
    if (ImGui::GetCurrentContext() == nullptr || id == 0)
        return nullptr;
    if (TextFieldRequest* existing = FindTextFieldRequest(id))
    {
        existing->frame = ImGui::GetFrameCount();
        return existing;
    }
    TextFieldRequest* slot = nullptr;
    for (TextFieldRequest& request : TextFieldRequests())
    {
        if (TextFieldRequestExpired(request))
        {
            slot = &request;
            break;
        }
        if (slot == nullptr || request.frame < slot->frame)
            slot = &request;
    }
    ClearTextFieldRequest(*slot);
    slot->id = id;
    slot->frame = ImGui::GetFrameCount();
    return slot;
}

// Where the last call of a field drew its text, for the caret and selection geometry.
struct TextFieldGeometry
{
    int frame = -1;
    ImVec2 origin;          // top-left of the first line, unscrolled (Dear ImGui's draw_pos)
    ImFont* font = nullptr;
    float fontSize = 0.0f;
    float caretWidth = 1.0f;
    float maskAdvance = 0.0f;  // > 0: masked, every code point this wide
    bool multiline = false;
    // A read-only field edits the caller's text directly; Dear ImGui keeps no copy. Used only in
    // the frame of the call.
    const char* readOnlyText = nullptr;
    // External change detection while the field has the keyboard: the text after the last call.
    ImGuiID textHash = 0;
    int textLength = 0;
    bool hashValid = false;
};

// Moves `offset` back onto the start of a UTF-8 code point.
int SnapToCodePoint(const char* text, int length, int offset)
{
    offset = std::clamp(offset, 0, length);
    while (offset > 0 && offset < length && (static_cast<unsigned char>(text[offset]) & 0xC0) == 0x80)
        --offset;
    return offset;
}

// The text layout Dear ImGui uses for a field, replayed for geometry queries.
struct TextFieldLayout
{
    const char* text = "";
    int length = 0;
    ImFont* font = nullptr;
    float size = 0.0f;
    float wrap = 0.0f;       // > 0: word wrap at this width (multiline)
    bool multiline = false;
    float maskAdvance = 0.0f;

    float Width(int from, int to) const
    {
        if (to <= from)
            return 0.0f;
        if (maskAdvance > 0.0f)
            return static_cast<float>(ImTextCountCharsFromUtf8(text + from, text + to)) * maskAdvance;
        return ImFontCalcTextSizeEx(font, size, FLT_MAX, 0.0f, text + from, text + to, text + to, nullptr, nullptr,
                                    ImDrawTextFlags_None)
            .x;
    }

    // The visual line starting at byte `start`: its end (exclusive, a hard line break not
    // included) and where the next line starts.
    void Line(int start, int* end, int* next) const
    {
        if (!multiline)
        {
            *end = length;
            *next = length + 1;
            return;
        }
        const char* begin = text + start;
        const char* textEnd = text + length;
        if (wrap > 0.0f)
        {
            const char* e = ImFontCalcWordWrapPositionEx(font, size, begin, textEnd, wrap, ImDrawTextFlags_WrapKeepBlanks);
            *end = static_cast<int>(e - text);
            *next = (e < textEnd && *e == '\n') ? *end + 1 : *end;
            if (*next == start)  // a line must move on, whatever the wrap width
                *next = start + 1;
            return;
        }
        const char* newline = static_cast<const char*>(std::memchr(begin, '\n', static_cast<std::size_t>(textEnd - begin)));
        *end = newline ? static_cast<int>(newline - text) : length;
        *next = newline ? *end + 1 : length + 1;
    }

    // Calls visit(lineIndex, start, end) for every visual line, as Dear ImGui's line index
    // counts them (an empty last line after a final line break included). Stops when it returns false.
    template <typename Visit>
    void ForEachLine(Visit visit) const
    {
        int index = 0;
        int start = 0;
        if (length == 0)
        {
            visit(0, 0, 0);
            return;
        }
        while (start < length)
        {
            int end = 0;
            int next = 0;
            Line(start, &end, &next);
            if (!visit(index, start, end))
                return;
            ++index;
            start = next;
        }
        if (multiline && text[length - 1] == '\n')
            visit(index, length, length);
    }
};

// The visual line of byte `offset` and the x of that offset on it, as InputTextEx places its
// caret (a caret on a wrap point after a move to the right sits at the end of the upper line).
void TextFieldLocate(const TextFieldLayout& layout, int offset, bool movedRight, int* lineOut, float* xOut)
{
    int line = 0;
    int lineStart = 0;
    bool found = false;
    const bool wrapped = layout.wrap > 0.0f;
    layout.ForEachLine([&](int index, int start, int) {
        const bool special = wrapped && movedRight && offset > 0 && layout.text[offset - 1] != '\n';
        if (start == offset && !(special && index > 0))
        {
            line = index;
            lineStart = start;
            found = true;
            return false;
        }
        if (start >= offset && index > 0)
        {
            found = true;
            return false;
        }
        line = index;
        lineStart = start;
        return true;
    });
    (void)found;
    *lineOut = line;
    *xOut = layout.Width(lineStart, std::max(lineStart, offset));
}

ImU32 U32(const ImVec4& color)
{
    return ToU32(color);
}

// Undoes every style change of the call: style colors and variables, item flags, the caret
// width and the reactivation request. Early returns are covered by the destructor.
class TextFieldStyleScope
{
public:
    TextFieldStyleScope()
        : g(*GImGui),
          colors(g.ColorStack.Size),
          vars(g.StyleVarStack.Size),
          itemFlags(g.ItemFlagsStack.Size),
          caretWidth(g.Style.InputTextCursorSize),
          reactivateId(g.InputTextReactivateId)
    {
    }
    ~TextFieldStyleScope()
    {
        if (g.ColorStack.Size > colors)
            ImGui::PopStyleColor(g.ColorStack.Size - colors);
        if (g.StyleVarStack.Size > vars)
            ImGui::PopStyleVar(g.StyleVarStack.Size - vars);
        while (g.ItemFlagsStack.Size > itemFlags)
            ImGui::PopItemFlag();
        g.Style.InputTextCursorSize = caretWidth;
        if (restoreReactivation)
            g.InputTextReactivateId = reactivateId;
    }
    // The call did not touch the reactivation request: whatever Dear ImGui sets during it
    // (io.ConfigInputTextEnterKeepActive) stays.
    void KeepReactivation() { restoreReactivation = false; }
    TextFieldStyleScope(const TextFieldStyleScope&) = delete;
    TextFieldStyleScope& operator=(const TextFieldStyleScope&) = delete;

private:
    ImGuiContext& g;
    int colors;
    int vars;
    int itemFlags;
    float caretWidth;
    ImGuiID reactivateId;
    bool restoreReactivation = true;
};

class TextFieldFontScope
{
public:
    explicit TextFieldFontScope(TextStyle style) { PushTextStyle(style); }
    ~TextFieldFontScope() { PopTextStyle(); }
    TextFieldFontScope(const TextFieldFontScope&) = delete;
    TextFieldFontScope& operator=(const TextFieldFontScope&) = delete;
};

// What the internal callback works with during one InputTextEx call.
struct TextFieldCallbackContext
{
    std::string* text = nullptr;               // resized for CallbackResize
    ImGuiInputTextCallback user = nullptr;
    void* userData = nullptr;
    ImGuiInputTextFlags userEvents = 0;        // the Callback* events the widget asked for
    int maxLength = 0;
    int remaining = INT_MIN;                   // code points that may still be added; INT_MIN: not counted yet
    ImGuiID id = 0;
    TextFieldRequest* request = nullptr;
    bool readOnly = false;
    bool multiline = false;
    bool resizeFailed = false;
};

int CountCodePoints(const char* text, int length)
{
    return length > 0 ? ImTextCountCharsFromUtf8(text, text + length) : 0;
}

// The byte length of the first `codePoints` code points of [text, text + length).
int CodePointBytes(const char* text, int length, int codePoints)
{
    int bytes = 0;
    while (bytes < length && codePoints > 0)
    {
        bytes += std::max(1, ImTextCountUtf8BytesFromChar(text + bytes, text + length));
        --codePoints;
    }
    return std::min(bytes, length);
}

void ApplyTextFieldRequest(TextFieldCallbackContext& context, ImGuiInputTextCallbackData* data)
{
    TextFieldRequest* request = context.request;
    if (request == nullptr || request->id != context.id)
        return;
    if (request->focus)
    {
        if (request->focusMode == TextFieldFocus::SelectAll)
        {
            data->SelectionStart = 0;
            data->SelectionEnd = data->CursorPos = data->BufTextLen;
        }
        else if (request->focusMode == TextFieldFocus::End)
        {
            data->SelectionStart = data->SelectionEnd = data->CursorPos = data->BufTextLen;
        }
        request->focus = false;
    }
    if (request->selection)
    {
        const int start = SnapToCodePoint(data->Buf, data->BufTextLen, request->selectionStart);
        const int end = SnapToCodePoint(data->Buf, data->BufTextLen, request->selectionEnd);
        data->SelectionStart = start;
        data->SelectionEnd = data->CursorPos = end;
        request->selection = false;
    }
    if (request->insert)
    {
        request->insert = false;
        if (!context.readOnly)
        {
            const int selStart = std::min(data->SelectionStart, data->SelectionEnd);
            const int selEnd = std::max(data->SelectionStart, data->SelectionEnd);
            if (selEnd > selStart)
            {
                data->DeleteChars(selStart, selEnd - selStart);
                data->CursorPos = selStart;
            }
            std::string& insert = request->text;
            if (!context.multiline)
                std::replace(insert.begin(), insert.end(), '\n', ' ');
            int bytes = static_cast<int>(std::min<std::size_t>(insert.size(), INT_MAX / 2));
            if (context.maxLength > 0)
            {
                const int room = std::max(0, context.maxLength - CountCodePoints(data->Buf, data->BufTextLen));
                bytes = CodePointBytes(insert.data(), bytes, room);
            }
            if (context.text == nullptr)
            {
                // A fixed buffer: cut at a code point that still fits with the terminator.
                const int room = std::max(0, data->BufSize - 1 - data->BufTextLen);
                if (bytes > room)
                    bytes = SnapToCodePoint(insert.data(), bytes, room);
            }
            if (bytes > 0)
                data->InsertChars(data->CursorPos, insert.data(), insert.data() + bytes);
            data->SelectionStart = data->SelectionEnd = data->CursorPos;
        }
    }
    if (!request->focus && !request->selection && !request->insert)
        ClearTextFieldRequest(*request);
}

int TextFieldCallback(ImGuiInputTextCallbackData* data)
{
    TextFieldCallbackContext& context = *static_cast<TextFieldCallbackContext*>(data->UserData);
    const auto forward = [&]() {
        data->UserData = context.userData;
        const int result = context.user(data);
        data->UserData = &context;
        return result;
    };
    switch (data->EventFlag)
    {
    case ImGuiInputTextFlags_CallbackResize:
        if (context.text != nullptr)
        {
            std::string& text = *context.text;
            try
            {
                text.resize(static_cast<std::size_t>(data->BufTextLen));
            }
            catch (...)
            {
                // Out of memory: keep what fits in the string as it is (shrinking never allocates).
                context.resizeFailed = true;
                const std::size_t keep = std::min(static_cast<std::size_t>(data->BufTextLen), text.capacity());
                text.resize(keep);
                data->BufTextLen = static_cast<int>(keep);
            }
            data->Buf = text.data();
            data->BufSize = static_cast<int>(std::min<std::size_t>(text.capacity() + 1, INT_MAX));
        }
        return 0;
    case ImGuiInputTextFlags_CallbackCharFilter:
        if ((context.userEvents & ImGuiInputTextFlags_CallbackCharFilter) != 0 && context.user != nullptr)
        {
            if (forward() != 0 || data->EventChar == 0)
                return 1;
        }
        if (context.maxLength > 0)
        {
            if (context.remaining == INT_MIN)
            {
                // Count once per call: the text, minus the selection a typed character replaces.
                context.remaining = context.maxLength;
                if (ImGuiInputTextState* state = ImGui::GetInputTextState(context.id))
                {
                    const char* text = state->TextA.Data != nullptr ? state->TextA.Data : "";
                    const int start = std::min(state->GetSelectionStart(), state->GetSelectionEnd());
                    const int end = std::max(state->GetSelectionStart(), state->GetSelectionEnd());
                    context.remaining -= CountCodePoints(text, state->TextLen) - CountCodePoints(text + start, end - start);
                }
            }
            if (context.remaining <= 0)
                return 1;
            --context.remaining;
        }
        return 0;
    default:
        ApplyTextFieldRequest(context, data);
        if ((context.userEvents & data->EventFlag) != 0 && context.user != nullptr)
            return forward();
        return 0;
    }
}

// Side of a square adornment button in a field of control size `size`, in pixels.
float TextFieldButtonSide(ControlSize size)
{
    return std::round(std::max(1.0f, ControlHeight(size) - Px(T().spacing.xs) * 2.0f));
}

ImRect TextFieldSlotButtonRect(const ImRect& slot, ControlSize size, int index)
{
    const float side = TextFieldButtonSide(size);
    const float gap = Px(T().spacing.xs);
    const float x1 = slot.Max.x - static_cast<float>(index) * (side + gap);
    const float y0 = std::round(slot.GetCenter().y - side * 0.5f);
    return ImRect(x1 - side, y0, x1, y0 + side);
}

// The plain icon button look shared by the clear button and slot buttons.
void DrawTextFieldButton(const ItemState& item, Icon icon, bool selected, float iconSize)
{
    if (!item.visible)
        return;
    const Colors& c = T().colors;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const float radius = Px(T().radii.element);
    if (!item.disabled)
    {
        ImVec4 fill = WithAlpha(c.fill[1], 0.0f);
        fill = Mix(fill, c.fill[1], item.hoverAmount);
        fill = Mix(fill, c.fill[0], item.pressAmount);
        if (selected)
            fill = Mix(c.fill[2], fill, item.hoverAmount);
        FillRect(drawList, item.rect.Min, item.rect.Max, U32(fill), radius);
    }
    const ImVec4 color = item.disabled ? c.labelTertiary
                         : (selected || item.hovered || item.held) ? c.label
                                                                   : c.labelSecondary;
    const float size = std::round(std::min(iconSize, item.rect.GetWidth()));
    const ImVec2 at(std::round(item.rect.GetCenter().x - size * 0.5f), std::round(item.rect.GetCenter().y - size * 0.5f));
    DrawIcon(drawList, icon, at, size, U32(color));
    DrawItemFocusRing(item, radius);
}

// Fades the glyphs drawn since `vertexStart` toward the edges of [left, right] where the text
// continues beyond them. Solid shapes (selection, caret) keep their alpha.
void FadeTextFieldEdges(ImDrawList* drawList, int vertexStart, float left, float right, bool fadeLeft, bool fadeRight)
{
    const ImVec2 white = drawList->_Data->TexUvWhitePixel;
    const int count = drawList->VtxBuffer.Size;
    float minX = FLT_MAX;
    float maxX = -FLT_MAX;
    for (int i = vertexStart; i < count; ++i)
    {
        const ImDrawVert& v = drawList->VtxBuffer.Data[i];
        if (v.uv.x == white.x && v.uv.y == white.y)
            continue;
        minX = std::min(minX, v.pos.x);
        maxX = std::max(maxX, v.pos.x);
    }
    if (minX == FLT_MAX)
        return;
    fadeLeft = fadeLeft || minX < left - 0.5f;
    fadeRight = fadeRight || maxX > right - 0.5f;
    if (!fadeLeft && !fadeRight)
        return;
    const float length = std::max(1.0f, std::min(Px(T().scroll.edgeFade), (right - left) * 0.5f));
    for (int i = vertexStart; i < count; ++i)
    {
        ImDrawVert& v = drawList->VtxBuffer.Data[i];
        if (v.uv.x == white.x && v.uv.y == white.y)
            continue;
        float factor = 1.0f;
        if (fadeLeft)
            factor = std::min(factor, std::clamp((v.pos.x - left) / length, 0.0f, 1.0f));
        if (fadeRight)
            factor = std::min(factor, std::clamp((right - v.pos.x) / length, 0.0f, 1.0f));
        if (factor < 1.0f)
            v.col = ScaleAlpha(v.col, factor);
    }
}

// The message line under a field: the error (with the error icon) or the help text, wrapped
// across `width`. Returns its height; draws only when `draw`.
float TextFieldMessage(const char* message, bool error, bool disabled, ImVec2 at, float width, bool draw)
{
    const Colors& c = T().colors;
    TextFieldFontScope font(TextStyle::Footnote);
    ImGuiContext& g = *GImGui;
    const float iconSize = error ? std::round(g.FontSize) : 0.0f;
    const float gap = error ? Px(T().spacing.xs) : 0.0f;
    const float wrap = std::max(1.0f, width - iconSize - gap);
    const ImVec2 size = ImGui::CalcTextSize(message, nullptr, false, wrap);
    if (draw)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec4 color = error ? c.destructive : (disabled ? c.labelTertiary : c.labelSecondary);
        if (error)
            DrawIcon(drawList, Icon("error"), ImVec2(at.x, at.y), iconSize, U32(color));
        drawList->AddText(g.Font, g.FontSize, ImVec2(at.x + iconSize + gap, at.y), U32(color), message, nullptr, wrap);
    }
    return std::max(size.y, iconSize);
}

} // namespace

// ---- The field -------------------------------------------------------------------------------

TextFieldResult TextFieldCall(const char* label, std::string* text, char* buffer, std::size_t bufferSize,
                              const TextFieldParams& params, const TextFieldHooks& hooks)
{
    TextFieldResult result;
    ImGuiContext* context = ImGui::GetCurrentContext();
    if (context == nullptr || !context->WithinFrameScope || context->CurrentWindow == nullptr)
        return result;
    ImGuiContext& g = *context;
    ImGuiWindow* window = g.CurrentWindow;
    if (window->SkipItems)
        return result;
    if (label == nullptr)
        label = "";

    const Theme& t = T();
    const Colors& c = t.colors;
    const NextItemData next = TakeNextItemData();
    const ForcedItemRequest forced = TakeForcedItemState();
    const bool forcedField = forced.state != ForcedItemState::None && forced.part < 0;
    const bool forcedFocus = forcedField && Has(forced.state, ForcedItemState::Focused);
    const bool forcedHover = forcedField && Has(forced.state, ForcedItemState::Hovered);
    result.forced = forcedField ? forced.state : ForcedItemState::None;

    const ImGuiID id = window->GetID(label);
    result.id = id;
    const char* labelEnd = ImGui::FindRenderedTextEnd(label);
    const bool hasLabel = labelEnd != label;
    const ControlSize size = ResolveControlSize(params.size);
    const bool disabled = (next.itemFlags & ImGuiItemFlags_Disabled) != 0;
    const bool multiline = params.multiline && !params.password;
    const bool masked = params.password && !params.revealed;
    const bool floating = params.floatingLabel && hasLabel && !multiline;
    const bool mixed = params.mixed || (next.itemFlags & ImGuiItemFlags_MixedValue) != 0;

    // The buffer Dear ImGui edits.
    char emptyBuffer[1] = {0};
    char* buf = emptyBuffer;
    int bufSize = 1;
    bool writable = true;
    if (text != nullptr)
    {
        buf = text->data();
        bufSize = static_cast<int>(std::min<std::size_t>(text->capacity() + 1, INT_MAX));
    }
    else if (buffer != nullptr && bufferSize > 0)
    {
        bufSize = static_cast<int>(std::min<std::size_t>(bufferSize, INT_MAX));
        if (std::memchr(buffer, 0, static_cast<std::size_t>(bufSize)) == nullptr)
        {
            IM_ASSERT_USER_ERROR(false, "TextFieldBehavior: the buffer is not zero terminated");
            buffer[bufSize - 1] = 0;
        }
        buf = buffer;
    }
    else
    {
        writable = false;  // null text or an empty buffer: an empty read-only field
    }
    const bool readOnly = params.readOnly || !writable || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;

    TextFieldGeometry& geometry = GetState<TextFieldGeometry>(id);
    TextFieldRequest* request = FindTextFieldRequest(id);
    if (request != nullptr && disabled)
    {
        ClearTextFieldRequest(*request);
        request = nullptr;
    }
    // A drag part with this id may still hold the active id (Enter pressed on a drag field): the
    // text input takes over from it.
    if (hooks.activate && g.ActiveId == id)
        ImGui::ClearActiveID();
    const bool activeBefore = g.ActiveId == id;
    ImGuiInputTextState* stateBefore = activeBefore ? ImGui::GetInputTextState(id) : nullptr;

    // Text the caller changed while the field has the keyboard replaces the edit text.
    if (stateBefore != nullptr && hooks.trackExternalChanges && geometry.hashValid && !readOnly && !hooks.dragMode)
    {
        const int length = static_cast<int>(std::strlen(buf));
        if (length != geometry.textLength || ImHashStr(buf, static_cast<std::size_t>(length)) != geometry.textHash)
            stateBefore->ReloadUserBufAndKeepSelection();
    }

    TextFieldFontScope font(ControlTextStyle(size));
    const float fontSize = g.FontSize;
    float captionSize = fontSize;
    if (floating)
    {
        TextFieldFontScope caption(TextStyle::Caption);
        captionSize = g.FontSize;
    }

    // ---- Geometry ----
    const float pad = params.frameless ? 0.0f : Px(t.field.padding);
    const float gapXs = Px(t.spacing.xs);
    const float gapSm = Px(t.spacing.sm);
    const float controlHeight = ControlHeight(size);
    const float caretWidth = std::max(1.0f, std::round(Px(t.field.caretWidth)));
    const float iconSize = std::round(fontSize);
    const float buttonSide = TextFieldButtonSide(size);

    float frameHeight = controlHeight;
    float padY = std::round(CenteredTextY(0.0f, controlHeight));
    if (floating)
    {
        frameHeight = std::round(std::max(controlHeight, gapXs + captionSize + fontSize + gapSm));
        padY = std::round(gapXs + captionSize);
    }
    else if (multiline)
    {
        padY = std::round((controlHeight - fontSize) * 0.5f);
        const float lines = std::round(fontSize) * 3.0f + padY * 2.0f;
        frameHeight = std::round(params.height > 0.0f ? std::max(Px(params.height), controlHeight) : lines);
    }

    const float labelWidth = (hasLabel && !floating) ? std::round(ImGui::CalcTextSize(label, labelEnd).x + gapSm) : 0.0f;
    const float leftInset = pad + (params.icon.IsValid() ? iconSize + gapSm : 0.0f);
    const bool leadingSpinner = params.loading && params.spinnerReplacesIcon && params.icon.IsValid();
    const bool trailingSpinner = params.loading && !leadingSpinner;
    const bool trailingButtons = params.trailingWidth > 0.0f || params.clearButton || trailingSpinner;
    float rightInset = trailingButtons ? gapXs : pad;
    const float slotWidth = params.trailingWidth > 0.0f ? std::round(Px(params.trailingWidth)) : 0.0f;
    if (slotWidth > 0.0f)
        rightInset += slotWidth + gapXs;
    if (params.clearButton)
        rightInset += buttonSide + gapXs;
    if (trailingSpinner)
        rightInset += buttonSide + gapXs;
    const float buttonsInset = rightInset;  // the buttons and the end padding, the suffix not included
    float suffixWidth = 0.0f;
    if (params.suffix != nullptr && params.suffix[0] != 0)
    {
        suffixWidth = ImGui::CalcTextSize(params.suffix).x;
        rightInset += suffixWidth + gapSm;
    }

    const float totalWidth = ResolveItemWidth(params.width, next, kFill);
    // Never narrower than one em of text between the insets: the text stays clear of every adornment.
    const float frameWidth = std::round(std::max(totalWidth - labelWidth, leftInset + rightInset + fontSize));

    ImGui::BeginGroup();
    const ImVec2 origin = window->DC.CursorPos;
    const ImRect frame(origin.x + labelWidth, origin.y, origin.x + labelWidth + frameWidth, origin.y + frameHeight);
    result.frame = frame;
    const float textLeft = frame.Min.x + leftInset;
    const float textRight = frame.Max.x - rightInset;
    const float textTop = frame.Min.y + padY;
    result.text = ImRect(textLeft, frame.Min.y, textRight, frame.Max.y);
    // InputTextEx works on the frame up to the trailing adornments, so the icon and the left
    // padding take clicks too; its frame padding puts the text after the icon.
    const ImRect textFrame(frame.Min.x, frame.Min.y, textRight, frame.Max.y);

    ImDrawList* drawList = window->DrawList;
    const float radius = Px(t.radii.control);
    const bool nonEmptyBefore = stateBefore != nullptr ? stateBefore->TextLen > 0 : buf[0] != 0;

    // ---- Frame, under the text ----
    const bool pointerOverFrame = ImGui::IsMouseHoveringRect(frame.Min, frame.Max) && g.HoveredWindow == window &&
                                  (g.ActiveId == 0 || g.ActiveId == id || g.ActiveIdAllowOverlap);
    const float hoverAmount =
        Tween(id, kFieldAnimHover, (!disabled && ((pointerOverFrame && forced.state == ForcedItemState::None) || forcedHover)) ? 1.0f : 0.0f,
              t.motion.durationFast);
    if (!params.frameless)
    {
        ImVec4 fill = disabled ? c.fill[3] : (readOnly ? WithAlpha(c.fill[3], 0.0f) : Mix(c.fill[2], c.fill[1], hoverAmount));
        FillRect(drawList, frame.Min, frame.Max, U32(fill), radius);
        ImVec4 outline = disabled ? c.separator : ControlBoundaryColor(c);
        if ((activeBefore || forcedFocus) && !disabled)
            outline = c.focusRing;
        if (params.error != nullptr)
            outline = c.destructive;
        StrokeRect(drawList, frame.Min, frame.Max, U32(outline), radius, HairlineThickness());
    }

    // ---- The label in front ----
    if (hasLabel && !floating)
        drawList->AddText(ImVec2(origin.x, textTop), U32(disabled ? c.labelTertiary : c.label), label, labelEnd);

    // ---- Text ----
    const bool showDisplayText = params.displayText != nullptr && !activeBefore && !hooks.activate && !params.hideText;
    const bool showMixed = mixed && !activeBefore && !hooks.activate && !params.hideText;
    const ImVec4 textColor = disabled ? c.labelTertiary : c.label;
    const ImVec4 placeholderColor = disabled ? c.labelTertiary : c.labelSecondary;
    const float floatTarget = (activeBefore || nonEmptyBefore || forcedFocus || hooks.activate) ? 1.0f : 0.0f;
    const float floatAmount = floating ? Spring(id, kFieldAnimFloat, floatTarget, t.motion.quick) : 1.0f;
    const char* hint = params.placeholder;
    // The placeholder shows once the floating label has made room for it.
    if (floating && (floatTarget < 0.5f || floatAmount < 0.9f))
        hint = nullptr;
    if (params.hideText)
        hint = nullptr;
    if (showMixed)
        hint = GetStrings().mixed;

    ImGuiInputTextFlags flags = params.flags;
    // Facet decides these itself.
    flags &= ~(ImGuiInputTextFlags_CallbackResize | ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_Password |
               ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_WordWrap | ImGuiInputTextFlags_EscapeClearsAll |
               ImGuiInputTextFlags_Multiline | ImGuiInputTextFlags_TempInput);
    const ImGuiInputTextFlags callbackEvents = ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory |
                                               ImGuiInputTextFlags_CallbackAlways | ImGuiInputTextFlags_CallbackCharFilter |
                                               ImGuiInputTextFlags_CallbackEdit;
    if (params.callback == nullptr)
        flags &= ~callbackEvents;
    if (multiline)
        flags &= ~(ImGuiInputTextFlags_CallbackHistory | ImGuiInputTextFlags_ElideLeft);
    if ((flags & ImGuiInputTextFlags_AllowTabInput) != 0)
        flags &= ~ImGuiInputTextFlags_CallbackCompletion;
    TextFieldCallbackContext callbackContext;
    callbackContext.text = (text != nullptr && writable) ? text : nullptr;
    callbackContext.user = params.callback;
    callbackContext.userData = params.callbackUser;
    callbackContext.userEvents = flags & callbackEvents;
    callbackContext.maxLength = params.maxLength;
    callbackContext.id = id;
    callbackContext.request = request;
    callbackContext.readOnly = readOnly;
    callbackContext.multiline = multiline;
    if (readOnly)
        flags |= ImGuiInputTextFlags_ReadOnly;
    if (masked)
        flags |= ImGuiInputTextFlags_Password;
    if (multiline)
    {
        flags |= ImGuiInputTextFlags_Multiline;
        if (params.wordWrap)
            flags |= ImGuiInputTextFlags_WordWrap;
    }
    if (callbackContext.text != nullptr)
        flags |= ImGuiInputTextFlags_CallbackResize;
    if (params.selectAllOnFocus)
        flags |= ImGuiInputTextFlags_AutoSelectAll;
    if (params.escapeClears)
        flags |= ImGuiInputTextFlags_EscapeClearsAll;
    if (params.maxLength > 0)
        flags |= ImGuiInputTextFlags_CallbackCharFilter;
    if (request != nullptr)
        flags |= ImGuiInputTextFlags_CallbackAlways;

    // The list keys go to an open completion level first.
    if (params.completionPopup != 0 && activeBefore && IsPopupLevelOpen(params.completionPopup))
    {
        const ImGuiKey listKeys[] = {ImGuiKey_UpArrow, ImGuiKey_DownArrow, ImGuiKey_Enter, ImGuiKey_KeypadEnter,
                                     ImGuiKey_Tab, ImGuiKey_Escape};
        for (const ImGuiKey key : listKeys)
            ImGui::SetKeyOwner(key, params.completionPopup, ImGuiInputFlags_LockThisFrame);
    }

    bool changed = false;
    ImGuiItemStatusFlags fieldStatus = 0;
    ItemState dragItem;
    const int vertexStart = drawList->VtxBuffer.Size;
    const ImVec2 maxPosBackup = window->DC.CursorMaxPos;
    // The text clip: the text area, plus the caret width at the end so a caret after the last
    // visible character still shows.
    const ImVec2 clipMin(textLeft, frame.Min.y);
    const ImVec2 clipMax(textRight + caretWidth, frame.Max.y);
    const float scrollbarSize = Px(t.scroll.barThickness) + Px(t.scroll.barInset) * 2.0f;
    {
        TextFieldStyleScope scope;
        const ImU32 clear = 0;
        const bool hideText = masked || showDisplayText || showMixed || params.hideText;
        ImGui::PushStyleColor(ImGuiCol_FrameBg, clear);
        ImGui::PushStyleColor(ImGuiCol_Border, clear);
        ImGui::PushStyleColor(ImGuiCol_BorderShadow, clear);
        ImGui::PushStyleColor(ImGuiCol_NavCursor, clear);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, clear);
        ImGui::PushStyleColor(ImGuiCol_Text, hideText ? WithAlpha(textColor, 0.0f) : textColor);
        ImGui::PushStyleColor(ImGuiCol_TextDisabled, showMixed ? WithAlpha(placeholderColor, 0.0f) : placeholderColor);
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, params.hideText ? WithAlpha(c.accentSubtle, 0.0f) : c.accentSubtle);
        ImGui::PushStyleColor(ImGuiCol_InputTextCursor, params.hideText ? WithAlpha(c.label, 0.0f) : c.label);
        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, clear);
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, c.scrollThumb);
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, c.scrollThumbHover);
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, c.scrollThumbHover);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, radius);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(leftInset, padY));
        // The label InputTextEx draws after its frame lands beyond the text clip, out of sight.
        ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(caretWidth + gapXs, g.Style.ItemInnerSpacing.y));
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, scrollbarSize);
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, Px(t.radii.element));
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarPadding, Px(t.scroll.barInset));
        // Facet picks the disabled colors itself, so the alpha of BeginDisabled is undone.
        if (disabled)
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, g.DisabledAlphaBackup);
        g.Style.InputTextCursorSize = caretWidth;
        if (hooks.liveEdit >= 0)
            ImGui::PushItemFlag(ImGuiItemFlags_LiveEditOnInputText, hooks.liveEdit != 0);
        if ((next.itemFlags & ImGuiItemFlags_MixedValue) != 0)
            ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, false);  // Facet shows mixed itself
        // A focus request rides on Dear ImGui's reactivation path (a click would do the same).
        // The scope puts the previous value back; nothing can have set it during an activation
        // frame, which ignores Enter.
        const bool wantFocus = !disabled && !activeBefore && (hooks.activate || (request != nullptr && request->focus));
        if (wantFocus)
            g.InputTextReactivateId = id;
        else
            scope.KeepReactivation();

        window->DC.CursorPos = textFrame.Min;
        drawList->PushClipRect(clipMin, clipMax, true);
        ApplyNextItemData(next);
        if (hooks.dragMode)
        {
            ImGui::ItemSize(textFrame.GetSize(), padY);
            ItemOptions options;
            options.handCursor = false;
            options.hitTarget = false;
            options.forced = &forced;
            // The suffix drags too: the whole frame up to the buttons is the handle.
            const ImRect dragFrame(frame.Min.x, frame.Min.y, std::max(textRight, frame.Max.x - buttonsInset), frame.Max.y);
            dragItem = PartBehavior(id, dragFrame, options);
            if (hooks.dragItem != nullptr)
                *hooks.dragItem = dragItem;
            if ((dragItem.hovered || dragItem.held) && !dragItem.disabled)
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            if (params.hideText)
            {
                // The widget draws the text.
            }
            else if (showMixed)
                drawList->AddText(ImVec2(textLeft, textTop), U32(placeholderColor), GetStrings().mixed);
            else if (params.displayText != nullptr || buf[0] != 0)
                drawList->AddText(ImVec2(textLeft, textTop), U32(textColor), params.displayText != nullptr ? params.displayText : buf);
            else if (hint != nullptr)
                drawList->AddText(ImVec2(textLeft, textTop), U32(placeholderColor), hint);
        }
        else
        {
            changed = ImGui::InputTextEx(label, hint, buf, bufSize, textFrame.GetSize(), flags, TextFieldCallback,
                                         &callbackContext);
            fieldStatus = g.LastItemData.StatusFlags;
        }
        if (text != nullptr)
            buf = text->data();  // the resize callback may have moved it

        ImGuiInputTextState* state = g.ActiveId == id ? ImGui::GetInputTextState(id) : nullptr;
        // What InputTextEx could not draw: the display text, the mixed text, the dots.
        float maskedWidth = 0.0f;
        if (!hooks.dragMode)
        {
            if (showDisplayText)
                drawList->AddText(ImVec2(textLeft, textTop), U32(textColor), params.displayText);
            else if (showMixed)
                drawList->AddText(ImVec2(textLeft, textTop), U32(placeholderColor), GetStrings().mixed);
            else if (masked && !params.hideText)
            {
                const char* shown = state != nullptr && state->TextA.Data != nullptr ? state->TextA.Data : buf;
                const int length = state != nullptr ? state->TextLen : static_cast<int>(std::strlen(buf));
                const int count = CountCodePoints(shown, length);
                ImFontBaked* baked = g.FontBaked;
                const ImFontGlyph* star = baked != nullptr ? baked->FindGlyph('*') : nullptr;
                const ImFontGlyph* bullet = baked != nullptr ? baked->FindGlyphNoFallback(kMaskBullet) : nullptr;
                if (star != nullptr && count > 0)
                {
                    const float advance = star->AdvanceX;
                    maskedWidth = advance * static_cast<float>(count);
                    const ImWchar glyph = bullet != nullptr ? kMaskBullet : static_cast<ImWchar>('*');
                    const float glyphAdvance = bullet != nullptr ? bullet->AdvanceX : advance;
                    const float scroll = state != nullptr ? state->Scroll.x : 0.0f;
                    const int first = std::max(0, static_cast<int>(std::floor(scroll / std::max(advance, 1.0f))));
                    const int last = std::min(count, static_cast<int>(std::ceil((scroll + textRight - textLeft) / std::max(advance, 1.0f))) + 1);
                    for (int i = first; i < last; ++i)
                    {
                        const float x = textLeft - scroll + static_cast<float>(i) * advance + (advance - glyphAdvance) * 0.5f;
                        g.Font->RenderChar(drawList, fontSize, ImVec2(x, textTop), U32(textColor), glyph);
                    }
                }
            }
            if (forcedFocus && state == nullptr && !params.hideText)
            {
                // The focused look of a capture: a caret after the text, where typing would go.
                const char* shown = showDisplayText ? params.displayText : (showMixed || masked ? "" : buf);
                const float width = masked && !showDisplayText ? maskedWidth : ImGui::CalcTextSize(shown).x;
                const float x = std::min(std::round(textLeft + width), textRight);
                drawList->AddRectFilled(ImVec2(x, textTop), ImVec2(x + caretWidth, textTop + fontSize), U32(c.label));
            }
        }
        if (!multiline)
            FadeTextFieldEdges(drawList, vertexStart, textLeft, textRight, state != nullptr && state->Scroll.x > 0.0f, false);
        drawList->PopClipRect();

        // Geometry for the caret and selection queries.
        geometry.frame = ImGui::GetFrameCount();
        geometry.origin = ImVec2(textFrame.Min.x + leftInset, textFrame.Min.y + padY);
        geometry.font = g.Font;
        geometry.fontSize = fontSize;
        geometry.caretWidth = caretWidth;
        geometry.multiline = multiline;
        geometry.readOnlyText = !writable ? "" : (readOnly ? buf : nullptr);
        geometry.maskAdvance = 0.0f;
        if (masked && g.FontBaked != nullptr)
            if (const ImFontGlyph* star = g.FontBaked->FindGlyph('*'))
                geometry.maskAdvance = star->AdvanceX;
    }
    window->DC.CursorMaxPos = ImMax(maxPosBackup, frame.Max);

    ImGuiInputTextState* state = g.ActiveId == id ? ImGui::GetInputTextState(id) : nullptr;
    const bool activeNow = state != nullptr;
    result.active = activeNow;
    result.edited = changed;
    if (!hooks.dragMode)
    {
        ImGuiInputTextState* any = ImGui::GetInputTextState(id);
        result.submitted = any != nullptr && any->ValidatedThisFrame && (activeBefore || activeNow);
    }
    if (request != nullptr && request->id == id && activeNow)
    {
        request->focus = false;
        if (!request->selection && !request->insert)
            ClearTextFieldRequest(*request);
    }
    if (activeNow)
    {
        const int length = static_cast<int>(std::strlen(buf));
        geometry.textLength = length;
        geometry.textHash = ImHashStr(buf, static_cast<std::size_t>(length));
        geometry.hashValid = true;
    }
    else
    {
        geometry.hashValid = false;
    }
    // Escape that ended the edit and restored the text is a cancel (Escape that only cleared a
    // search field is an edit).
    if (activeBefore && !activeNow && !params.escapeClears && !result.submitted && ImGui::IsKeyPressed(ImGuiKey_Escape, false))
        MarkEditCanceled(id);

    const bool nonEmpty = state != nullptr ? state->TextLen > 0 : buf[0] != 0;
    const ImVec4 secondary = disabled ? c.labelTertiary : c.labelSecondary;

    // ---- Leading icon, suffix, spinner ----
    const float spinnerThickness = std::max(HairlineThickness(), std::round(iconSize * kSpinnerStrokeShare));
    if (leadingSpinner)
        DrawSpinner(drawList, ImVec2(std::round(frame.Min.x + pad + iconSize * 0.5f), std::round(frame.GetCenter().y)),
                    iconSize * 0.5f - spinnerThickness * 0.5f, U32(secondary), spinnerThickness);
    else if (params.icon.IsValid())
        DrawIcon(drawList, params.icon, ImVec2(frame.Min.x + pad, std::round(frame.GetCenter().y - iconSize * 0.5f)),
                 iconSize, U32(secondary));
    float trailingX = frame.Max.x - (trailingButtons ? gapXs : pad);
    if (slotWidth > 0.0f)
    {
        result.trailing = ImRect(trailingX - slotWidth, frame.Min.y, trailingX, frame.Max.y);
        trailingX -= slotWidth + gapXs;
    }
    ImRect clearRect;
    if (params.clearButton)
    {
        clearRect = TextFieldSlotButtonRect(ImRect(trailingX - buttonSide, frame.Min.y, trailingX, frame.Max.y), size, 0);
        trailingX -= buttonSide + gapXs;
    }
    if (trailingSpinner)
    {
        const ImVec2 center(std::round(trailingX - buttonSide * 0.5f), std::round(frame.GetCenter().y));
        DrawSpinner(drawList, center, iconSize * 0.5f - spinnerThickness * 0.5f, U32(secondary), spinnerThickness);
        trailingX -= buttonSide + gapXs;
    }
    if (suffixWidth > 0.0f)
        drawList->AddText(ImVec2(trailingX - suffixWidth, textTop), U32(secondary), params.suffix);

    // ---- Floating label ----
    if (floating)
    {
        const float restY = std::round(CenteredTextY(frame.Min.y, frameHeight));
        const float floatY = std::round(frame.Min.y + gapXs);
        const float y = restY + (floatY - restY) * floatAmount;
        const float labelSize = fontSize + (captionSize - fontSize) * floatAmount;
        drawList->PushClipRect(frame.Min, ImVec2(textRight, frame.Max.y), true);
        drawList->AddText(g.Font, labelSize, ImVec2(textLeft, y), U32(secondary), label, labelEnd);
        drawList->PopClipRect();
    }

    // ---- Adornments (parts after the text, so Tab reaches the text first) ----
    bool clearPressed = false;
    if (params.clearButton && nonEmpty && !readOnly && !disabled)
    {
        ItemOptions options;
        options.tabStop = false;
        options.forced = &forced;
        options.part = 0;
        const ImGuiID clearId = ImGui::GetIDWithSeed("clear", nullptr, id);
        const ItemState item = PartBehavior(clearId, clearRect, options);
        DrawTextFieldButton(item, Icon("clear"), false, iconSize);
        ImGui::SetItemTooltip("%s", GetStrings().clear);
        result.clear = clearRect;
        if (item.pressed)
        {
            if (state != nullptr)
            {
                buf[0] = 0;
                if (text != nullptr)
                    text->clear();
                state->ReloadUserBufAndSelectAll();
            }
            else if (text != nullptr)
            {
                text->clear();
                buf = text->data();
            }
            else
            {
                buf[0] = 0;
            }
            ImGui::MarkItemEdited(clearId);
            clearPressed = true;
            TextFieldRequestFocus(id, TextFieldFocus::End);
        }
    }
    if (hooks.dragLabel && hasLabel && !floating)
    {
        // The visible label drags the value too (drag fields); it is not a focus stop of its own.
        ItemOptions options;
        options.navigable = false;
        options.tabStop = false;
        options.handCursor = false;
        options.hitTarget = false;
        options.forced = &forced;
        options.part = -2;
        const ImRect labelRect(origin.x, frame.Min.y, origin.x + std::max(1.0f, labelWidth - gapSm), frame.Max.y);
        const ItemState item = PartBehavior(ImGui::GetIDWithSeed("label", nullptr, id), labelRect, options);
        if ((item.hovered || item.held) && !item.disabled && !readOnly)
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        if (hooks.labelItem != nullptr)
            *hooks.labelItem = item;
    }
    if (params.trailingSlot != nullptr && slotWidth > 0.0f)
    {
        TextFieldSlot slot;
        slot.rect = result.trailing;
        slot.fieldId = id;
        slot.fieldActive = activeNow;
        slot.disabled = disabled;
        slot.readOnly = readOnly;
        slot.size = size;
        slot.forced = &forced;
        params.trailingSlot(slot, params.trailingUser);
    }

    // The whole frame takes the pointer: where no part is (the suffix strip, the padding at the
    // end), a click gives the text the keyboard, as a click on the text would.
    const bool overFrame = ImGui::IsMouseHoveringRect(frame.Min, frame.Max) && g.HoveredWindow == window;
    result.hovered = overFrame;
    if (overFrame && disabled)
        SetNotAllowedCursor();
    else if (overFrame && g.HoveredId == 0 && forced.state == ForcedItemState::None)
    {
        ImGui::SetMouseCursor(hooks.dragMode ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_TextInput);
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            if (hooks.dragMode)
            {
                if (hooks.frameClicked != nullptr)
                    *hooks.frameClicked = true;
            }
            else
            {
                TextFieldRequestFocus(id, TextFieldFocus::End);
            }
        }
    }

    // ---- Focus ring ----
    const bool focusVisible = (!disabled && g.NavId == id && KeyboardModality()) || forcedFocus;
    const float focusAmount = Tween(id, kFieldAnimFocus, focusVisible ? 1.0f : 0.0f, t.motion.durationFast);
    if (focusAmount > 0.0f && !params.frameless)
        DrawFocusRing(drawList, frame, radius, focusAmount);

    // ---- Message line ----
    float bottom = frame.Max.y;
    const char* message = params.error != nullptr ? params.error : params.helpText;
    if (message != nullptr && message[0] != 0)
    {
        const ImVec2 at(frame.Min.x, frame.Max.y + gapXs);
        bottom = at.y + TextFieldMessage(message, params.error != nullptr, disabled, at, frame.GetWidth(), true);
    }
    window->DC.CursorMaxPos = ImMax(window->DC.CursorMaxPos, ImVec2(frame.Max.x, bottom));
    ImGui::EndGroup();
    FinishWidgetGroup(id);
    // An edit applied after the field lost the keyboard (Tab to another field) is not seen by the
    // group; carry the field's own flag over.
    g.LastItemData.StatusFlags |= fieldStatus & ImGuiItemStatusFlags_Edited;

    if (clearPressed)
    {
        result.edited = true;
        result.clearPressed = true;
    }
    const char* finalText = state != nullptr && state->TextA.Data != nullptr && !readOnly ? state->TextA.Data : buf;
    const int finalLength = state != nullptr && !readOnly ? state->TextLen : static_cast<int>(std::strlen(buf));
    result.length = CountCodePoints(finalText, finalLength);
    if (multiline)
    {
        // The visual lines as InputTextEx lays them out: its wrap width is the area without
        // the padding and the room it keeps for the scrollbar.
        TextFieldLayout layout;
        layout.text = finalText;
        layout.length = finalLength;
        layout.font = geometry.font;
        layout.size = fontSize;
        layout.multiline = true;
        layout.wrap = params.wordWrap ? std::max(1.0f, textFrame.GetWidth() - leftInset - scrollbarSize) : 0.0f;
        int lines = 0;
        layout.ForEachLine([&](int, int, int) {
            ++lines;
            return true;
        });
        result.lineCount = std::max(1, lines);
    }
    if (!TextFieldCaretRect(id, &result.caret))
        result.caret = ImRect(textLeft, textTop, textLeft, textTop + fontSize);
    result.committed = IsItemEditCommitted();
    result.canceled = IsItemEditCanceled();
    if (callbackContext.resizeFailed)
        IM_ASSERT_USER_ERROR(false, "TextFieldBehavior: the text could not grow; it was cut");
    return result;
}

TextFieldResult TextFieldBehavior(const char* label, std::string* text, const TextFieldParams& params)
{
    return TextFieldCall(label, text, nullptr, 0, params, {});
}

TextFieldResult TextFieldBehavior(const char* label, char* buffer, std::size_t bufferSize, const TextFieldParams& params)
{
    return TextFieldCall(label, nullptr, buffer, bufferSize, params, {});
}

PopupAnchor TextFieldPopupAnchor(const TextFieldResult& field, TextFieldAnchor at)
{
    PopupAnchor anchor;
    anchor.side = PopupSide::Below;
    anchor.align = PopupAlign::Start;
    if (at == TextFieldAnchor::Field)
    {
        anchor.rect = field.frame;
        anchor.matchWidth = true;
    }
    else
    {
        anchor.rect = field.caret;
        anchor.matchWidth = false;
    }
    return anchor;
}

// ---- Slot buttons ------------------------------------------------------------------------------

ItemState TextFieldSlotButton(const TextFieldSlot& slot, int index, ImGuiID id, Icon icon,
                              const TextFieldSlotButtonOptions& options)
{
    ItemState item;
    if (ImGui::GetCurrentContext() == nullptr || ImGui::GetCurrentWindow()->SkipItems)
        return item;
    IM_ASSERT_USER_ERROR(options.name != nullptr && options.name[0] != 0, "TextFieldSlotButton: a name is required");
    const ImRect rect = TextFieldSlotButtonRect(slot.rect, slot.size, std::max(0, index));
    ItemOptions itemOptions;
    itemOptions.repeat = options.repeat;
    itemOptions.press = options.repeat ? PressMode::Click : PressMode::ClickRelease;
    itemOptions.tabStop = options.tabStop;
    itemOptions.disabled = options.disabled;
    itemOptions.forced = slot.forced;
    itemOptions.part = std::max(0, index) + 1;
    item = PartBehavior(id, rect, itemOptions);
    DrawTextFieldButton(item, icon, options.selected, std::round(ImGui::GetFontSize()));
    if (options.name != nullptr && options.name[0] != 0)
        ImGui::SetItemTooltip("%s", options.name);
    return item;
}

float TextFieldSlotButtonsWidth(int count, ControlSize size)
{
    if (count <= 0)
        return 0.0f;
    const Theme& t = T();
    const ControlSize resolved = ResolveControlSize(size);
    const float height = resolved == ControlSize::Small   ? t.metrics.controlHeightSmall
                         : resolved == ControlSize::Large ? t.metrics.controlHeightLarge
                                                          : t.metrics.controlHeight;
    const float side = std::max(1.0f, height - t.spacing.xs * 2.0f);
    return side * static_cast<float>(count) + t.spacing.xs * static_cast<float>(count - 1);
}

// ---- Geometry and control ------------------------------------------------------------------------

namespace
{

// The layout of the active field `id`, or false.
bool ActiveTextFieldLayout(ImGuiID id, ImGuiInputTextState** stateOut, const TextFieldGeometry** geometryOut,
                           TextFieldLayout* layout)
{
    ImGuiContext* context = ImGui::GetCurrentContext();
    if (context == nullptr || id == 0 || context->ActiveId != id)
        return false;
    ImGuiInputTextState* state = ImGui::GetInputTextState(id);
    if (state == nullptr)
        return false;
    const TextFieldGeometry& geometry = GetState<TextFieldGeometry>(id);
    if (geometry.font == nullptr || geometry.frame < ImGui::GetFrameCount() - 1)
        return false;
    if ((state->Flags & ImGuiInputTextFlags_ReadOnly) != 0)
    {
        // The caller's text, only while it is the text of this frame's call.
        if (geometry.readOnlyText == nullptr || geometry.frame != ImGui::GetFrameCount())
            return false;
        layout->text = geometry.readOnlyText;
        layout->length = std::min(state->TextLen, static_cast<int>(std::strlen(geometry.readOnlyText)));
    }
    else
    {
        layout->text = state->TextA.Data != nullptr ? state->TextA.Data : "";
        layout->length = state->TextLen;
    }
    layout->font = geometry.font;
    layout->size = geometry.fontSize;
    layout->wrap = geometry.multiline ? state->WrapWidth : 0.0f;
    layout->multiline = geometry.multiline;
    layout->maskAdvance = geometry.maskAdvance;
    *stateOut = state;
    *geometryOut = &geometry;
    return true;
}

} // namespace

bool TextFieldCaretRect(ImGuiID id, ImRect* out)
{
    ImGuiInputTextState* state = nullptr;
    const TextFieldGeometry* geometry = nullptr;
    TextFieldLayout layout;
    if (!ActiveTextFieldLayout(id, &state, &geometry, &layout))
    {
        if (out != nullptr)
            *out = ImRect();
        return false;
    }
    int line = 0;
    float x = 0.0f;
    TextFieldLocate(layout, state->GetCursorPos(), state->LastMoveDirectionLR == ImGuiDir_Right, &line, &x);
    const ImVec2 bottom = ImTrunc(ImVec2(geometry->origin.x + x - state->Scroll.x,
                                         geometry->origin.y + static_cast<float>(line + 1) * layout.size - state->Scroll.y));
    if (out != nullptr)
        *out = ImRect(bottom.x, bottom.y - layout.size, bottom.x + geometry->caretWidth, bottom.y);
    return true;
}

int TextFieldSelectionRects(ImGuiID id, std::span<ImRect> out)
{
    ImGuiInputTextState* state = nullptr;
    const TextFieldGeometry* geometry = nullptr;
    TextFieldLayout layout;
    if (!ActiveTextFieldLayout(id, &state, &geometry, &layout) || !state->HasSelection())
        return 0;
    const int selStart = std::clamp(std::min(state->GetSelectionStart(), state->GetSelectionEnd()), 0, layout.length);
    const int selEnd = std::clamp(std::max(state->GetSelectionStart(), state->GetSelectionEnd()), 0, layout.length);
    // As InputTextEx draws it: whole pixels, and half a space for a selected line break.
    float eolWidth = 0.0f;
    if (ImFontBaked* baked = layout.font->GetFontBaked(layout.size))
        eolWidth = IM_TRUNC(baked->GetCharAdvance(static_cast<ImWchar>(' ')) * 0.5f);
    int count = 0;
    layout.ForEachLine([&](int index, int start, int end) {
        const bool hardBreak = end < layout.length && layout.text[end] == '\n';
        const int a = std::max(selStart, start);
        const int b = std::min(selEnd, end);
        float width = 0.0f;
        if (a < b)
            width += std::ceil(layout.Width(a, b));
        if (hardBreak && selStart <= end && selEnd > end)
            width += eolWidth;
        if (width > 0.0f)
        {
            const float x = geometry->origin.x - state->Scroll.x + std::ceil(layout.Width(start, std::max(start, a)));
            const float y = geometry->origin.y - state->Scroll.y + static_cast<float>(index) * layout.size;
            if (count < static_cast<int>(out.size()))
                out[static_cast<std::size_t>(count)] = ImRect(x, y, x + width, y + layout.size);
            ++count;
        }
        return start < selEnd;
    });
    return count;
}

bool TextFieldGetSelection(ImGuiID id, int* start, int* end, int* caret)
{
    ImGuiContext* context = ImGui::GetCurrentContext();
    if (context == nullptr || id == 0 || context->ActiveId != id)
        return false;
    ImGuiInputTextState* state = ImGui::GetInputTextState(id);
    if (state == nullptr)
        return false;
    const int a = state->GetSelectionStart();
    const int b = state->GetSelectionEnd();
    if (start != nullptr)
        *start = std::min(a, b);
    if (end != nullptr)
        *end = std::max(a, b);
    if (caret != nullptr)
        *caret = state->GetCursorPos();
    return true;
}

void TextFieldSetSelection(ImGuiID id, int start, int end)
{
    if (TextFieldRequest* request = TakeTextFieldRequest(id))
    {
        request->selection = true;
        request->selectionStart = start;
        request->selectionEnd = end;
    }
}

void TextFieldInsert(ImGuiID id, const char* text)
{
    if (text == nullptr)
        return;
    if (TextFieldRequest* request = TakeTextFieldRequest(id))
    {
        try
        {
            request->text.assign(text);
            request->insert = true;
        }
        catch (...)
        {
            request->insert = false;  // out of memory: the insert is dropped
        }
    }
}

void TextFieldRequestFocus(ImGuiID id, TextFieldFocus select)
{
    if (TextFieldRequest* request = TakeTextFieldRequest(id))
    {
        request->focus = true;
        request->focusMode = select;
    }
}

void ResetTextFieldRequests()
{
    for (TextFieldRequest& request : TextFieldRequests())
        ClearTextFieldRequest(request);
}

Subsystem TextFieldSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "text_field";
    subsystem.init = [] {
        ResetTextFieldRequests();
        return true;
    };
    subsystem.shutdown = [] { ResetTextFieldRequests(); };
    return subsystem;
}

} // namespace facet::internal
