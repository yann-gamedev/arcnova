#include "core/terminal.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <stdexcept>
#include <string>

namespace arcnova::core {
namespace {

volatile LONG interrupted = 0;

BOOL WINAPI handleConsoleControl(DWORD event) noexcept {
    if (event == CTRL_C_EVENT) {
        InterlockedExchange(&interrupted, 1);
        return TRUE;
    }
    return FALSE;
}

void requireSuccess(BOOL success, const char* operation) {
    if (success == FALSE) {
        const DWORD error = GetLastError();
        throw std::runtime_error(std::string(operation) + " failed (Win32 error "
                                 + std::to_string(error) + ").");
    }
}

} // namespace

void initConsole() {
    static_assert(sizeof(void*) == 8, "ArcNova requires a Windows x64 build.");

    requireSuccess(SetConsoleOutputCP(CP_UTF8), "SetConsoleOutputCP");
    requireSuccess(SetConsoleCP(CP_UTF8), "SetConsoleCP");

    const HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    if (output == nullptr || output == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("Standard output handle is unavailable.");
    }

    DWORD mode = 0;
    // Redirected output is not a console and has no console mode to configure.
    if (GetFileType(output) == FILE_TYPE_CHAR) {
        requireSuccess(GetConsoleMode(output, &mode), "GetConsoleMode");
        requireSuccess(SetConsoleMode(output, mode | ENABLE_PROCESSED_OUTPUT
                                      | ENABLE_VIRTUAL_TERMINAL_PROCESSING),
                       "SetConsoleMode");
    }

    requireSuccess(SetConsoleTitleW(L"ArcNova Shell"), "SetConsoleTitleW");
    requireSuccess(SetConsoleCtrlHandler(handleConsoleControl, TRUE),
                   "SetConsoleCtrlHandler");

    // Ensure Ctrl+C generates a control event even if the parent disabled it.
    // Pipes and files do not have console input modes and are left untouched.
    const HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    DWORD inputMode = 0;
    if (GetConsoleMode(input, &inputMode) != FALSE) {
        requireSuccess(SetConsoleMode(input, inputMode | ENABLE_PROCESSED_INPUT),
                       "SetConsoleMode (input)");
    }

    // Clear the screen and position the cursor before rendering the welcome card.
    if (GetFileType(output) == FILE_TYPE_CHAR) {
        constexpr char clearScreen[] = "\x1b[2J\x1b[H";
        DWORD written = 0;
        requireSuccess(WriteFile(output, clearScreen,
                                 static_cast<DWORD>(sizeof(clearScreen) - 1),
                                 &written, nullptr), "WriteFile");
    }
}

bool consumeInterrupt() noexcept {
    return InterlockedExchange(&interrupted, 0) != 0;
}

} // namespace arcnova::core
