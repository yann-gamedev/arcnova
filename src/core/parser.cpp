#include "core/parser.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <utility>

namespace arcnova::core {
namespace {

std::vector<std::string> tokenize(const std::string& input) {
    std::vector<std::string> tokens;
    std::string token;
    char quote = '\0';
    bool started = false;
    for (const char ch : input) {
        if (quote != '\0') {
            if (ch == quote) {
                quote = '\0';
            } else {
                token += ch;
            }
        } else if (ch == '"' || ch == '\'') {
            quote = ch;
            started = true;
        } else if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
            if (started) {
                tokens.push_back(std::move(token));
                token.clear();
                started = false;
            }
        } else {
            token += ch;
            started = true;
        }
    }
    if (quote != '\0') {
        throw ParseError("unclosed quote in command input");
    }
    if (started) {
        tokens.push_back(std::move(token));
    }
    return tokens;
}

std::string expandTilde(std::string argument) {
    // ~user is not supported: expand only ~ and ~/path (or ~\path).
    if (argument.empty() || argument.front() != '~'
        || (argument.size() > 1 && argument[1] != '/' && argument[1] != '\\')) {
        return argument;
    }
    const char* profile = std::getenv("USERPROFILE");
    if (profile == nullptr || *profile == '\0') {
        throw ParseError("cannot expand ~: USERPROFILE is unavailable");
    }
    std::string home(profile);
    std::replace(home.begin(), home.end(), '\\', '/');
    std::replace(argument.begin(), argument.end(), '\\', '/');
    if (argument.size() == 1) {
        return home;
    }
    while (!home.empty() && home.back() == '/') {
        home.pop_back();
    }
    return home + argument.substr(1);
}

} // namespace

ParsedCommand parseInput(const std::string& input,
                         const std::filesystem::path& currentPath) {
    (void)currentPath; // Parsing must not resolve non-path arguments against the CWD.
    ParsedCommand result;
    result.rawInput = input;
    auto tokens = tokenize(input);
    if (tokens.empty()) {
        return result;
    }
    result.name = std::move(tokens.front());
    if (result.name.empty()) {
        throw ParseError("command name cannot be empty");
    }
    bool parseFlags = true;
    for (std::size_t index = 1; index < tokens.size(); ++index) {
        auto& token = tokens[index];
        if (parseFlags && token == "--") {
            parseFlags = false;
        } else if (parseFlags && token.size() > 1 && token.front() == '-') {
            result.flags += token.substr(1);
        } else {
            result.args.push_back(expandTilde(std::move(token)));
        }
    }
    return result;
}

} // namespace arcnova::core
