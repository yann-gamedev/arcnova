#include "core/shell.hpp"
#include "core/input.hpp"
#include "core/executor.hpp"
#include "core/terminal.hpp"
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

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <system_error>
#include <vector>
#include <utility>

namespace arcnova::core {
namespace {

class CoutRedirect {
public:
    explicit CoutRedirect(std::streambuf* buffer)
        : previousState_(std::cout.rdstate()) {
        std::cout.flush();
        previousBuffer_ = std::cout.rdbuf(buffer);
    }
    ~CoutRedirect() {
        // Always restore the console, including when a command throws.
        try { std::cout.flush(); } catch (...) {}
        std::cout.rdbuf(previousBuffer_);
        std::cout.clear(previousState_);
    }
    CoutRedirect(const CoutRedirect&) = delete;
    CoutRedirect& operator=(const CoutRedirect&) = delete;
private:
    std::ios::iostate previousState_;
    std::streambuf* previousBuffer_ = nullptr;
};

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
    std::vector<std::string> names;
    names.reserve(commands.size());
    for (const auto& entry : commands) { names.push_back(entry.first); }
    InputHandler::setCommandNames(std::move(names));
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
        const auto pipelines = parseLine(line, currentPath);
        ChainOp precedingOp = ChainOp::NONE;
        for (const auto& pipeline : pipelines) {
            if (!running) { break; }
            const bool shouldRun = precedingOp != ChainOp::AND || lastExitCode == 0;
            if (shouldRun) {
                std::string pipedInput;
                for (std::size_t index = 0; index < pipeline.pipedCommands.size(); ++index) {
                    if (!running) { break; }
                    auto command = pipeline.pipedCommands[index];
                    if (index != 0) { command.pipedInput = std::move(pipedInput); }
                    const bool capture = index + 1 < pipeline.pipedCommands.size();
                    if (capture) {
                        std::stringstream output;
                        {
                            CoutRedirect redirect(output.rdbuf());
                            executeCommand(command, true);
                        }
                        pipedInput = output.str();
                    } else {
                        executeCommand(command);
                    }
                }
            }
            precedingOp = pipeline.nextChainOp;
        }
    } catch (const ParseError& error) {
        handleParseError();
        std::cerr << "ArcNova: parse error: " << error.what() << '\n';
    }
}

void Shell::executeCommand(const ParsedCommand& parsed, bool captureOutput) {
    const auto& name = parsed.name;
    if (name.empty()) {
        return; // Blank input preserves the preceding command's status.
    }

    const auto command = commands.find(name);
    if (command == commands.end()) {
        std::cout << std::flush;
        std::cerr << std::flush;
        lastExitCode = executeExternalCommand(parsed.rawInput, currentPath,
                                              parsed.redirectOutFile, parsed.appendMode,
                                              parsed.pipedInput ? &*parsed.pipedInput : nullptr,
                                              captureOutput);
        // Consume the notification after the child handles Ctrl+C; do not discard
        // the next input line because of an interrupt from the previous command.
        (void)consumeInterrupt();
        refreshCurrentPath();
        return;
    }

    if (!command->second.acceptsArguments && (!parsed.args.empty() || !parsed.flags.empty())) {
        std::cerr << "ArcNova: " << name << " does not accept arguments.\n";
        lastExitCode = 2;
        return;
    }
    if (parsed.redirectOutFile.empty()) {
        lastExitCode = command->second.handler(parsed);
    } else {
        const auto path = currentPath / std::filesystem::u8path(parsed.redirectOutFile);
        std::ofstream output(path, std::ios::out
                             | (parsed.appendMode ? std::ios::app : std::ios::trunc));
        if (!output) {
            std::cerr << "ArcNova: cannot open output file: " << parsed.redirectOutFile << '\n';
            lastExitCode = 1;
            return;
        }
        {
            CoutRedirect redirect(output.rdbuf());
            lastExitCode = command->second.handler(parsed);
            std::cout.flush();
            if (!std::cout) { lastExitCode = 1; }
        }
        output.close();
        if (!output) {
            std::cerr << "ArcNova: cannot write output file: " << parsed.redirectOutFile << '\n';
            lastExitCode = 1;
        }
    }
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
