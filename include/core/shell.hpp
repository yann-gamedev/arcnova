#pragma once

#include "core/config.hpp"
#include "core/parser.hpp"

#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>

namespace arcnova::core {

class Shell {
public:
    Shell();
    Shell(const Shell&) = delete;
    Shell& operator=(const Shell&) = delete;
    Shell(Shell&&) = delete;
    Shell& operator=(Shell&&) = delete;

    [[nodiscard]] std::string formatPrompt() const;
    [[nodiscard]] bool isRunning() const noexcept;
    void executeLine(const std::string& line);
    void executeCommand(const ParsedCommand& command, bool captureOutput = false);
    [[nodiscard]] const std::filesystem::path& getCurrentPath() const noexcept;
    void handleParseError() noexcept;
    void handleInterrupt() noexcept;

private:
    struct Command {
        std::string description;
        std::function<int(const ParsedCommand&)> handler;
        bool acceptsArguments = false;
    };

    std::filesystem::path currentPath;
    std::filesystem::path previousPath;
    std::filesystem::path homePath;
    std::unordered_map<std::string, Command> commands;
    ConfigManager config;
    bool running = true;
    int lastExitCode = 0;

    void refreshCurrentPath();
    void refreshAliasCompletions() const;
    int clearScreen();
    int showHelp() const;
};

} // namespace arcnova::core
