// Fonts and text styles: loads one font per weight (and display faces) into the Dear ImGui
// font atlas and maps every TextStyle to a font and a size token.

#include "facet/core/text.h"

#include <windows.h>

#include <cfloat>
#include <climits>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include <imgui_internal.h>

#include "core/core_internal.h"
#include "internal/subsystem.h"

namespace facet
{
namespace
{

// Named instances of Segoe UI Variable in the font's instance order: the Small, Text and
// Display optical sizes, each in Light, Semilight, Regular, Semibold and Bold.
constexpr int kInstanceTextRegular = 7;
constexpr int kInstanceTextSemibold = 8;
constexpr int kInstanceTextBold = 9;
constexpr int kInstanceDisplaySemibold = 13;
constexpr int kInstanceDisplayBold = 14;

// FreeType reads the named instance from the upper 16 bits of the face index, counting from 1.
constexpr int InstanceFaceIndex(int instance)
{
    return (instance + 1) << 16;
}

// Text measured to check that the instances of the variable font really differ.
constexpr const char* kProbeText = "Hamburgefonstiv 0123456789";
constexpr float kProbeSize = 20.0f;
// The bold face must measure at least this much wider than the regular one.
constexpr float kMinBoldWidening = 1.02f;

enum Slot
{
    kRegular,
    kSemibold,
    kBold,
    kDisplayRegular,
    kDisplaySemibold,
    kDisplayBold,
    kSlotCount,
};

struct FontState
{
    // The custom set passed to SetFontSet, with its paths copied.
    FontSet custom;
    std::string customPaths[kSlotCount];
    bool hasCustom = false;
    bool reloadPending = false;

    ImFont* fonts[kSlotCount] = {};
    std::vector<std::unique_ptr<std::vector<char>>> fileData;  // font files read by Facet
    FontOrigin origin = FontOrigin::None;
    ImFont* previousDefault = nullptr;
    bool setDefault = false;
    bool initialized = false;
};

FontState& State()
{
    static FontState state;
    return state;
}

FontSource& SlotSource(FontSet& set, int slot)
{
    FontSource* sources[kSlotCount] = {&set.regular,        &set.semibold,        &set.bold,
                                       &set.displayRegular, &set.displaySemibold, &set.displayBold};
    return *sources[slot];
}

std::wstring Widen(const char* utf8)
{
    const int length = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
    if (length <= 1)
        return {};
    std::wstring out(static_cast<size_t>(length - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out.data(), length);
    return out;
}

std::wstring SystemFontPath(const wchar_t* file)
{
    wchar_t windows[MAX_PATH] = {};
    const UINT length = GetWindowsDirectoryW(windows, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
        return {};
    return std::wstring(windows) + L"\\Fonts\\" + file;
}

// Reads a whole file, or returns nullptr. Kept alive by FontState until the fonts are removed,
// because the rasterizer reads glyphs from it on demand.
const std::vector<char>* ReadWholeFile(const std::wstring& path)
{
    if (path.empty())
        return nullptr;
    FILE* file = nullptr;
    if (_wfopen_s(&file, path.c_str(), L"rb") != 0 || file == nullptr)
        return nullptr;
    auto data = std::make_unique<std::vector<char>>();
    char buffer[64 * 1024];
    size_t read = 0;
    while ((read = std::fread(buffer, 1, sizeof(buffer), file)) > 0)
        data->insert(data->end(), buffer, buffer + read);
    std::fclose(file);
    if (data->empty())
        return nullptr;
    State().fileData.push_back(std::move(data));
    return State().fileData.back().get();
}

ImFont* AddFont(ImFontAtlas* atlas, const void* data, size_t size, int faceIndex, const char* name)
{
    if (data == nullptr || size == 0 || size > static_cast<size_t>(INT_MAX))
        return nullptr;
    ImFontConfig config;
    config.FontDataOwnedByAtlas = false;
    config.FontNo = faceIndex;
    std::snprintf(config.Name, sizeof(config.Name), "%s", name);
    return atlas->AddFontFromMemoryTTF(const_cast<void*>(data), static_cast<int>(size), 0.0f, &config);
}

ImFont* AddSource(ImFontAtlas* atlas, const FontSource& source, const char* name)
{
    if (source.data != nullptr)
        return AddFont(atlas, source.data, source.size, source.faceIndex, name);
    if (source.path == nullptr)
        return nullptr;
    const std::vector<char>* data = ReadWholeFile(Widen(source.path));
    return data != nullptr ? AddFont(atlas, data->data(), data->size(), source.faceIndex, name) : nullptr;
}

// Removes Facet's fonts from the atlas and frees the file buffers they read from. A locked
// atlas cannot remove fonts; then nothing changes and false is returned, because freeing the
// buffers would leave the fonts pointing into released memory.
bool RemoveFonts(ImFontAtlas* atlas)
{
    FontState& state = State();
    if (atlas != nullptr && atlas->Locked)
    {
        for (ImFont* font : state.fonts)
        {
            if (font != nullptr && atlas->Fonts.contains(font))
                return false;
        }
    }
    for (ImFont*& font : state.fonts)
    {
        if (font != nullptr && atlas != nullptr && atlas->Fonts.contains(font))
            atlas->RemoveFont(font);
        font = nullptr;
    }
    state.fileData.clear();
    state.origin = FontOrigin::None;
    return true;
}

bool AtlasLocked()
{
    if (ImGui::GetCurrentContext() == nullptr)
        return false;
    const ImFontAtlas* atlas = ImGui::GetIO().Fonts;
    return atlas != nullptr && atlas->Locked;
}

bool HasRequiredFaces()
{
    const FontState& state = State();
    return state.fonts[kRegular] != nullptr && state.fonts[kSemibold] != nullptr && state.fonts[kBold] != nullptr;
}

bool LoadCustom(ImFontAtlas* atlas)
{
    FontState& state = State();
    static const char* const kNames[kSlotCount] = {"Regular",         "Semibold",         "Bold",
                                                   "Display Regular", "Display Semibold", "Display Bold"};
    for (int slot = 0; slot < kSlotCount; ++slot)
    {
        const FontSource& source = SlotSource(state.custom, slot);
        if (source.IsSet())
            state.fonts[slot] = AddSource(atlas, source, kNames[slot]);
    }
    if (HasRequiredFaces())
        return true;
    internal::LogWarning("the custom font set could not be loaded (regular, semibold and bold are required); "
                         "using the system fonts");
    RemoveFonts(atlas);
    return false;
}

float MeasureProbe(ImFont* font)
{
    return font->CalcTextSizeA(kProbeSize, FLT_MAX, 0.0f, kProbeText).x;
}

bool LoadVariable(ImFontAtlas* atlas)
{
#ifdef IMGUI_ENABLE_FREETYPE
    FontState& state = State();
    const std::vector<char>* data = ReadWholeFile(SystemFontPath(L"SegUIVar.ttf"));
    if (data == nullptr)
        return false;
    const struct
    {
        int slot;
        int instance;
        const char* name;
    } faces[] = {
        {kRegular, kInstanceTextRegular, "Segoe UI Variable Text"},
        {kSemibold, kInstanceTextSemibold, "Segoe UI Variable Text Semibold"},
        {kBold, kInstanceTextBold, "Segoe UI Variable Text Bold"},
        {kDisplaySemibold, kInstanceDisplaySemibold, "Segoe UI Variable Display Semibold"},
        {kDisplayBold, kInstanceDisplayBold, "Segoe UI Variable Display Bold"},
    };
    for (const auto& face : faces)
        state.fonts[face.slot] = AddFont(atlas, data->data(), data->size(), InstanceFaceIndex(face.instance), face.name);

    if (HasRequiredFaces())
    {
        // A rasterizer that ignores the instance bits draws every face with the default
        // instance; then regular and bold measure the same.
        const float regular = MeasureProbe(state.fonts[kRegular]);
        const float bold = MeasureProbe(state.fonts[kBold]);
        if (regular > 0.0f && bold >= regular * kMinBoldWidening)
            return true;
        internal::LogWarning("Segoe UI Variable instances are not applied by the rasterizer; using static Segoe UI");
    }
    RemoveFonts(atlas);
    return false;
#else
    (void)atlas;
    return false;
#endif
}

bool LoadStatic(ImFontAtlas* atlas)
{
    FontState& state = State();
    const struct
    {
        int slot;
        const wchar_t* file;
        const char* name;
    } faces[] = {
        {kRegular, L"segoeui.ttf", "Segoe UI"},
        {kSemibold, L"seguisb.ttf", "Segoe UI Semibold"},
        {kBold, L"segoeuib.ttf", "Segoe UI Bold"},
    };
    for (const auto& face : faces)
    {
        const std::vector<char>* data = ReadWholeFile(SystemFontPath(face.file));
        if (data != nullptr)
            state.fonts[face.slot] = AddFont(atlas, data->data(), data->size(), 0, face.name);
    }
    if (HasRequiredFaces())
        return true;
    RemoveFonts(atlas);
    return false;
}

void LoadFonts()
{
    FontState& state = State();
    if (ImGui::GetCurrentContext() == nullptr)
        return;
    ImGuiIO& io = ImGui::GetIO();
    ImFontAtlas* atlas = io.Fonts;
    if (atlas == nullptr || atlas->Locked)
    {
        internal::LogWarning("the font atlas is locked; Facet text uses the default font");
        return;
    }

    if (state.hasCustom && LoadCustom(atlas))
        state.origin = FontOrigin::Custom;
    else if (LoadVariable(atlas))
        state.origin = FontOrigin::Variable;
    else if (LoadStatic(atlas))
        state.origin = FontOrigin::Static;
    else
        internal::LogWarning("no Segoe UI font was found; Facet text uses the default font");

    if (state.origin != FontOrigin::None && (io.FontDefault == nullptr || state.setDefault))
    {
        if (!state.setDefault)
            state.previousDefault = io.FontDefault;
        io.FontDefault = state.fonts[kRegular];
        state.setDefault = true;
    }
}

bool HasLoadedFonts()
{
    for (const ImFont* font : State().fonts)
    {
        if (font != nullptr)
            return true;
    }
    return false;
}

// Removes Facet's fonts and restores the app's default font. Returns false, with everything
// left in place, while the atlas is locked.
bool UnloadFonts()
{
    FontState& state = State();
    if (AtlasLocked() && HasLoadedFonts())
        return false;
    ImFontAtlas* atlas = nullptr;
    if (ImGui::GetCurrentContext() != nullptr)
    {
        ImGuiIO& io = ImGui::GetIO();
        atlas = io.Fonts;
        if (state.setDefault)
            io.FontDefault = state.previousDefault;
    }
    state.setDefault = false;
    state.previousDefault = nullptr;
    return RemoveFonts(atlas);
}

// Replaces the loaded fonts with the current choice. A locked atlas (between ImGui::NewFrame
// and ImGui::EndFrame with a renderer that does not update textures itself) keeps the old
// fonts and leaves the reload pending.
void Reload()
{
    FontState& state = State();
    if (AtlasLocked())
    {
        if (!state.reloadPending)
            internal::LogWarning("the font atlas is locked; the new font set loads at the next unlocked frame "
                                 "(call SetFontSet before ImGui::NewFrame)");
        state.reloadPending = true;
        return;
    }
    state.reloadPending = false;
    UnloadFonts();
    LoadFonts();
}

bool FontInit()
{
    FontState& state = State();
    state.initialized = true;
    state.reloadPending = false;
    // Fonts left over from a shutdown during a locked frame are replaced now.
    UnloadFonts();
    LoadFonts();
    return true;  // missing fonts fall back to the default font
}

void FontShutdown()
{
    FontState& state = State();
    if (!UnloadFonts())
        internal::LogWarning("the font atlas is locked at shutdown; Facet's fonts stay in it");
    state.initialized = false;
    state.reloadPending = false;
}

void FontNewFrame()
{
    if (State().reloadPending)
        Reload();
}

// The display faces are meant for the two largest styles.
bool UsesDisplayFace(TextStyle style)
{
    return style == TextStyle::LargeTitle || style == TextStyle::Title1;
}

} // namespace

void SetFontSet(const FontSet& fonts)
{
    FontState& state = State();
    state.custom = fonts;
    state.hasCustom = false;
    for (int slot = 0; slot < kSlotCount; ++slot)
    {
        FontSource& source = SlotSource(state.custom, slot);
        state.customPaths[slot] = source.path != nullptr ? source.path : "";
        if (source.path != nullptr)
            source.path = state.customPaths[slot].c_str();
        state.hasCustom = state.hasCustom || source.IsSet();
    }
    // Between frames the fonts change at once. Inside a frame a font of the old set may still
    // be pushed, so the reload waits for the next facet::NewFrame (see Reload).
    if (!state.initialized)
        return;
    const ImGuiContext* context = ImGui::GetCurrentContext();
    if (context != nullptr && !context->WithinFrameScope)
        Reload();
    else
        state.reloadPending = true;
}

ImFont* WeightFont(FontWeight weight, bool display)
{
    const FontState& state = State();
    const int text = static_cast<int>(weight);
    if (display && state.fonts[kDisplayRegular + text] != nullptr)
        return state.fonts[kDisplayRegular + text];
    return state.fonts[text];
}

FontOrigin ActiveFontOrigin()
{
    return State().origin;
}

const char* FontOriginName(FontOrigin origin)
{
    switch (origin)
    {
    case FontOrigin::None:
        return "default font";
    case FontOrigin::Custom:
        return "custom font set";
    case FontOrigin::Variable:
        return "Segoe UI Variable";
    case FontOrigin::Static:
        return "Segoe UI";
    }
    return "unknown";
}

void PushTextStyle(TextStyle style)
{
    const TextStyleSpec& spec = T().type[style];
    ImGui::PushFont(WeightFont(spec.weight, UsesDisplayFace(style)), spec.size);
}

void PopTextStyle()
{
    ImGui::PopFont();
}

void Text(TextStyle style, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    TextV(style, fmt, args);
    va_end(args);
}

void TextV(TextStyle style, const char* fmt, va_list args)
{
    PushTextStyle(style);
    ImGui::TextV(fmt, args);
    PopTextStyle();
}

ImVec2 MeasureText(TextStyle style, const char* text, const char* textEnd, float wrapWidth)
{
    PushTextStyle(style);
    const ImVec2 size = ImGui::CalcTextSize(text, textEnd, false, wrapWidth > 0.0f ? wrapWidth : -1.0f);
    PopTextStyle();
    return size;
}

float CenteredTextY(float top, float height)
{
    const float size = ImGui::GetFontSize();
    float inkTop = 0.0f;
    float inkBottom = size;
    ImFontBaked* baked = ImGui::GetFontBaked();
    if (baked != nullptr && baked->Size > 0.0f)
    {
        // Glyph extents are relative to the top of the line, in units of the baked size.
        const ImFontGlyph* glyph = baked->FindGlyphNoFallback('H');
        if (glyph != nullptr && glyph->Y1 > glyph->Y0)
        {
            const float toPixels = size / baked->Size;
            inkTop = glyph->Y0 * toPixels;
            inkBottom = glyph->Y1 * toPixels;
        }
    }
    return std::round(top + height * 0.5f - (inkTop + inkBottom) * 0.5f);
}

namespace internal
{

// Loads the fonts at Init and removes them at Shutdown; a SetFontSet reloads them between
// frames or at the start of the next frame.
Subsystem FontSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "font";
    subsystem.init = &FontInit;
    subsystem.shutdown = &FontShutdown;
    subsystem.newFrame = &FontNewFrame;
    return subsystem;
}

} // namespace internal

} // namespace facet
