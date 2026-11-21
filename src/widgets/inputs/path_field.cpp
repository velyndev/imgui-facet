// PathField: a text field on the text field core for a file or folder path. While the field does
// not have the keyboard, a path wider than the text area is shown shortened in the middle, so the
// file name at its end stays visible, with the whole path as a tooltip; the browse button in the
// trailing slot is reported through PathFieldBrowsePressed for the caller to open its own dialog.

#include "facet/widgets/inputs.h"

#include <cstring>
#include <string_view>

#include <imgui.h>
#include <imgui_internal.h>

#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/state_storage.h"
#include "internal/text_field.h"
#include "internal/text_util.h"
#include "widgets/inputs/text_inputs_shared.h"

namespace facet
{
namespace
{

// PathFieldBrowsePressed: the browse button of the last PathField call of this frame.
struct PathFieldBrowse
{
    int frame = -1;
    bool pressed = false;
};

PathFieldBrowse& LastPathFieldBrowse()
{
    static PathFieldBrowse browse;
    return browse;
}

// Kept under the field id: the width of the text area in the last frame, to shorten the path to.
struct PathFieldState
{
    float textWidth = 0.0f;
};

struct PathFieldSlot
{
    ImGuiID buttonId = 0;
    bool off = false;
    bool pressed = false;
};

void PathFieldBrowseButton(const internal::TextFieldSlot& slot, void* user)
{
    PathFieldSlot& s = *static_cast<PathFieldSlot*>(user);
    const internal::ItemState item = internal::TextFieldSlotButton(
        slot, 0, s.buttonId, Icon("folder"), {.tabStop = true, .disabled = s.off, .name = GetStrings().browse});
    s.pressed = item.pressed;
}

template <class Call>
bool PathFieldCall(const char* label, std::string_view path, bool writable, const PathFieldOptions& options, Call call)
{
    if (ImGui::GetCurrentContext() == nullptr)
        return false;
    PathFieldBrowse& browse = LastPathFieldBrowse();
    browse.frame = ImGui::GetFrameCount();
    browse.pressed = false;
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const ImGuiContext& g = *GImGui;
    const ImGuiID id = window->GetID(label);
    PathFieldState& state = internal::GetState<PathFieldState>(id);
    const bool readOnly = options.readOnly || !writable || (g.CurrentItemFlags & ImGuiItemFlags_ReadOnly) != 0;

    // Shortened in the middle while the field does not have the keyboard.
    internal::FittedText shortened;
    if (g.ActiveId != id && state.textWidth > 0.0f && !path.empty())
    {
        const internal::ControlTextScope scope(internal::GetControlMetrics(options.size));
        shortened = internal::FitText(path.data(), path.data() + path.size(), state.textWidth,
                                      internal::TruncateMode::Middle);
    }

    PathFieldSlot slot;
    slot.buttonId = ImGui::GetIDWithSeed("browse", nullptr, id);
    slot.off = readOnly;
    internal::TextFieldParams params;
    params.placeholder = options.placeholder;
    params.size = options.size;
    params.width = options.width;
    params.readOnly = options.readOnly;
    params.error = options.error;
    params.helpText = options.helpText;
    params.displayText = shortened.truncated ? shortened.text : nullptr;
    if (options.browseButton)
    {
        params.trailingWidth = internal::TextFieldSlotButtonsWidth(1, options.size);
        params.trailingSlot = PathFieldBrowseButton;
        params.trailingUser = &slot;
    }
    const internal::TextFieldResult field = call(label, params);
    state.textWidth = field.text.GetWidth();
    // `path` is only read while the text was not edited in this call (an edit may move it).
    if (shortened.truncated && !field.edited && !field.active)
    {
        const bool focusVisible = g.NavId == id && internal::KeyboardModality();
        internal::CutTextTooltip(id, field.frame, field.hovered, focusVisible, path.data(), path.data() + path.size(),
                                 true);
    }
    browse.pressed = slot.pressed && !readOnly;
    return field.edited && writable && !options.readOnly;
}

std::string_view PathFieldBufferText(const char* buffer, std::size_t bufferSize)
{
    if (buffer == nullptr || bufferSize == 0)
        return {};
    const void* zero = std::memchr(buffer, 0, bufferSize);
    return std::string_view(buffer, zero != nullptr ? static_cast<std::size_t>(static_cast<const char*>(zero) - buffer)
                                                    : bufferSize);
}

} // namespace

bool PathField(const char* label, std::string* path, const PathFieldOptions& options)
{
    return PathFieldCall(label, path != nullptr ? std::string_view(*path) : std::string_view(), path != nullptr, options,
                         [&](const char* l, const internal::TextFieldParams& p) {
                             return internal::TextFieldBehavior(l, path, p);
                         });
}

bool PathField(const char* label, char* buffer, std::size_t bufferSize, const PathFieldOptions& options)
{
    return PathFieldCall(label, PathFieldBufferText(buffer, bufferSize), buffer != nullptr && bufferSize > 0, options,
                         [&](const char* l, const internal::TextFieldParams& p) {
                             return internal::TextFieldBehavior(l, buffer, bufferSize, p);
                         });
}

bool PathFieldBrowsePressed()
{
    const PathFieldBrowse& browse = LastPathFieldBrowse();
    return ImGui::GetCurrentContext() != nullptr && browse.frame == ImGui::GetFrameCount() && browse.pressed;
}

} // namespace facet
