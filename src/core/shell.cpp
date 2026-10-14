#include "core/shell.hpp"
#include "commands/fs_commands.hpp"
#include "commands/sys_commands.hpp"
#include "ui/banner.hpp"
#include "ui/colors.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <iomanip>
#include <iostream>
#include <sstream>
#include <system_error>
#include <vector>

namespace arcnova::core {
namespace {

std::filesystem::path findHomePath() {
    const DWORD size = GetEnvironmentVariableW(L"USERPROFILE", nullptr, 0);
    if (size == 0) {
        return {};
    }
    std::vector<wchar_t> buffer(size);
    const DWORD length = GetEnvironmentVariableW(L"USERPROFILE", buffer.data(), size);
    if (length == 0 || length >= size) {
        return {};
    }
    return std::filesystem::path(buffer.data()).lexically_normal();
}

bool sameComponent(const std::filesystem::path& left,
                   const std::filesystem::path& right) {
    return CompareStringOrdinal(left.c_str(), -1, right.c_str(), -1, TRUE)
           == CSTR_EQUAL;
}

std::string displayPath(const std::filesystem::path& current,
                        const std::filesystem::path& home) {
    if (current.empty()) {
        return "?";
    }
    if (!home.empty() && home.is_absolute()) {
        auto currentPart = current.begin();
        auto homePart = home.begin();
        for (; homePart != home.end(); ++homePart) {
            if (homePart->empty()) {
                continue;
            }
            if (currentPart == current.end() || !sameComponent(*currentPart, *homePart)) {
                break;
            }
            ++currentPart;
        }
        if (homePart == home.end()) {
            std::filesystem::path suffix;
            for (; currentPart != current.end(); ++currentPart) {
                suffix /= *currentPart;
            }
            return suffix.empty() ? "~" : "~/" + suffix.generic_u8string();
        }
    }
    return current.generic_u8string();
}

} // namespace

Shell::Shell() : homePath(findHomePath()) {
    refreshCurrentPath();
    commands.emplace("grep", Command{"Find text in a file (-i, -n)", cmd::grep, true});
    commands.emplace("echo", Command{"Print text arguments", cmd::echo, true});
    commands.emplace("whoami", Command{"Print current Windows user", cmd::whoami, true});
    commands.emplace("arcfetch", Command{"Show logo and live system information", cmd::arcfetch, true});
    commands.emplace("touch", Command{"Create files or update timestamps", cmd::touch, true});
    commands.emplace("mkdir", Command{"Create directories (-p: parents)", cmd::mkdir, true});
    commands.emplace("rm", Command{"Remove files/directories (-r, -f)", cmd::rm, true});
    commands.emplace("cp", Command{"Copy source to destination (-r)", cmd::cp, true});
    commands.emplace("mv", Command{"Move or rename source to destination", cmd::mv, true});
    commands.emplace("cat", Command{"Print text files (-n: line numbers)", cmd::cat, true});
    commands.emplace("pwd", Command{"Print working directory (-W: Windows)", cmd::pwd, true});
    commands.emplace("ls", Command{"List directory entries (-a, -l, -h)", cmd::ls, true});
    commands.emplace("cd", Command{"Change directory (path, ~, or -)", [this](const ParsedCommand& command) {
        return cmd::cd(command, previousPath);
    }, true});
    commands.emplace("clear", Command{"Clear screen and show welcome banner", [this](const ParsedCommand&) {
        return clearScreen();
    }});
    commands.emplace("cls", Command{"Alias for clear", [this](const ParsedCommand&) {
        return clearScreen();
    }});
    commands.emplace("help", Command{"Show available internal commands", [this](const ParsedCommand&) {
        return showHelp();
    }});
    commands.emplace("exit", Command{"Exit ArcNova Shell", [this](const ParsedCommand&) {
        running = false;
        return 0;
    }});
}

void Shell::refreshCurrentPath() {
    std::error_code error;
    const auto path = std::filesystem::current_path(error);
    if (error) {
        std::cerr << "ArcNova: cannot read current directory: " << error.message() << '\n';
        lastExitCode = 1;
        return;
    }
    currentPath = path.lexically_normal();
}

std::string Shell::formatPrompt() const {
    return "arcnova " + displayPath(currentPath, homePath) + " "
           + (lastExitCode == 0 ? ui::colors::Green : ui::colors::Red)
           + u8"❯" + ui::colors::Reset + " ";
}

bool Shell::isRunning() const noexcept {
    return running;
}

void Shell::handleInterrupt() noexcept {
    lastExitCode = 130;
}

const std::filesystem::path& Shell::getCurrentPath() const noexcept {
    return currentPath;
}

void Shell::handleParseError() noexcept {
    lastExitCode = 2;
}

void Shell::executeLine(const std::string& line) {
    try {
        executeCommand(parseInput(line, currentPath));
    } catch (const ParseError& error) {
        handleParseError();
        std::cerr << "ArcNova: " << error.what() << '\n';
    }
}

void Shell::executeCommand(const ParsedCommand& parsed) {
    const auto& name = parsed.name;
    if (name.empty()) {
        return; // Blank input preserves the preceding command's status.
    }

    const auto command = commands.find(name);
    if (command == commands.end()) {
        std::cerr << "ArcNova: command not found: " << name << '\n';
        lastExitCode = 127;
        return;
    }

    if (!command->second.acceptsArguments && (!parsed.args.empty() || !parsed.flags.empty())) {
        std::cerr << "ArcNova: " << name << " does not accept arguments.\n";
        lastExitCode = 2;
        return;
    }
    lastExitCode = command->second.handler(parsed);
    refreshCurrentPath();
}

int Shell::clearScreen() {
    std::cout << "\x1b[2J\x1b[H";
    ui::renderWelcomeBanner();
    return std::cout ? 0 : 1;
}

int Shell::showHelp() const {
    // Explicit ordering keeps the table stable despite unordered_map iteration order.
    constexpr const char* order[] = {"pwd", "cd", "ls", "touch", "mkdir", "rm", "cp", "mv", "cat", "grep", "echo", "whoami", "arcfetch", "clear", "cls", "help", "exit"};
    std::ostringstream table;
    constexpr const char* border = "+----------+------------------------------------------+\n";
    table << border
          << "| Command  | Description                              |\n"
          << border;
    for (const auto* name : order) {
        table << "| " << std::left << std::setw(8) << name
              << " | " << std::setw(40) << commands.at(name).description << " |\n";
    }
    table << border;
    std::cout << table.str();
    return std::cout ? 0 : 1;
}

} // namespace arcnova::core
