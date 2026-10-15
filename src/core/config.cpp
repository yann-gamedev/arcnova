#include "core/config.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <system_error>
#include <utility>

namespace arcnova::core {
namespace {

constexpr const char* defaultRc =
    "# ~/.arcnovarc - ArcNova Shell configuration\n"
    "# Read once at startup. Blank lines and lines starting with # are ignored.\n"
    "#\n"
    "# Aliases:   alias <name>=\"<command>\"\n"
    "#   The alias replaces the first word of a command, including commands\n"
    "#   after |, ; and &&. Quote a command name (\"ll\") to bypass an alias.\n"
    "#\n"
    "# At runtime: alias (list), alias <name>=\"<command>\", unalias <name>.\n"
    "# Runtime changes are not saved; edit this file to keep them.\n"
    "\n"
    "alias ll=\"ls -la\"\n"
    "alias la=\"ls -a\"\n"
    "alias ..=\"cd ..\"\n";

bool isBlank(char ch) {
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

std::string trim(const std::string& text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) { return {}; }
    return text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
}

std::filesystem::path rcPath() {
    const DWORD size = GetEnvironmentVariableW(L"USERPROFILE", nullptr, 0);
    if (size == 0) { return {}; }
    std::vector<wchar_t> buffer(size);
    const DWORD length = GetEnvironmentVariableW(L"USERPROFILE", buffer.data(), size);
    if (length == 0 || length >= size) { return {}; }
    return std::filesystem::path(buffer.data()) / ".arcnovarc";
}

bool balancedQuotes(const std::string& text) {
    char quote = '\0';
    for (const char ch : text) {
        if (quote != '\0') { if (ch == quote) { quote = '\0'; } }
        else if (ch == '"' || ch == '\'') { quote = ch; }
    }
    return quote == '\0';
}

} // namespace

void ConfigManager::loadRcFile() {
    const auto path = rcPath();
    if (path.empty()) {
        std::cerr << "ArcNova: .arcnovarc: USERPROFILE is unavailable\n";
        return;
    }
    std::error_code ec;
    const bool exists = std::filesystem::exists(path, ec);
    if (ec) {
        std::cerr << "ArcNova: .arcnovarc: " << ec.message() << '\n';
        return;
    }
    if (!exists) {
        std::ofstream output(path, std::ios::binary);
        output << defaultRc;
        output.close();
        if (!output) { std::cerr << "ArcNova: cannot create default .arcnovarc\n"; }
    }

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        if (exists) { std::cerr << "ArcNova: cannot open .arcnovarc\n"; }
        return; // Without a file, the in-memory alias table simply stays empty.
    }
    std::string line;
    std::size_t number = 0;
    while (std::getline(file, line)) {
        ++number;
        if (number == 1 && line.compare(0, 3, "\xEF\xBB\xBF") == 0) { line.erase(0, 3); }
        line = trim(line);
        if (line.empty() || line.front() == '#') { continue; }
        const auto split = line.find_first_of(" \t");
        const auto keyword = line.substr(0, split);
        const auto rest = split == std::string::npos ? std::string{} : line.substr(split);
        std::string message;
        if (keyword == "alias") {
            if (!defineAlias(rest, message)) {
                std::cerr << "ArcNova: .arcnovarc:" << number << ": " << message << '\n';
            }
        } else if (keyword != "theme") { // theme is reserved for a later v1.3 stage.
            std::cerr << "ArcNova: .arcnovarc:" << number << ": unknown directive '"
                      << keyword << "'\n";
        }
    }
    if (file.bad()) { std::cerr << "ArcNova: failed to read .arcnovarc\n"; }
}

std::string ConfigManager::expandAlias(const std::string& input) const {
    if (aliases.empty()) { return input; }
    std::string result;
    result.reserve(input.size());
    bool segmentStart = true;
    char quote = '\0';
    std::size_t index = 0;
    while (index < input.size()) {
        const char ch = input[index];
        if (quote != '\0') {
            if (ch == quote) { quote = '\0'; }
            result += ch;
            ++index;
            continue;
        }
        if (segmentStart) {
            if (isBlank(ch)) { result += ch; ++index; continue; }
            segmentStart = false;
            // The command word ends where the parser would end the token.
            auto end = index;
            while (end < input.size() && !isBlank(input[end])
                   && input[end] != ';' && input[end] != '|' && input[end] != '>'
                   && input[end] != '"' && input[end] != '\''
                   && !(input[end] == '&' && end + 1 < input.size() && input[end + 1] == '&')) {
                ++end;
            }
            // A quote touching the word would merge into the same token: no expansion.
            const bool quoted = end < input.size() && (input[end] == '"' || input[end] == '\'');
            if (end > index && !quoted) {
                const auto found = aliases.find(input.substr(index, end - index));
                if (found != aliases.end()) {
                    result += found->second;
                    index = end;
                    continue;
                }
            }
            continue; // Re-process the current character as ordinary text.
        }
        if (ch == '"' || ch == '\'') {
            quote = ch;
        } else if (ch == ';' || ch == '|') {
            segmentStart = true;
        } else if (ch == '&' && index + 1 < input.size() && input[index + 1] == '&') {
            result += "&&";
            index += 2;
            segmentStart = true;
            continue;
        }
        result += ch;
        ++index;
    }
    return result;
}

bool ConfigManager::defineAlias(const std::string& definition, std::string& message) {
    const auto text = trim(definition);
    const auto split = text.find_first_of("= \t");
    const auto name = text.substr(0, split);
    if (name.empty() || name.front() == '-' || name.find_first_of("\"'|;&<>") != std::string::npos) {
        message = "invalid alias name: '" + name + "'";
        return false;
    }
    if (split == std::string::npos) {
        message = "missing command for alias '" + name + "'";
        return false;
    }
    auto value = trim(text.substr(split));
    if (!value.empty() && value.front() == '=') { value = trim(value.substr(1)); }
    if (value.size() >= 2 && (value.front() == '"' || value.front() == '\'')
        && value.back() == value.front()
        && value.find(value.front(), 1) == value.size() - 1) {
        value = value.substr(1, value.size() - 2);
    }
    value = trim(value);
    if (value.empty()) {
        message = "missing command for alias '" + name + "'";
        return false;
    }
    if (!balancedQuotes(value)) {
        message = "unclosed quote in alias '" + name + "'";
        return false;
    }
    aliases[name] = std::move(value);
    return true;
}

bool ConfigManager::removeAlias(const std::string& name) {
    return aliases.erase(name) != 0;
}

void ConfigManager::clearAliases() noexcept {
    aliases.clear();
}

const std::unordered_map<std::string, std::string>& ConfigManager::getAliases() const noexcept {
    return aliases;
}

std::vector<std::string> ConfigManager::aliasNames() const {
    std::vector<std::string> names;
    names.reserve(aliases.size());
    for (const auto& entry : aliases) { names.push_back(entry.first); }
    std::sort(names.begin(), names.end());
    return names;
}

} // namespace arcnova::core
