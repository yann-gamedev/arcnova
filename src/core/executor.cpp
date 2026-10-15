#include "core/executor.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
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

// Buffered stages use temporary, delete-on-close files for child stdin/stdout.
// Unlike synchronous anonymous pipes, these cannot deadlock on large output.
bool createSpool(Handle& file) {
    std::array<wchar_t, MAX_PATH + 1> directory{};
    const DWORD length = GetTempPathW(static_cast<DWORD>(directory.size()), directory.data());
    if (length == 0) { return false; }
    if (length >= directory.size()) {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return false;
    }
    std::array<wchar_t, MAX_PATH + 1> name{};
    if (GetTempFileNameW(directory.data(), L"arc", 0, name.data()) == 0) { return false; }
    SECURITY_ATTRIBUTES security{};
    security.nLength = sizeof(security);
    security.bInheritHandle = TRUE;
    *file.address() = CreateFileW(name.data(), GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, &security,
        OPEN_EXISTING, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    if (file.get() == INVALID_HANDLE_VALUE) {
        const DWORD code = GetLastError();
        DeleteFileW(name.data());
        SetLastError(code);
        return false;
    }
    return true;
}

bool rewindSpool(HANDLE file) {
    LARGE_INTEGER offset{};
    return SetFilePointerEx(file, offset, nullptr, FILE_BEGIN) != FALSE;
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

int executeExternalCommand(const std::string& rawInput, const std::filesystem::path& currentPath,
                           const std::string& redirectOutFile, bool appendMode,
                           const std::string* pipedInput, bool captureOutput) {
    const auto begin = rawInput.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) { return 0; }
    const auto end = rawInput.find_last_not_of(" \t\r\n");
    std::wstring command;
    if (!toWide(rawInput.substr(begin, end - begin + 1), command)) {
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
    Handle inputSpool;
    Handle outputSpool;
    if (pipedInput != nullptr) {
        if (!createSpool(inputSpool)) { return report("create input spool", GetLastError()); }
        std::size_t offset = 0;
        while (offset < pipedInput->size()) {
            const auto remaining = pipedInput->size() - offset;
            const DWORD chunk = static_cast<DWORD>((std::min)(remaining, std::size_t{65536}));
            DWORD written = 0;
            if (WriteFile(inputSpool.get(), pipedInput->data() + offset, chunk, &written, nullptr) == FALSE) {
                return report("write input spool", GetLastError());
            }
            if (written == 0) { return report("write input spool", ERROR_WRITE_FAULT); }
            offset += written;
        }
        if (!rewindSpool(inputSpool.get())) { return report("rewind input spool", GetLastError()); }
    }
    const bool capture = captureOutput && redirectOutFile.empty();
    if (capture && !createSpool(outputSpool)) {
        return report("create output spool", GetLastError());
    }
    Handle outputFile;
    if (!redirectOutFile.empty()) {
        const auto path = currentPath / std::filesystem::u8path(redirectOutFile);
        SECURITY_ATTRIBUTES security{};
        security.nLength = sizeof(security);
        security.bInheritHandle = TRUE;
        *outputFile.address() = CreateFileW(path.c_str(),
            appendMode ? FILE_APPEND_DATA : GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, &security,
            appendMode ? OPEN_ALWAYS : CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (outputFile.get() == INVALID_HANDLE_VALUE) {
            return report("open output file", GetLastError());
        }
    }
    startup.hStdInput = pipedInput != nullptr ? inputSpool.get() : inherited[0].get();
    startup.hStdOutput = !redirectOutFile.empty() ? outputFile.get()
                        : (capture ? outputSpool.get() : inherited[1].get());
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
    if (capture) {
        if (!rewindSpool(outputSpool.get())) { return report("rewind output spool", GetLastError()); }
        std::array<char, 65536> buffer{};
        for (;;) {
            DWORD count = 0;
            if (ReadFile(outputSpool.get(), buffer.data(), static_cast<DWORD>(buffer.size()),
                         &count, nullptr) == FALSE) {
                return report("read output spool", GetLastError());
            }
            if (count == 0) { break; }
            std::cout.write(buffer.data(), static_cast<std::streamsize>(count));
            if (!std::cout) {
                std::cerr << "ArcNova: cannot capture process output\n";
                return 1;
            }
        }
    }
    // Preserve normal exit codes; Windows exception statuses do not fit int.
    return exitCode <= static_cast<DWORD>((std::numeric_limits<int>::max)())
               ? static_cast<int>(exitCode) : 1;
}
} // namespace arcnova::core
