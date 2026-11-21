#pragma once

// Parts shared by the media widgets (facet/widgets/media.h): the radius of a Radius token, an image
// drawn into a box with its loading and failed states, avatars with their initials fallback, the
// message of an empty or failed collection, and the alt text of a label. Every name carries the
// family, because every family links into the same library.
//
//     const MediaImage picture = MediaImageOf(handle);           // status, texture and size
//     MediaDrawImageSlot(drawList, picture, box, {.fit = ImageFit::Cover, .radius = r}, "Alt text");
//
// Images are drawn on the pixel grid: a box is snapped to whole pixels, and an image shown at the
// size of its texture lands on whole pixels too, so it is sampled one texel per pixel and stays
// sharp. Rounded corners are real geometry with the texture mapped over them, so nothing of the
// image shows outside the corner.

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/icons.h"
#include "facet/core/image.h"
#include "facet/widgets/common.h"

struct ID3D11ShaderResourceView;

namespace facet::internal
{

// ---- Images -------------------------------------------------------------------------------------

// The pixels of a Radius token at the current scale (None is 0).
float MediaRadiusPx(Radius corners);

// What a handle or an app texture shows now.
struct MediaImage
{
    ImageStatus status = ImageStatus::Invalid;
    ImTextureRef texture;           // valid while Ready
    ImVec2 size = ImVec2(0.0f, 0.0f);  // texture pixels; zero while not Ready
};

MediaImage MediaImageOf(ImageHandle image);
// An app texture: Ready with its size, or Failed for a null view or one without a 2D texture.
MediaImage MediaImageOf(ID3D11ShaderResourceView* texture);

struct MediaDrawOptions
{
    ImageFit fit = ImageFit::Contain;
    float radius = 0.0f;       // pixels, clamped to half the shorter side of what is drawn
    bool circular = false;     // circular arcs whatever the radius (circles, capsules); else the theme's corners
    ImU32 tint = IM_COL32_WHITE;
    float opacity = 1.0f;      // multiplies the tint alpha (fade-ins)
};

// The rectangle a texture of `textureSize` takes in `box` for `fit`, snapped to whole pixels: an
// image as large as its texture keeps exactly its size.
ImRect MediaFitRect(const ImRect& box, ImVec2 textureSize, ImageFit fit, ImVec2* uvMin, ImVec2* uvMax);

// Draws a Ready image into `box`. Returns the rectangle it covers (the box for Cover and Fill).
ImRect MediaDrawTexture(ImDrawList* drawList, const MediaImage& image, const ImRect& box, const MediaDrawOptions& options);

// Draws whatever `image` shows into `box`: the texture when Ready, the skeleton while Loading, the
// failed marker with `altText` (zero terminated, null or empty for none) otherwise.
void MediaDrawImageSlot(ImDrawList* drawList, const MediaImage& image, const ImRect& box, const MediaDrawOptions& options,
                        const char* altText);

// ---- Text ---------------------------------------------------------------------------------------

inline constexpr int kMediaTextSize = 256;

// The visible part of `label` (up to "##"), zero terminated, cut at a whole character when longer.
struct MediaText
{
    char text[kMediaTextSize] = {};
    bool IsEmpty() const { return text[0] == '\0'; }
};
MediaText MediaVisibleText(const char* label);

// Up to two initials from `text`: the first letter of the first word and of the last word, ASCII
// letters in upper case, other characters as they are. Empty when the text has no word.
struct MediaInitials
{
    char text[16] = {};
};
MediaInitials MediaInitialsOf(const char* text);

// ---- Avatars ------------------------------------------------------------------------------------

struct MediaAvatarLook
{
    bool squircle = false;   // continuous corners instead of a circle
    bool disabled = false;
    bool selected = false;   // the accent ring inside the edge
};

// The diameter of an avatar in pixels: `units` when > 0, else the control height of `size`; even,
// so its center falls on a whole pixel.
float MediaAvatarDiameter(float units, ControlSize size);

// The corner radius of an avatar of `side` pixels.
float MediaAvatarRadius(float side, bool squircle);

// Draws one avatar into `rect` (a square): the image when Ready, a skeleton while Loading, else the
// initials of `name` (or the user icon when it has none) in label on fill[1].
void MediaDrawAvatar(ImDrawList* drawList, const ImRect& rect, const char* name, ImageHandle image,
                     const MediaAvatarLook& look);

// ---- Collections --------------------------------------------------------------------------------

// The empty or error message of a strip or carousel: an icon over a line of Footnote text in
// labelSecondary (labelTertiary while disabled), centered in `rect` and cut with an ellipsis;
// the icon is left out where there is no room for it.
void MediaDrawMessage(ImDrawList* drawList, const ImRect& rect, Icon icon, const char* text, bool disabled);

// A caption over an image (carousel slides, the corners of a comparison): Footnote text in label
// on a bgElevated chip with a hairline, so it reads over any picture in both themes. It sits in
// the corner `corner` of `area`, spacing.sm in from its edges, and is cut with an ellipsis to fit.
enum class MediaCorner
{
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
};
void MediaDrawCaption(ImDrawList* drawList, const ImRect& area, MediaCorner corner, const char* text, bool disabled);

// ---- Motion -------------------------------------------------------------------------------------

// A position that follows a target on a spring and can be grabbed: a drag sets the position
// directly, and on release the spring takes over from there with the speed of the drag, so a strip
// or a carousel glides on and snaps without a jump. The animation store starts every key at its
// target, which is why these widgets keep this pair in their state instead. The spring is solved in
// closed form for the frame time (frame-rate independent); its response is multiplied by
// motion.scale, and with reduced motion the position jumps to the target.
struct MediaSpringValue
{
    float position = 0.0f;
    float velocity = 0.0f;   // units of position per second
    bool started = false;    // false: the first step jumps to the target
};

// Advances `value` toward `target` by `dt` seconds on `spec`.
void MediaSpringStep(MediaSpringValue& value, float target, const SpringSpec& spec, float dt);
// Holds the value at `position` (a drag), tracking its speed from the movement over `dt`.
void MediaSpringHold(MediaSpringValue& value, float position, float dt);

// ---- Keys and the wheel -------------------------------------------------------------------------

// For a focused item `id`: takes `key` from keyboard navigation and returns true when it was
// pressed this frame (with repeat).
bool MediaTakeKey(ImGuiID id, ImGuiKey key);

// The mouse wheel for item `id` while the pointer is over `rect`: the item claims the wheel and
// returns the notches it turned (y positive away from the user, x positive to the left; with Shift
// the vertical wheel turns into x), once it owned it from the previous frame. Without `vertical`
// only the sideways wheel and Shift+wheel are claimed, so the plain wheel still scrolls the window.
// A scroll gesture still running keeps the wheel. Zero otherwise.
ImVec2 MediaClaimWheel(ImGuiID id, const ImRect& rect, bool vertical = true);

} // namespace facet::internal
