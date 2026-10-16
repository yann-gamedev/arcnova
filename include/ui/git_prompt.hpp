#pragma once

#include "ui/colors.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

namespace arcnova::ui {
namespace git_prompt_detail {

struct Handle {
    HANDLE value = nullptr;
    ~Handle() {
        if (value != nullptr && value != INVALID_HANDLE_VALUE) { CloseHandle(value); }
    }
    Handle() = default;
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
};

inline bool isDirty(const std::filesystem::path& directory) {
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    Handle reader;
    Handle writer;
    if (!CreatePipe(&reader.value, &writer.value, &security, 0)
        || !SetHandleInformation(reader.value, HANDLE_FLAG_INHERIT, 0)) { return false; }
    Handle nullDevice;
    nullDevice.value = CreateFileW(L"NUL", GENERIC_READ | GENERIC_WRITE,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE, &security,
                                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (nullDevice.value == INVALID_HANDLE_VALUE) { return false; }

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = nullDevice.value;
    startup.hStdOutput = writer.value;
    startup.hStdError = nullDevice.value;
    PROCESS_INFORMATION process{};
    // Explicit .git prevents malformed fixtures from discovering an outer repository.
    // Disable optional index writes and fsmonitor hooks; -uno intentionally ignores untracked files.
    wchar_t command[] = L"git.exe --no-optional-locks --git-dir=.git --work-tree=. "
                        L"-c core.fsmonitor=false status --porcelain -uno";
    if (!CreateProcessW(nullptr, command, nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
                        nullptr, directory.c_str(), &startup, &process)) { return false; }
    Handle child;
    child.value = process.hProcess;
    Handle thread;
    thread.value = process.hThread;
    CloseHandle(writer.value);
    writer.value = nullptr;

    constexpr ULONGLONG timeoutMs = 200;
    const auto started = GetTickCount64();
    bool hasOutput = false;
    while (GetTickCount64() - started < timeoutMs) {
        DWORD available = 0;
        if (PeekNamedPipe(reader.value, nullptr, 0, nullptr, &available, nullptr)
            && available != 0) {
            char buffer[4096];
            DWORD received = 0;
            const DWORD count = available < sizeof(buffer) ? available : sizeof(buffer);
            if (!ReadFile(reader.value, buffer, count, &received, nullptr)) { break; }
            hasOutput = hasOutput || received != 0;
            continue; // Drain while running so a large status cannot fill the pipe.
        }
        const DWORD wait = WaitForSingleObject(child.value, 5);
        if (wait == WAIT_OBJECT_0) {
            // Capture bytes written between the previous peek and process exit.
            available = 0;
            if (PeekNamedPipe(reader.value, nullptr, 0, nullptr, &available, nullptr)) {
                hasOutput = hasOutput || available != 0;
            }
            DWORD exitCode = 1;
            return GetExitCodeProcess(child.value, &exitCode) && exitCode == 0 && hasOutput;
        }
        if (wait == WAIT_FAILED) { break; }
    }
    // Missing Git, errors, and timeouts leave the branch visible without a dirty marker.
    TerminateProcess(child.value, 1);
    WaitForSingleObject(child.value, 50);
    return false;
}

} // namespace git_prompt_detail

[[nodiscard]] inline std::string getGitPromptInfo(const std::filesystem::path& currentPath) {
    if (currentPath.empty()) { return {}; }

    std::error_code ec;
    auto directory = std::filesystem::absolute(currentPath, ec);
    if (ec) { return {}; }

    while (!directory.empty()) {
        auto gitPath = directory / ".git";
        const auto status = std::filesystem::status(gitPath, ec);
        if (ec && ec != std::errc::no_such_file_or_directory) { return {}; }

        if (std::filesystem::exists(status)) {
            if (std::filesystem::is_regular_file(status)) {
                // Worktrees and submodules store a gitdir pointer instead of a directory.
                std::ifstream gitFile(gitPath);
                std::string target;
                constexpr std::string_view prefix = "gitdir: ";
                if (!std::getline(gitFile, target)) { return {}; }
                if (!target.empty() && target.back() == '\r') { target.pop_back(); }
                if (target.compare(0, prefix.size(), prefix) != 0
                    || target.size() == prefix.size()) { return {}; }
                gitPath = std::filesystem::u8path(target.substr(prefix.size()));
                if (gitPath.is_relative()) { gitPath = directory / gitPath; }
            } else if (!std::filesystem::is_directory(status)) {
                return {};
            }

            std::ifstream headFile(gitPath / "HEAD");
            std::string head;
            if (!std::getline(headFile, head)) { return {}; }
            if (!head.empty() && head.back() == '\r') { head.pop_back(); }

            constexpr std::string_view prefix = "ref: refs/heads/";
            std::string info;
            if (head.compare(0, prefix.size(), prefix) == 0) {
                info = head.substr(prefix.size());
            } else if (head.size() >= 7
                       && head.find_first_not_of("0123456789abcdefABCDEF") == std::string::npos) {
                info = head.substr(0, 7);
            }
            // Missing or malformed HEAD must not fall back to an outer repository.
            if (info.empty()) { return {}; }
            if (git_prompt_detail::isDirty(directory)) {
                info += std::string(" ") + colors::Red + u8"✗" + colors::Reset
                        + colors::currentTheme().gitColor;
            }
            return info;
        }

        const auto parent = directory.parent_path();
        if (parent == directory) { break; }
        directory = parent;
    }
    return {};
}

} // namespace arcnova::ui
