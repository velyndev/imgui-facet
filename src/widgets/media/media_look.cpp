// The parts shared by the media widgets (media_look.h).

#include "widgets/media/media_look.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <d3d11.h>
#include <wrl/client.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "image/image_internal.h"
#include "internal/control.h"
#include "internal/loading.h"
#include "internal/scroll.h"

namespace facet::internal
{
namespace
{

// The corner radius of a squircle avatar as a share of its side: round enough to read as a soft
// square next to circles, square enough not to be taken for one.
constexpr float kMediaSquircleShare = 0.3f;
// Initials take the largest text style whose size is at most this share of the avatar's side, so
// two letters fit inside the circle with room around them.
constexpr float kMediaInitialsShare = 0.42f;
// The user icon of an avatar without initials covers this share of its side.
constexpr float kMediaAvatarIconShare = 0.55f;
// The selection ring of an avatar is this many hairlines wide, and the image keeps one hairline of
// gap inside it, so the ring reads apart from the picture at every scale.
constexpr float kMediaSelectedRingHairlines = 2.0f;

// The speed of a held position is smoothed at this rate per second (about two frames at 60 Hz),
// so one uneven frame does not decide a flick.
constexpr float kMediaVelocitySmoothing = 30.0f;

// Text styles from the largest to the smallest, for picking the initials size.
constexpr TextStyle kMediaInitialStyles[] = {TextStyle::Title1, TextStyle::Title2, TextStyle::Title3,
                                             TextStyle::Headline, TextStyle::Footnote, TextStyle::Caption};

} // namespace

float MediaRadiusPx(Radius corners)
{
    const Radii& r = T().radii;
    switch (corners)
    {
    case Radius::Element:
        return Px(r.element);
    case Radius::Control:
        return Px(r.control);
    case Radius::Card:
        return Px(r.card);
    case Radius::Popover:
        return Px(r.popover);
    case Radius::Window:
        return Px(r.window);
    case Radius::None:
    default:
        return 0.0f;
    }
}

MediaImage MediaImageOf(ImageHandle image)
{
    MediaImage result;
    result.status = GetImageStatus(image);
    if (result.status == ImageStatus::Ready)
    {
        result.texture = ImageTexture(image);
        result.size = ImageSize(image);
        if (result.size.x <= 0.0f || result.size.y <= 0.0f)
            result.status = ImageStatus::Failed;
    }
    return result;
}

MediaImage MediaImageOf(ID3D11ShaderResourceView* texture)
{
    MediaImage result;
    result.status = ImageStatus::Failed;
    if (texture == nullptr)
        return result;
    Microsoft::WRL::ComPtr<ID3D11Resource> resource;
    texture->GetResource(&resource);
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2d;
    if (!resource || FAILED(resource.As(&texture2d)))
        return result;
    D3D11_TEXTURE2D_DESC desc = {};
    texture2d->GetDesc(&desc);
    if (desc.Width == 0 || desc.Height == 0)
        return result;
    result.status = ImageStatus::Ready;
    result.size = ImVec2(static_cast<float>(desc.Width), static_cast<float>(desc.Height));
    result.texture = ImTextureRef(static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(texture)));
    return result;
}

ImRect MediaFitRect(const ImRect& box, ImVec2 textureSize, ImageFit fit, ImVec2* uvMin, ImVec2* uvMax)
{
    const ImVec2 boxMin(std::round(box.Min.x), std::round(box.Min.y));
    const ImVec2 boxMax(std::round(box.Max.x), std::round(box.Max.y));
    const FitResult f = FitImage(boxMin, boxMax, textureSize, fit);
    if (uvMin != nullptr)
        *uvMin = f.uvMin;
    if (uvMax != nullptr)
        *uvMax = f.uvMax;
    if (fit != ImageFit::Contain)
        return ImRect(boxMin, boxMax);
    // Letterboxed: whole pixels, and exactly the texture size when it fits as it is.
    ImRect r(std::round(f.min.x), std::round(f.min.y), std::round(f.max.x), std::round(f.max.y));
    if (std::fabs(f.max.x - f.min.x - textureSize.x) < 0.5f && std::fabs(f.max.y - f.min.y - textureSize.y) < 0.5f)
        r.Max = ImVec2(r.Min.x + textureSize.x, r.Min.y + textureSize.y);
    return r;
}

ImRect MediaDrawTexture(ImDrawList* drawList, const MediaImage& image, const ImRect& box, const MediaDrawOptions& options)
{
    ImVec2 uvMin;
    ImVec2 uvMax;
    const ImRect r = MediaFitRect(box, image.size, options.fit, &uvMin, &uvMax);
    if (drawList == nullptr || image.status != ImageStatus::Ready || r.GetWidth() <= 0.0f || r.GetHeight() <= 0.0f)
        return r;
    const ImU32 tint = ScaleAlpha(options.tint, std::clamp(options.opacity, 0.0f, 1.0f));
    if ((tint & IM_COL32_A_MASK) == 0)
        return r;
    const float radius = std::clamp(options.radius, 0.0f, 0.5f * std::min(r.GetWidth(), r.GetHeight()));
    drawList->PushTexture(image.texture);
    // The corners must be geometry: the baked corner shapes live in the font atlas, which is not
    // the texture bound for this shape (draw.h).
    drawList->PushDrawFlag(ImDrawFlags_UseTexForRoundCorners, false);
    const int vertexStart = drawList->VtxBuffer.Size;
    if (options.circular)
        FillRoundRect(drawList, r.Min, r.Max, tint, radius);
    else
        FillRect(drawList, r.Min, r.Max, tint, radius);
    ImGui::ShadeVertsLinearUV(drawList, vertexStart, drawList->VtxBuffer.Size, r.Min, r.Max, uvMin, uvMax, true);
    drawList->PopDrawFlag();
    drawList->PopTexture();
    return r;
}

void MediaDrawImageSlot(ImDrawList* drawList, const MediaImage& image, const ImRect& box, const MediaDrawOptions& options,
                        const char* altText)
{
    if (drawList == nullptr || box.GetWidth() <= 0.0f || box.GetHeight() <= 0.0f)
        return;
    const ImVec2 min(std::round(box.Min.x), std::round(box.Min.y));
    const ImVec2 max(std::round(box.Max.x), std::round(box.Max.y));
    const float radius = std::clamp(options.radius, 0.0f, 0.5f * std::min(max.x - min.x, max.y - min.y));
    switch (image.status)
    {
    case ImageStatus::Ready:
        MediaDrawTexture(drawList, image, box, options);
        break;
    case ImageStatus::Loading:
        DrawSkeleton(drawList, min, max, radius, options.circular);
        break;
    default:
        DrawImageFailed(drawList, min, max, radius, altText != nullptr ? altText : "");
        break;
    }
}

MediaText MediaVisibleText(const char* label)
{
    MediaText out;
    if (label == nullptr)
        return out;
    const char* end = ImGui::FindRenderedTextEnd(label);
    std::size_t length = static_cast<std::size_t>(end - label);
    if (length >= sizeof(out.text))
    {
        // Cut before the lead byte of the character that does not fit whole.
        length = sizeof(out.text) - 1;
        while (length > 0 && (static_cast<unsigned char>(label[length]) & 0xC0) == 0x80)
            --length;
    }
    std::memcpy(out.text, label, length);
    out.text[length] = '\0';
    return out;
}

MediaInitials MediaInitialsOf(const char* text)
{
    MediaInitials out;
    if (text == nullptr)
        return out;
    const char* firstWord = nullptr;
    const char* lastWord = nullptr;
    for (const char* p = text; *p != '\0'; ++p)
    {
        const bool space = *p == ' ' || *p == '\t' || *p == '\n';
        if (!space && (p == text || p[-1] == ' ' || p[-1] == '\t' || p[-1] == '\n'))
        {
            if (firstWord == nullptr)
                firstWord = p;
            lastWord = p;
        }
    }
    std::size_t at = 0;
    const auto append = [&](const char* word) {
        unsigned int codepoint = 0;
        const int bytes = ImTextCharFromUtf8(&codepoint, word, nullptr);
        if (bytes <= 0 || at + static_cast<std::size_t>(bytes) >= sizeof(out.text))
            return;
        for (int i = 0; i < bytes; ++i)
        {
            char ch = word[i];
            if (bytes == 1 && ch >= 'a' && ch <= 'z')
                ch = static_cast<char>(ch - 'a' + 'A');
            out.text[at++] = ch;
        }
        out.text[at] = '\0';
    };
    if (firstWord != nullptr)
        append(firstWord);
    if (lastWord != nullptr && lastWord != firstWord)
        append(lastWord);
    return out;
}

float MediaAvatarDiameter(float units, ControlSize size)
{
    const float diameter = units > 0.0f ? Px(units) : ControlHeight(size);
    return std::max(2.0f, std::round(diameter * 0.5f) * 2.0f);
}

float MediaAvatarRadius(float side, bool squircle)
{
    return squircle ? std::round(side * kMediaSquircleShare) : side * 0.5f;
}

void MediaDrawAvatar(ImDrawList* drawList, const ImRect& rect, const char* name, ImageHandle image,
                     const MediaAvatarLook& look)
{
    if (drawList == nullptr || rect.GetWidth() <= 0.0f)
        return;
    const Theme& t = T();
    const Colors& c = t.colors;
    ImRect box(ImVec2(std::round(rect.Min.x), std::round(rect.Min.y)), ImVec2(std::round(rect.Max.x), std::round(rect.Max.y)));
    const float side = box.GetWidth();
    if (look.selected)
    {
        const float ring = std::max(1.0f, std::round(HairlineThickness() * kMediaSelectedRingHairlines));
        const ImVec4 color = look.disabled ? c.labelTertiary : c.accent;
        if (look.squircle)
            StrokeRect(drawList, box.Min, box.Max, ToU32(color), MediaAvatarRadius(side, true), ring);
        else
            StrokeRoundRect(drawList, box.Min, box.Max, ToU32(color), side * 0.5f, ring);
        box.Expand(-(ring + HairlineThickness()));
    }
    const float inner = box.GetWidth();
    if (inner <= 0.0f)
        return;
    const float radius = MediaAvatarRadius(inner, look.squircle);
    const MediaImage picture = MediaImageOf(image);
    if (picture.status == ImageStatus::Ready)
    {
        MediaDrawOptions options;
        options.fit = ImageFit::Cover;
        options.radius = radius;
        options.circular = !look.squircle;
        MediaDrawTexture(drawList, picture, box, options);
        return;
    }
    if (picture.status == ImageStatus::Loading)
    {
        DrawSkeleton(drawList, box.Min, box.Max, radius, !look.squircle);
        return;
    }
    const ImU32 fill = ToU32(look.disabled ? c.fill[3] : c.fill[1]);
    if (look.squircle)
        FillRect(drawList, box.Min, box.Max, fill, radius);
    else
        FillRoundRect(drawList, box.Min, box.Max, fill, radius);
    const ImU32 ink = ToU32(ControlLabelColor(look.disabled));
    const MediaInitials initials = MediaInitialsOf(name);
    if (initials.text[0] == '\0')
    {
        const float icon = std::max(1.0f, std::round(inner * kMediaAvatarIconShare));
        DrawIcon(drawList, Icon("user"), ImVec2(std::round(box.GetCenter().x - icon * 0.5f), std::round(box.GetCenter().y - icon * 0.5f)),
                 icon, ink);
        return;
    }
    TextStyle style = TextStyle::Caption;
    for (const TextStyle candidate : kMediaInitialStyles)
    {
        if (Px(t.type[candidate].size) <= inner * kMediaInitialsShare)
        {
            style = candidate;
            break;
        }
    }
    ControlTextScope text(style);
    const ImVec2 size = ImGui::CalcTextSize(initials.text);
    const ImVec2 pos(std::round(box.GetCenter().x - size.x * 0.5f), std::round(box.GetCenter().y - size.y * 0.5f));
    drawList->PushClipRect(box.Min, box.Max, true);
    drawList->AddText(pos, ink, initials.text);
    drawList->PopClipRect();
}

void MediaDrawMessage(ImDrawList* drawList, const ImRect& rect, Icon icon, const char* text, bool disabled)
{
    if (drawList == nullptr || rect.GetWidth() <= 0.0f || rect.GetHeight() <= 0.0f)
        return;
    const Theme& t = T();
    ControlTextScope scope(TextStyle::Footnote);
    const float pad = PxSnapped(t.spacing.sm);
    const FittedText shown = FitText(text != nullptr ? text : "", nullptr, std::max(0.0f, rect.GetWidth() - pad * 2.0f));
    const float lineHeight = ImGui::GetTextLineHeight();
    const float iconSize = std::floor(Px(t.spacing.xl));
    const float gap = PxSnapped(t.spacing.xs);
    const bool showIcon = icon.IsValid() && rect.GetHeight() >= iconSize + gap + lineHeight + pad * 2.0f;
    const float block = (showIcon ? iconSize + gap : 0.0f) + lineHeight;
    float y = std::round(rect.GetCenter().y - block * 0.5f);
    const ImU32 color = ToU32(ControlSecondaryColor(disabled));
    drawList->PushClipRect(rect.Min, rect.Max, true);
    if (showIcon)
    {
        DrawIcon(drawList, icon, ImVec2(std::round(rect.GetCenter().x - iconSize * 0.5f), y), iconSize, color);
        y += iconSize + gap;
    }
    drawList->AddText(ImVec2(std::round(rect.GetCenter().x - shown.width * 0.5f), y), color, shown.text);
    drawList->PopClipRect();
}

void MediaDrawCaption(ImDrawList* drawList, const ImRect& area, MediaCorner corner, const char* text, bool disabled)
{
    if (drawList == nullptr || text == nullptr || text[0] == '\0')
        return;
    const Theme& t = T();
    const Colors& c = t.colors;
    ControlTextScope scope(TextStyle::Footnote);
    const float inset = PxSnapped(t.spacing.sm);
    const float padX = PxSnapped(t.spacing.sm);
    const float padY = PxSnapped(t.spacing.xs);
    const float room = area.GetWidth() - inset * 2.0f - padX * 2.0f;
    const float lineHeight = std::ceil(ImGui::GetTextLineHeight());
    if (room <= 0.0f || area.GetHeight() < lineHeight + padY * 2.0f + inset * 2.0f)
        return;
    const FittedText shown = FitText(text, nullptr, room);
    const ImVec2 size(std::ceil(shown.width) + padX * 2.0f, lineHeight + padY * 2.0f);
    const bool right = corner == MediaCorner::TopRight || corner == MediaCorner::BottomRight;
    const bool bottom = corner == MediaCorner::BottomLeft || corner == MediaCorner::BottomRight;
    const ImVec2 min(std::round(right ? area.Max.x - inset - size.x : area.Min.x + inset),
                     std::round(bottom ? area.Max.y - inset - size.y : area.Min.y + inset));
    const ImVec2 max(min.x + size.x, min.y + size.y);
    const float radius = Px(t.radii.element);
    FillRect(drawList, min, max, ToU32(c.bgElevated), radius);
    StrokeRect(drawList, min, max, ToU32(c.separator), radius, HairlineThickness());
    drawList->AddText(ImVec2(min.x + padX, min.y + padY), ToU32(ControlLabelColor(disabled)), shown.text);
}

void MediaSpringStep(MediaSpringValue& value, float target, const SpringSpec& spec, float dt)
{
    const Motion& motion = T().motion;
    const float response = spec.response * motion.scale;
    if (!value.started || ReduceMotion() || response <= 0.0f || !std::isfinite(value.position))
    {
        value = {target, 0.0f, true};
        return;
    }
    dt = std::max(0.0f, dt);
    // Displacement from the target as a damped oscillator, solved exactly for dt.
    const double x0 = static_cast<double>(value.position) - target;
    const double v0 = value.velocity;
    const double omega = 2.0 * 3.14159265358979323846 / response;
    const double zeta = std::max(0.0f, spec.dampingRatio);
    const double t = dt;
    double x = 0.0;
    double v = 0.0;
    if (zeta < 1.0)
    {
        const double wd = omega * std::sqrt(1.0 - zeta * zeta);
        const double a = x0;
        const double b = (v0 + zeta * omega * x0) / wd;
        const double decay = std::exp(-zeta * omega * t);
        const double cs = std::cos(wd * t);
        const double sn = std::sin(wd * t);
        x = decay * (a * cs + b * sn);
        v = decay * ((b * wd - zeta * omega * a) * cs - (a * wd + zeta * omega * b) * sn);
    }
    else
    {
        const double a = x0;
        const double b = v0 + omega * x0;
        const double decay = std::exp(-omega * t);
        x = (a + b * t) * decay;
        v = (b - omega * (a + b * t)) * decay;
    }
    // Snap once the rest is below a hundredth of a pixel and the motion has died down.
    if (std::fabs(x) < 0.01 && std::fabs(v) < 0.1)
    {
        x = 0.0;
        v = 0.0;
    }
    value.position = static_cast<float>(target + x);
    value.velocity = static_cast<float>(v);
}

void MediaSpringHold(MediaSpringValue& value, float position, float dt)
{
    if (value.started && dt > 0.0f)
    {
        const float measured = (position - value.position) / dt;
        const float keep = std::exp(-dt * kMediaVelocitySmoothing);
        value.velocity = value.velocity * keep + measured * (1.0f - keep);
    }
    value.position = position;
    value.started = true;
}

bool MediaTakeKey(ImGuiID id, ImGuiKey key)
{
    ImGui::SetKeyOwner(key, id);
    return ImGui::IsKeyPressed(key, ImGuiInputFlags_Repeat, id);
}

ImVec2 MediaClaimWheel(ImGuiID id, const ImRect& rect, bool vertical)
{
    ImGuiContext& g = *GImGui;
    const bool over = g.HoveredWindow == g.CurrentWindow && ImGui::IsMouseHoveringRect(rect.Min, rect.Max);
    if (!over || ScrollWheelBelongsToScroll())
        return ImVec2(0.0f, 0.0f);
    const bool takeY = vertical || g.IO.KeyShift;
    const bool owned = ImGui::TestKeyOwner(ImGuiKey_MouseWheelX, id) && (!takeY || ImGui::TestKeyOwner(ImGuiKey_MouseWheelY, id));
    ImGui::SetKeyOwner(ImGuiKey_MouseWheelX, id);
    if (takeY)
        ImGui::SetKeyOwner(ImGuiKey_MouseWheelY, id);
    if (!owned)
        return ImVec2(0.0f, 0.0f);
    ImVec2 wheel(g.IO.MouseWheelH, takeY ? g.IO.MouseWheel : 0.0f);
    if (g.IO.KeyShift && wheel.x == 0.0f)
        wheel = ImVec2(wheel.y, 0.0f);
    return wheel;
}

} // namespace facet::internal
