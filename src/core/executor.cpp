#include "core/executor.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <array>
#include <iostream>
#include <limits>
#include <string>
#include <system_error>
#include <vector>

namespace arcnova::core {
namespace {
class Handle {
public:
    explicit Handle(HANDLE value = nullptr) noexcept : value_(value) {}
    ~Handle() { if (value_ != nullptr && value_ != INVALID_HANDLE_VALUE) { CloseHandle(value_); } }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    HANDLE get() const noexcept { return value_; }
    HANDLE* address() noexcept { return &value_; }
private:
    HANDLE value_;
};

int report(const char* operation, DWORD code) {
    std::cerr << "ArcNova: " << operation << ": "
              << std::system_category().message(static_cast<int>(code))
              << " (Win32 " << code << ")\n";
    return 1;
}

bool toWide(const std::string& input, std::wstring& output) {
    if (input.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())
        || input.find('\0') != std::string::npos) { return false; }
    const int length = static_cast<int>(input.size());
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input.data(), length, nullptr, 0);
    if (count == 0) { return false; }
    output.resize(static_cast<std::size_t>(count));
    return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input.data(), length,
                               output.data(), count) == count;
}

bool spawn(const wchar_t* application, const std::wstring& command,
           const wchar_t* directory, STARTUPINFOW& startup, PROCESS_INFORMATION& process) {
    // CreateProcessW can modify its command-line buffer.
    std::vector<wchar_t> buffer(command.begin(), command.end());
    buffer.push_back(L'\0');
    return CreateProcessW(application, buffer.data(), nullptr, nullptr, TRUE, 0,
                          nullptr, directory, &startup, &process) != FALSE;
}
} // namespace

int executeExternalCommand(const std::string& rawInput, const std::filesystem::path& currentPath) {
    if (rawInput.find_first_not_of(" \t\r\n") == std::string::npos) { return 0; }
    std::wstring command;
    if (!toWide(rawInput, command)) {
        std::cerr << "ArcNova: invalid UTF-8 or embedded NUL in command line\n";
        return 2;
    }
    if (command.size() >= 32767) {
        std::cerr << "ArcNova: command line exceeds Windows length limit\n";
        return 2;
    }

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    // Duplicate redirected handles as inheritable too, without modifying the
    // parent's handle flags. The child shares the same console or pipe/file.
    std::array<Handle, 3> inherited;
    constexpr DWORD ids[] = {STD_INPUT_HANDLE, STD_OUTPUT_HANDLE, STD_ERROR_HANDLE};
    for (std::size_t index = 0; index < inherited.size(); ++index) {
        const HANDLE source = GetStdHandle(ids[index]);
        if (source == nullptr || source == INVALID_HANDLE_VALUE) {
            return report("standard handle unavailable", ERROR_INVALID_HANDLE);
        }
        if (DuplicateHandle(GetCurrentProcess(), source, GetCurrentProcess(),
                            inherited[index].address(), 0, TRUE, DUPLICATE_SAME_ACCESS) == FALSE) {
            return report("DuplicateHandle", GetLastError());
        }
    }
    startup.hStdInput = inherited[0].get();
    startup.hStdOutput = inherited[1].get();
    startup.hStdError = inherited[2].get();
    PROCESS_INFORMATION process{};
    const wchar_t* directory = currentPath.empty() ? nullptr : currentPath.c_str();
    // Shell operators require cmd even when the first token is a native program.
    bool shellSyntax = false;
    bool quoted = false;
    for (const auto ch : command) {
        if (ch == L'"') { quoted = !quoted; }
        else if (!quoted && std::wstring(L"&|<>^%!").find(ch) != std::wstring::npos) { shellSyntax = true; }
    }
    bool started = !shellSyntax && spawn(nullptr, command, directory, startup, process);
    const DWORD directError = started || shellSyntax ? ERROR_SUCCESS : GetLastError();
    if (!started) {
        if (!shellSyntax && directError != ERROR_FILE_NOT_FOUND && directError != ERROR_PATH_NOT_FOUND
            && directError != ERROR_BAD_EXE_FORMAT) {
            return report("CreateProcessW", directError);
        }
        std::array<wchar_t, MAX_PATH + 1> system{};
        const UINT length = GetSystemDirectoryW(system.data(), static_cast<UINT>(system.size()));
        if (length == 0) { return report("GetSystemDirectoryW", GetLastError()); }
        if (length >= system.size()) { return report("system directory too long", ERROR_INSUFFICIENT_BUFFER); }
        const std::wstring interpreter = std::wstring(system.data()) + L"\\cmd.exe";
        const std::wstring fallback = L"\"" + interpreter + L"\" /d /s /c \"" + command + L"\"";
        if (fallback.size() >= 32767) { return report("command line too long", ERROR_BAD_LENGTH); }
        started = spawn(interpreter.c_str(), fallback, directory, startup, process);
        if (!started) { return report("CreateProcessW (cmd fallback)", GetLastError()); }
    }
    Handle processHandle(process.hProcess);
    Handle threadHandle(process.hThread);
    if (WaitForSingleObject(processHandle.get(), INFINITE) != WAIT_OBJECT_0) {
        return report("WaitForSingleObject", GetLastError());
    }
    DWORD exitCode = 0;
    if (GetExitCodeProcess(processHandle.get(), &exitCode) == FALSE) {
        return report("GetExitCodeProcess", GetLastError());
    }
    // Preserve normal exit codes; Windows exception statuses do not fit int.
    return exitCode <= static_cast<DWORD>((std::numeric_limits<int>::max)())
               ? static_cast<int>(exitCode) : 1;
}
} // namespace arcnova::core
