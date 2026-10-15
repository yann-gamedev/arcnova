#pragma once

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace arcnova::core {

enum class ChainOp { NONE, AND, SEMICOLON };

struct ParsedCommand {
    std::string name;
    std::vector<std::string> args;
    std::string flags;
    std::string rawInput;
    std::string redirectOutFile;
    bool appendMode = false;
    // Disengaged outside a pipe; engaged even when the upstream output is empty.
    std::optional<std::string> pipedInput;
};

struct CommandPipeline {
    std::vector<ParsedCommand> pipedCommands;
    ChainOp nextChainOp = ChainOp::NONE;
};

// Splits unquoted chaining/pipe operators; throws ParseError for invalid syntax.
[[nodiscard]] std::vector<CommandPipeline> parseLine(
    const std::string& input, const std::filesystem::path& currentPath);

class ParseError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Quotes group text and are removed; backslashes remain literal for Windows paths.
// -- ends flag parsing; a lone - is an argument. Relative paths remain relative.
// currentPath is reserved for future path resolution, not applied to ordinary args.
// Throws ParseError for unclosed quotes or an unavailable USERPROFILE when expanding ~.
[[nodiscard]] ParsedCommand parseInput(const std::string& input,
                                       const std::filesystem::path& currentPath);

} // namespace arcnova::core
