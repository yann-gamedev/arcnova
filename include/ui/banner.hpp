#pragma once

#include "ui/colors.hpp"

#include <iostream>

namespace arcnova::ui {

inline void renderWelcomeBanner() {
    using namespace colors;
    const auto& palette = currentTheme();

    std::cout
        << palette.border << u8"╭────────────────────────────────────────────────────────────────────╮\n"
        << u8"│                                                                    │\n"
        << u8"│             " << palette.primary << u8"▲" << palette.border << u8"                                                      │\n"
        << u8"│            " << palette.primary << u8"╱ ╲" << palette.border << u8"                                                     │\n"
        << u8"│           " << palette.primary << u8"╱ " << palette.accent << u8"◯" << palette.primary << u8" ╲"
        << Reset << Bold << u8"       A R C N O V A   S H E L L" << Reset << palette.border << u8"                    │\n"
        << u8"│          " << palette.primary << u8"╱─────╲" << palette.border << u8"      v1.0.0 • Windows_NT x64                      │\n"
        << u8"│           " << palette.primary << u8"─────" << palette.border << u8"                                                    │\n"
        << u8"│                                                                    │\n"
        << u8"│" << Reset << u8"   Welcome to ArcNova — Fast Linux-styled Command Shell for Win32   " << palette.border << u8"│\n"
        << u8"│" << Reset << u8"   Type 'help' to view available commands or 'arcfetch' for info.   " << palette.border << u8"│\n"
        << u8"╰────────────────────────────────────────────────────────────────────╯" << Reset << '\n';
}

} // namespace arcnova::ui
