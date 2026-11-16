#pragma once

// Theme selection shared by the capture and selftest modes.

#include <vector>

namespace facet::demo
{

enum class ThemeSelection
{
    Light,
    Dark,
    Both,
};

// The themes to run, as "dark" flags: light first when both are selected.
inline std::vector<bool> SelectedThemes(ThemeSelection selection)
{
    switch (selection)
    {
    case ThemeSelection::Light:
        return {false};
    case ThemeSelection::Dark:
        return {true};
    case ThemeSelection::Both:
        break;
    }
    return {false, true};
}

// Name used on the command line, in file names and in logs.
inline const char* ThemeName(bool dark)
{
    return dark ? "dark" : "light";
}

} // namespace facet::demo
