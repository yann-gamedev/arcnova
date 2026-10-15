#include "commands/fs_commands.hpp"
#include "ui/colors.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <fstream>
#include <limits>
#include <iostream>
#include <sstream>
#include <system_error>
#include <vector>

namespace arcnova::cmd {
namespace {
namespace fs = std::filesystem;

int fail(const char* name, const std::string& message) {
    std::cerr << "ArcNova: " << name << ": " << message << '\n';
    return 1;
}

bool valid(const core::ParsedCommand& command, const std::string& allowed,
           std::size_t maxArgs) {
    if (command.args.size() > maxArgs) {
        std::cerr << "ArcNova: " << command.name << ": too many arguments\n";
        return false;
    }
    for (const char flag : command.flags) {
        if (allowed.find(flag) == std::string::npos) {
            std::cerr << "ArcNova: " << command.name << ": unsupported flag -" << flag << '\n';
            return false;
        }
    }
    return true;
}

std::string sizeText(std::uintmax_t size, bool human) {
    if (!human) {
        return std::to_string(size);
    }
    constexpr const char* units[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB"};
    double value = static_cast<double>(size);
    std::size_t unit = 0;
    while (value >= 1024.0 && unit < 6) {
        value /= 1024.0;
        ++unit;
    }
    std::ostringstream text;
    text << std::fixed << std::setprecision(unit == 0 ? 0 : 1) << value << units[unit];
    return text.str();
}

std::string timeText(fs::file_time_type time) {
    const auto systemTime = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        time - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
    const auto stamp = std::chrono::system_clock::to_time_t(systemTime);
    const auto* local = std::localtime(&stamp); // REPL metadata rendering is single-threaded.
    if (local == nullptr) {
        return "????????????????";
    }
    std::ostringstream text;
    text << std::put_time(local, "%Y-%m-%d %H:%M");
    return text.str();
}
} // namespace

int pwd(const core::ParsedCommand& command) {
    if (!valid(command, "W", 0)) {
        return 2;
    }
    try {
        std::error_code ec;
        const auto path = fs::current_path(ec);
        if (ec) {
            return fail("pwd", ec.message());
        }
        std::cout << (command.flags.find('W') == std::string::npos
                          ? path.generic_string() : path.u8string()) << '\n';
        return std::cout ? 0 : 1;
    } catch (const fs::filesystem_error& error) {
        return fail("pwd", error.what());
    }
}

int cd(const core::ParsedCommand& command, fs::path& previousPath) {
    if (!valid(command, "", 1)) {
        return 2;
    }
    try {
        fs::path target;
        const bool goBack = !command.args.empty() && command.args.front() == "-";
        if (goBack) {
            if (previousPath.empty()) {
                return fail("cd", "previous directory is not set");
            }
            target = previousPath;
        } else if (command.args.empty()) {
            const DWORD size = GetEnvironmentVariableW(L"USERPROFILE", nullptr, 0);
            if (size == 0) {
                return fail("cd", "USERPROFILE is unavailable");
            }
            std::vector<wchar_t> home(size);
            const DWORD length = GetEnvironmentVariableW(L"USERPROFILE", home.data(), size);
            if (length == 0 || length >= size) {
                return fail("cd", "cannot read USERPROFILE");
            }
            target = home.data();
        } else {
            if (command.args.front().empty()) {
                return fail("cd", "empty directory path");
            }
            target = fs::u8path(command.args.front());
        }
        std::error_code ec;
        auto before = fs::current_path(ec);
        if (ec) {
            return fail("cd", ec.message());
        }
        fs::current_path(target, ec);
        if (ec) {
            return fail("cd", target.generic_u8string() + ": " + ec.message());
        }
        previousPath.swap(before);
        if (goBack) {
            std::cout << target.generic_u8string() << '\n';
        }
        return 0;
    } catch (const fs::filesystem_error& error) {
        return fail("cd", error.what());
    }
}

int ls(const core::ParsedCommand& command) {
    if (!valid(command, "alh", 1)) {
        return 2;
    }
    try {
        const bool all = command.flags.find('a') != std::string::npos;
        const bool longFormat = command.flags.find('l') != std::string::npos;
        const bool human = command.flags.find('h') != std::string::npos;
        const fs::path target = command.args.empty() ? fs::path(".") : fs::u8path(command.args.front());
        std::error_code ec;
        fs::directory_iterator iterator(target, ec);
        if (ec) {
            return fail("ls", target.generic_u8string() + ": " + ec.message());
        }
        std::vector<fs::directory_entry> entries;
        const fs::directory_iterator end;
        for (; iterator != end; iterator.increment(ec)) {
            if (ec) {
                break;
            }
            entries.push_back(*iterator);
        }
        int status = 0;
        if (ec) {
            status = fail("ls", ec.message());
        }
        std::sort(entries.begin(), entries.end(), [](const auto& left, const auto& right) {
            return left.path().filename() < right.path().filename();
        });
        for (const auto& entry : entries) {
            const auto name = entry.path().filename().u8string();
            const DWORD attributes = GetFileAttributesW(entry.path().c_str());
            if (attributes == INVALID_FILE_ATTRIBUTES) {
                const auto code = static_cast<int>(GetLastError());
                status = fail("ls", name + ": " + std::system_category().message(code));
                continue;
            }
            const bool hidden = (!name.empty() && name.front() == '.')
                                || (attributes & FILE_ATTRIBUTE_HIDDEN) != 0;
            if (hidden && !all) {
                continue;
            }
            const auto type = entry.status(ec);
            if (ec) {
                status = fail("ls", name + ": " + ec.message());
                continue;
            }
            const bool directory = fs::is_directory(type);
            auto extension = entry.path().extension().u8string();
            std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });
            const bool executable = extension == ".exe" || extension == ".bat";
            const char* color = hidden ? "\x1b[38;5;244m"
                               : directory ? ui::colors::currentTheme().dirColor
                               : executable ? "\x1b[1;38;5;82m" : "\x1b[37m";
            if (longFormat) {
                std::string size = "-";
                if (fs::is_regular_file(type)) {
                    const auto bytes = entry.file_size(ec);
                    if (ec) {
                        status = fail("ls", name + ": " + ec.message());
                        size = "?";
                    } else {
                        size = sizeText(bytes, human);
                    }
                }
                const auto modified = entry.last_write_time(ec);
                std::string timestamp = "????????????????";
                if (ec) {
                    status = fail("ls", name + ": " + ec.message());
                } else {
                    timestamp = timeText(modified);
                }
                std::cout << (directory ? "drwxr-xr-x" : executable ? "-rwxr-xr-x" : "-rw-r--r--")
                          << ' ' << std::setw(12) << size << ' ' << timestamp << ' ';
            }
            std::cout << color << name << (directory ? "/" : "") << ui::colors::Reset << '\n';
        }
        return std::cout ? status : 1;
    } catch (const fs::filesystem_error& error) {
        return fail("ls", error.what());
    }
}
namespace {

bool operands(const core::ParsedCommand& command, const std::string& flags,
              std::size_t minimum, std::size_t maximum = (std::numeric_limits<std::size_t>::max)()) {
    if (!valid(command, flags, maximum)) {
        return false;
    }
    if (command.args.size() < minimum) {
        std::cerr << "ArcNova: " << command.name << ": missing operand\n";
        return false;
    }
    for (const auto& argument : command.args) {
        if (argument.empty()) {
            std::cerr << "ArcNova: " << command.name << ": empty path is not allowed\n";
            return false;
        }
    }
    return true;
}

bool within(const fs::path& child, const fs::path& parent) {
    auto c = child.begin();
    for (auto p = parent.begin(); p != parent.end(); ++p, ++c) {
        if (c == child.end() || CompareStringOrdinal(c->c_str(), -1, p->c_str(), -1, TRUE) != CSTR_EQUAL) {
            return false;
        }
    }
    return true;
}

fs::path environmentPath(const wchar_t* name) {
    const DWORD size = GetEnvironmentVariableW(name, nullptr, 0);
    if (size == 0) {
        return {};
    }
    std::vector<wchar_t> value(size);
    const DWORD length = GetEnvironmentVariableW(name, value.data(), size);
    return length != 0 && length < size ? fs::path(value.data()) : fs::path{};
}

// Fail closed if a protected location cannot be resolved. Canonical paths also
// prevent .. and junction aliases from bypassing the protected-directory check.
bool protectedRemoval(const fs::path& target, std::error_code& ec) {
    const auto resolved = fs::weakly_canonical(target, ec).lexically_normal();
    if (ec) {
        return true;
    }
    if (resolved == resolved.root_path()) {
        return true;
    }
    std::vector<fs::path> protectedPaths = {
        resolved.root_path() / "Windows", resolved.root_path() / "Program Files",
        resolved.root_path() / "Program Files (x86)", resolved.root_path() / "ProgramData",
        resolved.root_path() / "Users", resolved.root_path() / "$Recycle.Bin",
        resolved.root_path() / "System Volume Information"
    };
    for (const auto* variable : {L"SystemRoot", L"ProgramFiles", L"ProgramFiles(x86)", L"ProgramData"}) {
        const auto path = environmentPath(variable);
        if (!path.empty()) {
            protectedPaths.push_back(path);
        }
    }
    for (const auto& path : protectedPaths) {
        const auto critical = fs::weakly_canonical(path, ec).lexically_normal();
        if (ec) {
            return true;
        }
        // Protect Users itself and ancestors, but allow ordinary user files.
        const bool users = CompareStringOrdinal(path.filename().c_str(), -1, L"Users", -1, TRUE) == CSTR_EQUAL;
        if (within(critical, resolved) || (!users && within(resolved, critical))) {
            return true;
        }
    }
    return false;
}

// Recursive deletion must not descend through junctions or other reparse points.
bool safeRemovalTree(const fs::path& target, std::error_code& ec) {
    const DWORD attributes = GetFileAttributesW(target.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        ec = std::error_code(static_cast<int>(GetLastError()), std::system_category());
        return false;
    }
    if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        return false;
    }
    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        return true;
    }
    fs::recursive_directory_iterator it(target, ec), end;
    while (!ec && it != end) {
        const DWORD childAttributes = GetFileAttributesW(it->path().c_str());
        if (childAttributes == INVALID_FILE_ATTRIBUTES) {
            ec = std::error_code(static_cast<int>(GetLastError()), std::system_category());
            return false;
        }
        if ((childAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
            return false;
        }
        it.increment(ec);
    }
    return !ec;
}

} // namespace

int touch(const core::ParsedCommand& command) {
    if (!operands(command, "", 1)) { return 2; }
    int status = 0;
    for (const auto& argument : command.args) {
        try {
            const auto path = fs::u8path(argument);
            std::error_code ec;
            const bool exists = fs::exists(path, ec);
            if (!ec && exists) {
                fs::last_write_time(path, fs::file_time_type::clock::now(), ec);
            } else if (!ec) {
                // Append avoids truncating a file created between the check and open.
                std::ofstream file(path, std::ios::app | std::ios::binary);
                if (!file) {
                    status = fail("touch", argument + ": cannot create file");
                    continue;
                }
                file.close();
                if (!file) { status = fail("touch", argument + ": cannot close file"); }
            }
            if (ec) { status = fail("touch", argument + ": " + ec.message()); }
        } catch (const fs::filesystem_error& error) { status = fail("touch", error.what()); }
    }
    return status;
}

int mkdir(const core::ParsedCommand& command) {
    if (!operands(command, "p", 1)) { return 2; }
    int status = 0;
    for (const auto& argument : command.args) {
        try {
            std::error_code ec;
            const auto path = fs::u8path(argument);
            const bool parents = command.flags.find('p') != std::string::npos;
            const bool created = parents ? fs::create_directories(path, ec) : fs::create_directory(path, ec);
            if (ec) { status = fail("mkdir", argument + ": " + ec.message()); }
            else if (!created && !parents) { status = fail("mkdir", argument + ": already exists"); }
        } catch (const fs::filesystem_error& error) { status = fail("mkdir", error.what()); }
    }
    return status;
}

int rm(const core::ParsedCommand& command) {
    if (!operands(command, "rf", 1)) { return 2; }
    const bool recursive = command.flags.find('r') != std::string::npos;
    const bool force = command.flags.find('f') != std::string::npos;
    int status = 0;
    for (const auto& argument : command.args) {
        try {
            const auto path = fs::u8path(argument);
            std::error_code ec;
            if (protectedRemoval(path, ec)) {
                status = fail("rm", argument + ": deletion refused (protected system path or unresolved safety check)");
                continue;
            }
            const auto type = fs::symlink_status(path, ec);
            if (ec == std::errc::no_such_file_or_directory || (!ec && !fs::exists(type))) {
                if (!force) { status = fail("rm", argument + ": no such file or directory"); }
                continue;
            }
            if (ec) { status = fail("rm", argument + ": " + ec.message()); continue; }
            if (fs::is_directory(type) && !recursive) {
                status = fail("rm", argument + ": directory requires -r");
                continue;
            }
            if (!safeRemovalTree(path, ec)) {
                status = fail("rm", argument + ": unsafe reparse point or unreadable directory tree"
                              + (ec ? ": " + ec.message() : ""));
                continue;
            }
            if (fs::is_directory(type)) { fs::remove_all(path, ec); }
            else { fs::remove(path, ec); }
            if (ec) { status = fail("rm", argument + ": " + ec.message()); }
        } catch (const fs::filesystem_error& error) { status = fail("rm", error.what()); }
    }
    return status;
}

namespace {
int transfer(const core::ParsedCommand& command, bool move) {
    const char* name = move ? "mv" : "cp";
    if (!operands(command, move ? "" : "r", 2, 2)) { return 2; }
    try {
        const auto source = fs::u8path(command.args[0]);
        auto destination = fs::u8path(command.args[1]);
        std::error_code ec;
        const auto sourceType = fs::status(source, ec);
        if (ec) { return fail(name, command.args[0] + ": " + ec.message()); }
        if (!fs::exists(sourceType)) { return fail(name, "source does not exist"); }
        const bool directory = fs::is_directory(sourceType);
        if (!move && directory && command.flags.find('r') == std::string::npos) {
            return fail(name, "directory copy requires -r");
        }
        const bool destinationDirectory = fs::is_directory(destination, ec);
        if (ec == std::errc::no_such_file_or_directory) { ec.clear(); }
        if (ec) { return fail(name, ec.message()); }
        if (destinationDirectory) { destination /= source.filename(); }
        const auto destinationType = fs::symlink_status(destination, ec);
        if (ec == std::errc::no_such_file_or_directory) { ec.clear(); }
        if (ec) { return fail(name, ec.message()); }
        if (fs::exists(destinationType)) { return fail(name, "destination already exists (overwrite disabled)"); }
        const auto resolvedSource = fs::canonical(source, ec);
        if (ec) { return fail(name, ec.message()); }
        const auto resolvedDestination = fs::weakly_canonical(destination, ec);
        if (ec) { return fail(name, ec.message()); }
        if (within(resolvedDestination, resolvedSource)) {
            return fail(name, "destination cannot be the source or inside the source directory");
        }
        if (move) {
            fs::rename(source, destination, ec);
        } else {
            fs::copy(source, destination, directory ? fs::copy_options::recursive | fs::copy_options::copy_symlinks
                                                    : fs::copy_options::none, ec);
        }
        return ec ? fail(name, ec.message()) : 0;
    } catch (const fs::filesystem_error& error) { return fail(name, error.what()); }
}
} // namespace

int cp(const core::ParsedCommand& command) { return transfer(command, false); }
int mv(const core::ParsedCommand& command) { return transfer(command, true); }

int cat(const core::ParsedCommand& command) {
    if (!operands(command, "n", 1)) { return 2; }
    int status = 0;
    std::uintmax_t number = 1;
    for (const auto& argument : command.args) {
        try {
            const auto path = fs::u8path(argument);
            std::error_code ec;
            if (!fs::is_regular_file(path, ec)) {
                status = fail("cat", argument + ": " + (ec ? ec.message() : "not a regular file"));
                continue;
            }
            std::ifstream file(path);
            if (!file) { status = fail("cat", argument + ": cannot open file"); continue; }
            std::string line;
            while (std::getline(file, line)) {
                if (command.flags.find('n') != std::string::npos) {
                    std::cout << std::setw(6) << number << "  ";
                }
                std::cout << line << '\n';
                ++number;
            }
            if (file.bad()) { status = fail("cat", argument + ": read failed"); }
        } catch (const fs::filesystem_error& error) { status = fail("cat", error.what()); }
    }
    return std::cout ? status : 1;
}
} // namespace arcnova::cmd
