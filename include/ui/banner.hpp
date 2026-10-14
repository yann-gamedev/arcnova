#pragma once

#include "ui/colors.hpp"

#include <iostream>

namespace arcnova::ui {

inline void renderWelcomeBanner() {
    using namespace colors;

    std::cout
        << MutedGray << u8"╭────────────────────────────────────────────────────────────────────╮\n"
        << u8"│                                                                    │\n"
        << u8"│             " << Cyan << u8"▲" << MutedGray << u8"                                                      │\n"
        << u8"│            " << Cyan << u8"╱ ╲" << MutedGray << u8"                                                     │\n"
        << u8"│           " << Cyan << u8"╱ " << Gold << u8"◯" << Cyan << u8" ╲"
        << Reset << Bold << u8"       A R C N O V A   S H E L L" << Reset << MutedGray << u8"                    │\n"
        << u8"│          " << Cyan << u8"╱─────╲" << MutedGray << u8"      v1.0.0 • Windows_NT x64                      │\n"
        << u8"│           " << Cyan << u8"─────" << MutedGray << u8"                                                    │\n"
        << u8"│                                                                    │\n"
        << u8"│" << Reset << u8"   Welcome to ArcNova — Fast Linux-styled Command Shell for Win32   " << MutedGray << u8"│\n"
        << u8"│" << Reset << u8"   Type 'help' to view available commands or 'arcfetch' for info.   " << MutedGray << u8"│\n"
        << u8"╰────────────────────────────────────────────────────────────────────╯" << Reset << '\n';
}

} // namespace arcnova::ui
