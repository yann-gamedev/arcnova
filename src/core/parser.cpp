#include "core/parser.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <utility>

namespace arcnova::core {
namespace {

struct Token {
    std::string text;
    std::size_t begin;
    std::size_t end;
    bool redirect;
};

std::vector<Token> tokenize(const std::string& input) {
    std::vector<Token> tokens;
    std::string token;
    char quote = '\0';
    bool started = false;
    std::size_t begin = 0;
    const auto finish = [&](std::size_t end) {
        if (started) {
            tokens.push_back({std::move(token), begin, end, false});
            token.clear();
            started = false;
        }
    };
    for (std::size_t index = 0; index < input.size(); ++index) {
        const char ch = input[index];
        if (quote != '\0') {
            if (ch == quote) {
                quote = '\0';
            } else {
                token += ch;
            }
        } else if (ch == '>') {
            finish(index);
            const auto operatorBegin = index;
            if (index + 1 < input.size() && input[index + 1] == '>') {
                ++index;
            }
            tokens.push_back({input.substr(operatorBegin, index - operatorBegin + 1),
                              operatorBegin, index + 1, true});
        } else if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
            finish(index);
        } else {
            if (!started) {
                begin = index;
                started = true;
            }
            if (ch == '"' || ch == '\'') {
                quote = ch;
            } else {
                token += ch;
            }
        }
    }
    if (quote != '\0') {
        throw ParseError("unclosed quote in command input");
    }
    finish(input.size());
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
    (void)currentPath; // Resolve redirection paths at execution time, after any cd.
    ParsedCommand result;
    const auto tokens = tokenize(input);
    bool parseFlags = true;
    bool hasName = false;
    std::size_t rawBegin = 0;
    for (std::size_t index = 0; index < tokens.size(); ++index) {
        const auto& token = tokens[index];
        if (token.redirect) {
            if (index + 1 == tokens.size() || tokens[index + 1].redirect
                || tokens[index + 1].text.empty()) {
                throw ParseError("output redirection requires a file name");
            }
            if (!result.redirectOutFile.empty()) {
                throw ParseError("multiple output redirections are not supported");
            }
            const auto& target = tokens[++index];
            result.redirectOutFile = expandTilde(target.text);
            result.appendMode = token.text == ">>";
            // Preserve original quoting for external commands, but remove redirection.
            result.rawInput += input.substr(rawBegin, token.begin - rawBegin);
            result.rawInput += ' ';
            rawBegin = target.end;
        } else if (!hasName) {
            result.name = token.text;
            if (result.name.empty()) {
                throw ParseError("command name cannot be empty");
            }
            hasName = true;
        } else if (parseFlags && token.text == "--") {
            parseFlags = false;
        } else if (parseFlags && token.text.size() > 1 && token.text.front() == '-') {
            result.flags += token.text.substr(1);
        } else {
            result.args.push_back(expandTilde(token.text));
        }
    }
    result.rawInput += input.substr(rawBegin);
    if (!hasName && !result.redirectOutFile.empty()) {
        throw ParseError("output redirection requires a command");
    }
    return result;
}

std::vector<CommandPipeline> parseLine(const std::string& input,
                                       const std::filesystem::path& currentPath) {
    std::vector<CommandPipeline> pipelines;
    CommandPipeline pipeline;
    char quote = '\0';
    std::size_t begin = 0;
    const auto addCommand = [&](std::size_t end) {
        auto command = parseInput(input.substr(begin, end - begin), currentPath);
        if (command.name.empty()) {
            throw ParseError("expected command after or before operator");
        }
        pipeline.pipedCommands.push_back(std::move(command));
    };
    for (std::size_t index = 0; index < input.size(); ++index) {
        const char ch = input[index];
        if (quote != '\0') {
            if (ch == quote) { quote = '\0'; }
            continue;
        }
        if (ch == '"' || ch == '\'') {
            quote = ch;
            continue;
        }
        const bool andOp = ch == '&' && index + 1 < input.size() && input[index + 1] == '&';
        if (ch != ';' && ch != '|' && !andOp) { continue; }
        addCommand(index);
        if (ch != '|') {
            pipeline.nextChainOp = andOp ? ChainOp::AND : ChainOp::SEMICOLON;
            pipelines.push_back(std::move(pipeline));
            pipeline = CommandPipeline{};
        }
        if (andOp) { ++index; }
        begin = index + 1;
    }
    if (quote != '\0') {
        throw ParseError("unclosed quote in command input");
    }
    const bool hasTail = input.find_first_not_of(" \t\r\n", begin) != std::string::npos;
    if (hasTail) {
        addCommand(input.size());
        pipelines.push_back(std::move(pipeline));
    } else if (!pipeline.pipedCommands.empty()
               || (!pipelines.empty() && pipelines.back().nextChainOp == ChainOp::AND)) {
        throw ParseError("expected command after operator");
    }
    return pipelines;
}

} // namespace arcnova::core
