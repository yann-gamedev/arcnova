#pragma once

#include <array>
#include <string>

namespace arcnova::ui::colors {

struct ColorPalette {
    const char* primary;
    const char* accent;
    const char* border;
    const char* dirColor;
};

struct ThemePreset {
    const char* name;
    ColorPalette palette;
};

inline constexpr std::array<ThemePreset, 4> themePresets{{
    {"default", {"\033[1;38;5;39m", "\033[1;38;5;214m", "\033[38;5;244m", "\033[1;38;5;75m"}},
    {"dracula", {"\033[1;38;5;141m", "\033[1;38;5;212m", "\033[38;5;61m", "\033[1;38;5;141m"}},
    {"nord", {"\033[1;38;5;110m", "\033[1;38;5;152m", "\033[38;5;60m", "\033[1;38;5;110m"}},
    {"matrix", {"\033[1;38;5;46m", "\033[38;5;34m", "\033[38;5;22m", "\033[1;38;5;46m"}}
}};

namespace detail {
inline const ThemePreset* activeTheme = &themePresets.front();
} // namespace detail

[[nodiscard]] inline const ColorPalette& currentTheme() noexcept {
    return detail::activeTheme->palette;
}

[[nodiscard]] inline const char* currentThemeName() noexcept {
    return detail::activeTheme->name;
}

// Unknown names leave the active palette unchanged.
[[nodiscard]] inline bool setTheme(const std::string& themeName) noexcept {
    for (const auto& preset : themePresets) {
        if (themeName == preset.name) {
            detail::activeTheme = &preset;
            return true;
        }
    }
    return false;
}

inline constexpr char Cyan[] = "\x1b[38;2;0;191;255m"; // Deep Sky Blue (#00BFFF)
inline constexpr char Gold[] = "\x1b[38;2;255;191;0m"; // Warm Amber (#FFBF00)
inline constexpr char MutedGray[] = "\x1b[38;2;128;128;128m";
inline constexpr char Green[] = "\x1b[32m";
inline constexpr char Red[] = "\x1b[31m";
inline constexpr char Reset[] = "\x1b[0m";
inline constexpr char Bold[] = "\x1b[1m";

} // namespace arcnova::ui::colors
