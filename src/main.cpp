#include "core/terminal.hpp"
#include "core/shell.hpp"
#include "ui/banner.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <iomanip>
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
            try {
                const auto command = arcnova::core::parseInput(line, shell.getCurrentPath());
                if (command.name.empty()) {
                    continue;
                }
                // Temporary parser diagnostics for Phase 2, Part 1.
                std::cout << "[parse] name=" << std::quoted(command.name) << " args=[";
                for (std::size_t index = 0; index < command.args.size(); ++index) {
                    if (index != 0) {
                        std::cout << ", ";
                    }
                    std::cout << std::quoted(command.args[index]);
                }
                std::cout << "] flags=" << std::quoted(command.flags) << '\n';
                shell.executeCommand(command);
            } catch (const arcnova::core::ParseError& error) {
                shell.handleParseError();
                std::cerr << "ArcNova: parse error: " << error.what() << '\n';
            }
        }
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "ArcNova: " << error.what() << '\n';
    } catch (...) {
        std::cerr << "ArcNova: unexpected error.\n";
    }
    return EXIT_FAILURE;
}
