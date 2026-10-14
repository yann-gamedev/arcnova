#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace arcnova::core {

struct ParsedCommand {
    std::string name;
    std::vector<std::string> args;
    std::string flags;
    std::string rawInput;
};

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
