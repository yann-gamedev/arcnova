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
            // Piped input (and some hosts writing UTF-8) may start with a byte order
            // mark; without stripping it, the first command name is not recognised.
            constexpr char utf8Bom[] = "\xEF\xBB\xBF";
            if (line.compare(0, 3, utf8Bom) == 0) {
                line.erase(0, 3);
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
