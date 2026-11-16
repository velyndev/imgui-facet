// The single translation unit that compiles the vendored stb implementations.
// Every other file includes the stb headers without the *_IMPLEMENTATION macros.
// The headers are included as external (system) headers, so their warnings stay silent.

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WINDOWS_UTF8
#define STBIW_WINDOWS_UTF8

#if defined(_MSC_VER)
#pragma warning(push, 0)
#pragma warning(disable : 4996)
#endif

#include <stb_image.h>
#include <stb_image_write.h>

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
