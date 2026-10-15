#include "commands/sys_commands.hpp"
#include "ui/colors.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <lmcons.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <system_error>
#include <vector>

namespace arcnova::cmd {
namespace {

int error(const char* command, const std::string& message, int code = 1) {
    std::cerr << "ArcNova: " << command << ": " << message << '\n';
    return code;
}

bool noOperands(const core::ParsedCommand& command) {
    if (!command.args.empty() || !command.flags.empty()) {
        error(command.name.c_str(), "does not accept arguments or flags", 2);
        return false;
    }
    return true;
}

std::string utf8(const wchar_t* text) {
    const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, -1,
                                       nullptr, 0, nullptr, nullptr);
    if (size == 0) { return "Unavailable"; }
    std::string result(static_cast<std::size_t>(size), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, -1,
                            result.data(), size, nullptr, nullptr) == 0) {
        return "Unavailable";
    }
    result.pop_back();
    return result;
}

std::string userName() {
    std::array<wchar_t, UNLEN + 1> buffer{};
    DWORD size = static_cast<DWORD>(buffer.size());
    return GetUserNameW(buffer.data(), &size) != FALSE ? utf8(buffer.data()) : "Unavailable";
}

std::string hostName() {
    DWORD size = 0;
    GetComputerNameExW(ComputerNameDnsHostname, nullptr, &size);
    if (size == 0) { return "Unavailable"; }
    std::vector<wchar_t> buffer(size, L'\0');
    return GetComputerNameExW(ComputerNameDnsHostname, buffer.data(), &size) != FALSE
               ? utf8(buffer.data()) : "Unavailable";
}

std::string registryText(HKEY hive, const wchar_t* key, const wchar_t* value) {
    DWORD bytes = 0;
    constexpr DWORD flags = RRF_RT_REG_SZ; // x64 process uses the native registry view.
    if (RegGetValueW(hive, key, value, flags, nullptr, nullptr, &bytes) != ERROR_SUCCESS || bytes == 0) {
        return "Unavailable";
    }
    std::vector<wchar_t> buffer(static_cast<std::size_t>(bytes) / sizeof(wchar_t) + 1, L'\0');
    if (RegGetValueW(hive, key, value, flags, nullptr, buffer.data(), &bytes) != ERROR_SUCCESS) {
        return "Unavailable";
    }
    return utf8(buffer.data());
}

std::string osVersion() {
    // Registry build number avoids manifest-dependent GetVersionEx results.
    constexpr const wchar_t* key = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
    const auto build = registryText(HKEY_LOCAL_MACHINE, key, L"CurrentBuildNumber");
    const auto product = registryText(HKEY_LOCAL_MACHINE, key, L"ProductName");
    if (build == "Unavailable") { return "Windows x64 (build unavailable)"; }
    unsigned long number = 0;
    std::istringstream input(build);
    input >> number;
    std::string name = product;
    if (product.find("Windows 10") != std::string::npos || product.find("Windows 11") != std::string::npos) {
        name = number >= 22000 ? "Windows 11" : "Windows 10";
    }
    if (name == "Unavailable") { name = "Windows"; }
    return name + " x64 (build " + build + ")";
}

std::string memoryUsage() {
    MEMORYSTATUSEX memory{};
    memory.dwLength = sizeof(memory);
    if (GlobalMemoryStatusEx(&memory) == FALSE || memory.ullTotalPhys == 0) {
        return "Unavailable";
    }
    const auto used = memory.ullTotalPhys - memory.ullAvailPhys;
    constexpr double gib = 1024.0 * 1024.0 * 1024.0;
    const double percentage = 100.0 * static_cast<double>(used) / static_cast<double>(memory.ullTotalPhys);
    std::ostringstream text;
    text << std::fixed << std::setprecision(1) << static_cast<double>(used) / gib
         << " / " << static_cast<double>(memory.ullTotalPhys) / gib << " GiB (" << percentage << "%)";
    return text.str();
}

std::string stripAnsi(const std::string& text) {
    std::string result;
    result.reserve(text.size());
    for (std::size_t index = 0; index < text.size();) {
        if (text[index] != '\x1b') {
            result += text[index++];
            continue;
        }
        ++index;
        if (index == text.size()) { break; }
        const char type = text[index++];
        if (type == '[') {
            // CSI: parameters/intermediates followed by a final byte in 0x40..0x7e.
            while (index < text.size()) {
                const auto ch = static_cast<unsigned char>(text[index++]);
                if (ch >= 0x40 && ch <= 0x7e) { break; }
            }
        } else if (type == ']') {
            // OSC (including hyperlinks): terminated by BEL or ESC followed by backslash.
            while (index < text.size()) {
                if (text[index++] == '\a') { break; }
                if (text[index - 1] == '\x1b' && index < text.size() && text[index] == '\\') {
                    ++index;
                    break;
                }
            }
        }
    }
    return result;
}

std::string foldCase(std::string text) {
    // Byte-wise case folding supports ASCII; UTF-8 bytes are left intact.
    for (auto& ch : text) {
        if (ch >= 'A' && ch <= 'Z') { ch = static_cast<char>(ch + ('a' - 'A')); }
    }
    return text;
}

// Text after the command word, with original quoting (redirection already removed).
std::string operandText(const std::string& raw) {
    auto index = raw.find_first_not_of(" \t");
    char quote = '\0';
    for (; index != std::string::npos && index < raw.size(); ++index) {
        const char ch = raw[index];
        if (quote != '\0') { if (ch == quote) { quote = '\0'; } }
        else if (ch == '"' || ch == '\'') { quote = ch; }
        else if (ch == ' ' || ch == '\t') { break; }
    }
    if (index == std::string::npos || index >= raw.size()) { return {}; }
    const auto first = raw.find_first_not_of(" \t\r\n", index);
    if (first == std::string::npos) { return {}; }
    return raw.substr(first, raw.find_last_not_of(" \t\r\n") - first + 1);
}

void printAlias(const std::string& name, const std::string& value) {
    std::cout << "alias " << name << "=\"" << value << "\"\n";
}

} // namespace

int grep(const core::ParsedCommand& command) {
    if (command.args.empty() || command.args.size() > 2
        || (!command.pipedInput && command.args.size() != 2)
        || command.flags.find_first_not_of("in") != std::string::npos) {
        return error("grep", "usage: grep [-i] [-n] <pattern> [file] (file required without pipe)", 2);
    }
    try {
        std::ifstream file;
        std::istringstream piped;
        std::istream* input = nullptr;
        if (command.pipedInput) {
            piped.str(*command.pipedInput);
            input = &piped;
        } else {
            const auto path = std::filesystem::u8path(command.args[1]);
            std::error_code ec;
            if (!std::filesystem::is_regular_file(path, ec)) {
                return error("grep", command.args[1] + ": " + (ec ? ec.message() : "not a regular file"), 2);
            }
            file.open(path);
            if (!file) { return error("grep", "cannot open " + command.args[1], 2); }
            input = &file;
        }
        const bool insensitive = command.flags.find('i') != std::string::npos;
        const bool numbered = command.flags.find('n') != std::string::npos;
        const auto pattern = insensitive ? foldCase(command.args[0]) : command.args[0];
        std::string line;
        std::size_t number = 0;
        bool matched = false;
        while (std::getline(*input, line)) {
            ++number;
            // Match and highlight the same visible bytes, not ANSI color parameters.
            line = stripAnsi(line);
            const auto haystack = insensitive ? foldCase(line) : line;
            auto position = haystack.find(pattern);
            if (position == std::string::npos) { continue; }
            matched = true;
            if (numbered) { std::cout << number << ':'; }
            if (pattern.empty()) {
                std::cout << line << '\n';
                continue;
            }
            std::size_t printed = 0;
            while (position != std::string::npos) {
                std::cout << line.substr(printed, position - printed)
                          << ui::colors::Bold << ui::colors::Gold
                          << line.substr(position, pattern.size()) << ui::colors::Reset;
                printed = position + pattern.size();
                position = haystack.find(pattern, printed);
            }
            std::cout << line.substr(printed) << '\n';
        }
        if (input->bad() || !std::cout) { return error("grep", "I/O failure", 2); }
        return matched ? 0 : 1;
    } catch (const std::filesystem::filesystem_error& exception) {
        return error("grep", exception.what(), 2);
    }
}

int echo(const core::ParsedCommand& command) {
    if (!command.flags.empty()) {
        return error("echo", "flags are not supported; use echo -- to print text beginning with -", 2);
    }
    for (std::size_t index = 0; index < command.args.size(); ++index) {
        if (index != 0) { std::cout << ' '; }
        std::cout << command.args[index];
    }
    std::cout << '\n';
    return std::cout ? 0 : 1;
}

int whoami(const core::ParsedCommand& command) {
    if (!noOperands(command)) { return 2; }
    const auto user = userName();
    if (user == "Unavailable") { return error("whoami", "cannot query Windows user name"); }
    std::cout << user << '\n';
    return std::cout ? 0 : 1;
}

int arcfetch(const core::ParsedCommand& command) {
    if (!noOperands(command)) { return 2; }
    const auto minutes = GetTickCount64() / 60000ULL;
    const std::array<std::string, 8> info = {
        "ArcNova Shell", "OS:       " + osVersion(), "Hostname: " + hostName(),
        "User:     " + userName(),
        "CPU:      " + registryText(HKEY_LOCAL_MACHINE,
            L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", L"ProcessorNameString"),
        "RAM:      " + memoryUsage(),
        "Uptime:   " + std::to_string(minutes / 60ULL) + "h " + std::to_string(minutes % 60ULL) + "m",
        "Shell:    ArcNova v1.0.0"
    };
    const std::array<std::string, 8> logo = {
        u8"          ▲", u8"         ╱ ╲", u8"        ╱   ╲",
        std::string(u8"       ╱  ") + ui::colors::Gold + u8"◯" + ui::colors::Cyan + u8"  ╲",
        u8"      ╱       ╲", u8"     ╱─────────╲", u8"      ─────────", ""
    };
    std::cout << '\n';
    for (std::size_t row = 0; row < logo.size(); ++row) {
        // CHA sets the right-hand column independently of Unicode/ANSI byte lengths.
        std::cout << ui::colors::Cyan << logo[row] << ui::colors::Reset
                  << "\x1b[30G" << (row == 0 ? ui::colors::Bold : "")
                  << info[row] << ui::colors::Reset << '\n';
    }
    std::cout << '\n';
    return std::cout ? 0 : 1;
}

int alias(const core::ParsedCommand& command, core::ConfigManager& config) {
    // Use the raw text so `alias ll ls -la` keeps "-la" and quoting survives.
    const auto operand = operandText(command.rawInput);
    if (operand.empty()) {
        for (const auto& name : config.aliasNames()) {
            printAlias(name, config.getAliases().at(name));
        }
        return std::cout ? 0 : 1;
    }
    if (operand.find_first_of("= \t") == std::string::npos) {
        const auto found = config.getAliases().find(operand);
        if (found == config.getAliases().end()) { return error("alias", operand + ": not found"); }
        printAlias(found->first, found->second);
        return std::cout ? 0 : 1;
    }
    std::string message;
    if (!config.defineAlias(operand, message)) {
        return error("alias", message + " (usage: alias <name>=\"<command>\")", 2);
    }
    return 0;
}

int unalias(const core::ParsedCommand& command, core::ConfigManager& config) {
    if (command.flags == "a" && command.args.empty()) {
        config.clearAliases();
        return 0;
    }
    if (!command.flags.empty() || command.args.empty()) {
        return error("unalias", "usage: unalias <name...> | unalias -a", 2);
    }
    int status = 0;
    for (const auto& name : command.args) {
        if (!config.removeAlias(name)) { status = error("unalias", name + ": not found"); }
    }
    return status;
}
} // namespace arcnova::cmd
