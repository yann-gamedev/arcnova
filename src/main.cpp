#include "core/terminal.hpp"
#include "core/shell.hpp"
#include "ui/banner.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

int main() {
    try {
        arcnova::core::initConsole();
        arcnova::ui::renderWelcomeBanner();

        arcnova::core::Shell shell;
        std::string line;
        while (shell.isRunning()) {
            std::cout << shell.formatPrompt() << std::flush;
            const bool readSucceeded = static_cast<bool>(std::getline(std::cin, line));
            if (arcnova::core::consumeInterrupt()) {
                std::cin.clear();
                shell.handleInterrupt();
                std::cout << '\n';
                continue;
            }
            if (!readSucceeded) {
                if (std::cin.bad()) {
                    std::cerr << "ArcNova: failed to read standard input.\n";
                    return EXIT_FAILURE;
                }
                break; // EOF also exits cleanly for redirected input.
            }
            shell.executeLine(line);
        }
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "ArcNova: " << error.what() << '\n';
    } catch (...) {
        std::cerr << "ArcNova: unexpected error.\n";
    }
    return EXIT_FAILURE;
}
