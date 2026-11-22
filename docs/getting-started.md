# Getting started

This guide goes from an empty CMake project to a window with Facet widgets in it. Every code
block is taken from code that is compiled in this repository: the consumer example in
`examples/consumer/` (`main.cpp` and `CMakeLists.txt`), the demo in `demo/`, or the tests.
`examples/consumer/main.cpp` is the complete program; the blocks below are parts of it.

Requirements: Windows 10 or 11, a C++20 compiler (the presets use Visual Studio 2026 with
MSVC v145), CMake 3.28 or newer, and Dear ImGui 1.93.0 or newer. Facet renders with Direct3D 11
only.

## 1. Add Facet to the project

Facet is added from source with `add_subdirectory` (a copy of the repository in the project, a
git submodule, or a `FetchContent` download of it). It links as `facet::facet`. When Facet is
not the top-level project, its demo and tests are off by default, so only the library builds.
The library target sets C++20 for its users, because the public headers need it, and passes on
no compiler flags or definitions of its own.

There are two ways to get Dear ImGui.

### Facet fetches Dear ImGui

With nothing set, Facet downloads Dear ImGui v1.93.0 (and FreeType for its font loader, while
`FACET_USE_FREETYPE` is on) and builds it as a static library. The Win32 and DX11 backends are
then a separate target, `facet::imgui_backends`, which the application links if it uses them;
the library itself never compiles a backend. This is how `demo/CMakeLists.txt` links the demo:

```cmake
target_link_libraries(facet_demo PRIVATE
    facet::facet
    facet::imgui_backends
    ...)
```

### The project brings its own Dear ImGui

A project that already has a Dear ImGui target, with its own `imconfig.h`, names it in
`FACET_IMGUI_TARGET` before adding Facet. Facet then fetches neither Dear ImGui nor FreeType,
compiles its sources against that target (the include paths and definitions the target
publishes, `IMGUI_USER_CONFIG` among them), and links it publicly. `facet::imgui_backends` does not exist in this
mode: the backends come from the project's own target. From `examples/consumer/CMakeLists.txt`:

```cmake
add_library(consumer_imgui STATIC
    "${imgui_dir}/imgui.cpp"
    "${imgui_dir}/imgui_draw.cpp"
    "${imgui_dir}/imgui_tables.cpp"
    "${imgui_dir}/imgui_widgets.cpp"
    "${imgui_dir}/backends/imgui_impl_dx11.cpp"
    "${imgui_dir}/backends/imgui_impl_win32.cpp")
target_include_directories(consumer_imgui PUBLIC
    "${imgui_dir}"
    "${imgui_dir}/backends"
    "${CMAKE_CURRENT_SOURCE_DIR}/config")
target_compile_definitions(consumer_imgui PUBLIC IMGUI_USER_CONFIG="consumer_imconfig.h")
target_link_libraries(consumer_imgui PUBLIC d3d11 d3dcompiler dxgi dwmapi)

# A normal variable: Facet's cache entry of the same name does not replace it.
set(FACET_IMGUI_TARGET consumer_imgui)
add_subdirectory("${FACET_SOURCE_DIR}" facet)

add_executable(facet_consumer main.cpp)
target_link_libraries(facet_consumer PRIVATE facet::facet consumer_imgui)
```

The target has to exist before `add_subdirectory`, or the configure stops with an error. Its
configuration must be the one the application uses, because Facet's own sources are compiled
with it (the example's `consumer_imconfig.h` switches to 32-bit draw indices to prove this). Facet
selects the Segoe UI Variable font only when the Dear ImGui target rasterizes with FreeType
(`IMGUI_ENABLE_FREETYPE` and `misc/freetype/imgui_freetype.cpp`); without it, Facet uses the
static Segoe UI files. The library checks `IMGUI_VERSION_NUM >= 19300` at compile time.

## 2. Start up

The order matters in three places:

- a Dear ImGui context must exist before `facet::Init`;
- `facet::Init` gets the device and the immediate context that render the Dear ImGui draw data;
- `facet::Init` runs before the first `ImGui::NewFrame`, because it loads the fonts into the font
  atlas and because Dear ImGui reads the ini file, which also holds Facet's saved widget state,
  in that first frame.

The process DPI awareness is the application's choice; Facet never changes it. From
`RunWindow` in `examples/consumer/main.cpp`:

```cpp
// The application decides the DPI awareness; Facet never changes it.
SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
...
window.dpiScale = static_cast<float>(GetDpiForWindow(window.hwnd)) / 96.0f;
ShowWindow(window.hwnd, SW_SHOWDEFAULT);

IMGUI_CHECKVERSION();
ImGui::CreateContext();
ConfigureIo();
ConfigureFacetLook(window.dpiScale);
ImGui_ImplWin32_Init(window.hwnd);
ImGui_ImplDX11_Init(window.device.Get(), window.context.Get());
facet::InitDesc init;
init.device = window.device.Get();
init.context = window.context.Get();
if (!facet::Init(init))
    std::fprintf(stderr, "error: facet::Init failed\n");
```

with these two helpers:

```cpp
// The Dear ImGui settings this example uses: no ini file and keyboard navigation on.
void ConfigureIo()
{
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
}

// The look: Facet keeps the stock Dear ImGui style in step with its theme and scale.
void ConfigureFacetLook(float dpiScale)
{
    facet::SetScaleMode(facet::ScaleMode::FollowDpi);
    facet::SetDpiScale(dpiScale);
    facet::SyncImGuiStyle(true);
    facet::ApplyImGuiStyle();
}
```

`ImGuiConfigFlags_NavEnableKeyboard` makes Tab move between widgets; Facet never sets io flags
itself. `facet::Init` returns false and leaves Facet uninitialized when the context or the device
is missing, when a module fails to start, or when Facet is already initialized.

## 3. Each frame

`facet::NewFrame` comes right after `ImGui::NewFrame`, and `facet::EndFrame` right before
`ImGui::Render`. Facet reads its frame time from `io.DeltaTime`. The draw data is rendered on
the context given to `facet::Init`, before the next `facet::NewFrame`. From the frame loop of
`RunWindow`:

```cpp
if (facet::DpiScale() != window.dpiScale)
    facet::SetDpiScale(window.dpiScale);

ImGui_ImplDX11_NewFrame();
ImGui_ImplWin32_NewFrame();
ImGui::NewFrame();
facet::NewFrame();
DrawContent(state, assets);
facet::EndFrame();
ImGui::Render();

const ImVec4& bg = facet::T().colors.bg;
const float clear[4] = {bg.x, bg.y, bg.z, 1.0f};
ID3D11RenderTargetView* views[] = {window.backBuffer.Get()};
window.context->OMSetRenderTargets(1, views, nullptr);
window.context->ClearRenderTargetView(window.backBuffer.Get(), clear);
ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
const HRESULT presented = window.swapChain->Present(1, 0);
```

`facet::EndFrame` also draws the toast stack and reports a `PushAccent`, `PushControlSize` or
Facet Begin left open through Dear ImGui's recoverable error path.

## 4. A themed widget

Widgets are free functions in `namespace facet` with Dear ImGui's label and id rules. Options are
aggregates passed with designated initializers; lengths in them are design units. The content
of the example, from `DrawContent`:

```cpp
if (!facet::BeginAppFrame("consumer"))
    return;

facet::Text(facet::TextStyle::Title2, "Facet consumer example");
facet::IconGlyph(assets.glyph);
ImGui::SameLine();
ImGui::TextUnformatted("An icon registered from SVG text");

if (facet::Toggle("Dark theme", &state.dark))
    facet::SetThemeMode(state.dark ? facet::ThemeMode::Dark : facet::ThemeMode::Light);

if (facet::Slider("UI scale", &state.uiScale, 0.75f, 2.0f, {.format = "%.2f", .step = 0.25, .width = 320.0f}))
    facet::SetUiScale(state.uiScale);

if (facet::Button("Count a press", {.variant = facet::ButtonVariant::Primary}))
{
    ++state.presses;
    facet::ShowToast("Button pressed", {.tone = facet::Tone::Success});
}
...
facet::EndAppFrame();
```

`BeginAppFrame` fills the main viewport with an undecorated window; `BeginShell` is the floating
menu window for overlays. Calls are always qualified (`facet::Button`, `ImGui::Button`), and
stock Dear ImGui items mix freely with Facet ones. `docs/api-conventions.md` has the rules every
widget follows, and `docs/widgets.md` lists the families.

The theme: `SetThemeMode` switches between the light and the dark preset (`System` follows the
Windows app mode), `SetAccent` sets the accent color, and `SetTheme` takes a whole
`facet::Theme`. `docs/theming.md` describes the tokens.

## 5. Scale modes

Every Facet length is a design unit times `facet::Scale()`. The mode decides where the scale
comes from (`include/facet/core/scale.h`, `docs/scaling.md`):

| Mode | `Scale()` | For |
|---|---|---|
| `ScaleMode::FollowDpi` (default) | `DpiScale() * UiScale()` | Desktop apps; the host reports the monitor DPI with `SetDpiScale(dpi / 96.0f)` |
| `ScaleMode::Manual` | `UiScale()` | Overlays inside another application, where the host decides alone |
| `ScaleMode::FitDisplay` | `min(width / 1920, height / 1080) * UiScale()` of the Dear ImGui display size | Menus that grow and shrink with the host's render resolution |

The example follows the window's DPI: `WM_DPICHANGED` stores the new value, and the frame loop
passes it on before the next frame.

```cpp
case WM_DPICHANGED:
    if (g_window != nullptr)
    {
        g_window->dpiScale = static_cast<float>(HIWORD(wParam)) / 96.0f;
        const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
        SetWindowPos(hwnd, nullptr, suggested->left, suggested->top, suggested->right - suggested->left,
                     suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
    }
    return 0;
```

An overlay picks the manual mode and sets the scale itself, as `tests/test_scale.cpp` does:

```cpp
facet::SetScaleMode(facet::ScaleMode::Manual);
facet::SetUiScale(1.5f);
```

While `SetManageFontScale` is on (the default), Facet writes `style.FontScaleDpi` and
`style.FontScaleMain` so that text grows with the same factor. A host that manages the font scale
itself turns it off with `facet::SetManageFontScale(false)`.

## 6. Icons and images

Icons are single-color SVG glyphs, tinted when drawn. The built-in set is the files in
`assets/icons/`, used by name (`facet::Icon("chevron-down")`); an application registers its own
from SVG text. Images are handles to textures Facet owns: decoded from a file or memory, created
from RGBA pixels, or rasterized from an SVG at the current scale. Loading needs Facet to be
initialized. From `LoadAssets` in the example:

```cpp
assets.glyph = facet::RegisterIcon("consumer-glyph", kGlyphSvg);
assets.svg = facet::LoadSvgMemory(kGlyphSvg, 0, ImVec2(24.0f, 24.0f));
const std::filesystem::path path = PatternPath();
if (WritePatternBmp(path))
    assets.file = facet::LoadImageFileAsync(reinterpret_cast<const char*>(path.u8string().c_str()));
const std::vector<unsigned char> pixels = PatternPixels();
assets.pixels = facet::CreateImageRGBA(pixels.data(), kPatternSide, kPatternSide);
```

and drawing them, sized in design units:

```cpp
facet::ImageView("SVG image", assets.svg, {.width = 48.0f, .height = 48.0f});
ImGui::SameLine();
facet::ImageView("Image file", assets.file, {.corners = facet::Radius::Element, .width = 48.0f, .height = 48.0f});
```

`LoadImageFileAsync` returns at once and decodes on a worker thread; `ImageView` shows a skeleton
until the image is ready and a failed state when it cannot be read. `LoadImageFile` is the
synchronous form. Paths are UTF-8. `docs/images-and-icons.md` has the details: limits, release,
the formats, and how a new icon is drawn.

## 7. Device loss

Facet holds Direct3D objects of its own (atlases, shaders, textures). Two cases:

**The same device, objects recreated** (for example after the host released shared resources).
Between frames, never between recording a frame and rendering it. From `RunOffscreen`:

```cpp
facet::InvalidateDeviceObjects();
ImGui_ImplDX11_InvalidateDeviceObjects();
Check(ImGui_ImplDX11_CreateDeviceObjects(), "the DX11 backend recreates its objects");
Check(facet::CreateDeviceObjects(), "facet::CreateDeviceObjects recreates Facet's objects");
```

Afterwards SVG images are rasterized again and images from files load again (asynchronously);
images created from memory or pixels report `ImageStatus::Lost` and are created again by the
application.

**A new device** (`DXGI_ERROR_DEVICE_REMOVED` or `DXGI_ERROR_DEVICE_RESET` from `Present`). Facet
and the backend stop, the device is created again, both start again, and every image is loaded
again, because handles do not survive `facet::Shutdown`. Registered icons stay. From
`RunWindow`:

```cpp
facet::Shutdown();
ImGui_ImplDX11_Shutdown();
window.backBuffer.Reset();
window.swapChain.Reset();
window.context.Reset();
window.device.Reset();
if (!CreateDeviceAndSwapChain(window))
    break;
ImGui_ImplDX11_Init(window.device.Get(), window.context.Get());
init.device = window.device.Get();
init.context = window.context.Get();
facet::Init(init);
LoadAssets(assets);
```

## 8. Shut down

Facet stops first, then the backends, then the Dear ImGui context. From `RunWindow`:

```cpp
ReleaseAssets(assets);
facet::Shutdown();
ImGui_ImplDX11_Shutdown();
ImGui_ImplWin32_Shutdown();
ImGui::DestroyContext();
```

`facet::Shutdown` releases every Direct3D reference Facet holds and removes the fonts it added;
it is safe to call when Facet is not initialized.

## 9. Build and run the example

The Facet build compiles the example as a separate project and runs it as the test
`facet_consumer` when `FACET_BUILD_CONSUMER_TEST` is on:

```
cmake --preset msvc-x64 -DFACET_BUILD_CONSUMER_TEST=ON
cmake --build --preset msvc-x64-release
ctest --preset msvc-x64-release
```

Without arguments the program renders offscreen on a WARP device (no GPU needed), operates the
toggle with queued mouse input, checks the pixels, both device loss paths and the images, and
exits with 0. `build\msvc-x64\consumer\bin\Release\facet_consumer.exe --window` opens the same
content in a window. The example also configures on its own; the commands are at the top of
`examples/consumer/CMakeLists.txt`.
