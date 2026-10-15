#include "core/input.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <limits>

namespace arcnova::core {
namespace {
constexpr std::size_t historyLimit = 1000;

class InputMode {
public:
    InputMode(HANDLE input, DWORD mode) : handle(input), original(mode) {}
    ~InputMode() { SetConsoleMode(handle, original); }
    InputMode(const InputMode&) = delete;
    InputMode& operator=(const InputMode&) = delete;
private:
    HANDLE handle;
    DWORD original;
};

std::wstring decode(const std::string& text) {
    if (text.empty() || text.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())) { return {}; }
    const int length = static_cast<int>(text.size());
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), length, nullptr, 0);
    if (count == 0) { return {}; }
    std::wstring result(static_cast<std::size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), length, result.data(), count);
    return result;
}

std::string encode(const std::wstring& text) {
    if (text.empty()) { return {}; }
    const int length = static_cast<int>(text.size());
    const int count = WideCharToMultiByte(CP_UTF8, 0, text.data(), length, nullptr, 0, nullptr, nullptr);
    if (count == 0) { return {}; }
    std::string result(static_cast<std::size_t>(count), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), length, result.data(), count, nullptr, nullptr);
    return result;
}

bool low(wchar_t ch) { return ch >= 0xDC00 && ch <= 0xDFFF; }
bool high(wchar_t ch) { return ch >= 0xD800 && ch <= 0xDBFF; }
std::size_t previous(const std::wstring& text, std::size_t cursor) {
    if (cursor == 0) { return 0; }
    --cursor;
    if (cursor > 0 && low(text[cursor]) && high(text[cursor - 1])) { --cursor; }
    return cursor;
}
std::size_t next(const std::wstring& text, std::size_t cursor) {
    if (cursor >= text.size()) { return text.size(); }
    return cursor + (high(text[cursor]) && cursor + 1 < text.size() && low(text[cursor + 1]) ? 2U : 1U);
}

void redraw(const std::string& prompt, const std::wstring& buffer, std::size_t cursor) {
    std::cout << "\r\033[K" << prompt << encode(buffer);
    // Move left once per Unicode scalar (surrogate pairs count as one).
    std::size_t cells = 0;
    for (auto index = cursor; index < buffer.size(); index = next(buffer, index)) { ++cells; }
    if (cells != 0) { std::cout << "\033[" << cells << 'D'; }
    std::cout << std::flush;
}
} // namespace

InputHandler::InputHandler() {
    const DWORD count = GetEnvironmentVariableW(L"USERPROFILE", nullptr, 0);
    if (count != 0) {
        std::vector<wchar_t> home(count);
        const DWORD length = GetEnvironmentVariableW(L"USERPROFILE", home.data(), count);
        if (length != 0 && length < count) { historyPath = std::filesystem::path(home.data()) / ".arcnova_history"; }
    }
    loadHistory();
}

void InputHandler::loadHistory() {
    history.clear();
    if (historyPath.empty()) { return; }
    std::error_code ec;
    const bool exists = std::filesystem::exists(historyPath, ec);
    if (ec) { std::cerr << "ArcNova: history: " << ec.message() << '\n'; return; }
    if (!exists) { return; }
    std::ifstream file(historyPath, std::ios::binary);
    if (!file) { std::cerr << "ArcNova: cannot open history file\n"; return; }
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') { line.pop_back(); }
        if (!line.empty() && !decode(line).empty() && (history.empty() || history.back() != line)) {
            history.push_back(line);
            if (history.size() > historyLimit) { history.erase(history.begin()); }
        }
    }
    if (file.bad()) { std::cerr << "ArcNova: failed to read history file\n"; }
}

void InputHandler::saveHistory() const {
    if (historyPath.empty()) { return; }
    std::ofstream file(historyPath, std::ios::binary | std::ios::trunc);
    if (!file) { std::cerr << "ArcNova: cannot save history file\n"; return; }
    for (const auto& line : history) { file << line << '\n'; }
    file.close();
    if (!file) { std::cerr << "ArcNova: failed to write history file\n"; }
}

void InputHandler::remember(const std::string& line) {
    if (line.find_first_not_of(" \t\r\n") == std::string::npos || (!history.empty() && history.back() == line)) { return; }
    history.push_back(line);
    if (history.size() > historyLimit) { history.erase(history.begin()); }
    saveHistory();
}

bool InputHandler::readLine(const std::string& prompt, std::string& line) {
    line.clear();
    interrupted = false;
    const HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(input, &mode) == FALSE) {
        std::cout << prompt << std::flush;
        if (!std::getline(std::cin, line)) { return false; }
        if (line.compare(0, 3, "\xEF\xBB\xBF") == 0) { line.erase(0, 3); }
        // Scripted input must not pollute interactive history.
        return true;
    }
    InputMode restore(input, mode);
    const DWORD rawMode = (mode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT
                                   | ENABLE_VIRTUAL_TERMINAL_INPUT | ENABLE_QUICK_EDIT_MODE)) | ENABLE_EXTENDED_FLAGS;
    if (SetConsoleMode(input, rawMode) == FALSE) {
        std::cerr << "ArcNova: cannot enable raw console input\n";
        return false;
    }
    std::wstring buffer, draft;
    std::size_t cursor = 0, historyIndex = history.size();
    std::cout << prompt << std::flush;
    for (;;) {
        INPUT_RECORD record{};
        DWORD read = 0;
        if (ReadConsoleInputW(input, &record, 1, &read) == FALSE || read == 0) {
            std::cerr << "ArcNova: failed to read console input\n";
            return false;
        }
        if (record.EventType != KEY_EVENT || record.Event.KeyEvent.bKeyDown == FALSE) { continue; }
        const auto& key = record.Event.KeyEvent;
        const bool control = (key.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;
        const bool alt = (key.dwControlKeyState & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) != 0;
        if (control && key.wVirtualKeyCode == 'C') {
            interrupted = true;
            std::cout << "^C\n" << std::flush;
            return true;
        }
        if (control && key.wVirtualKeyCode == 'L') {
            std::cout << "\033[2J\033[H";
            redraw(prompt, buffer, cursor);
            continue;
        }
        for (WORD repeat = 0; repeat < key.wRepeatCount; ++repeat) {
            switch (key.wVirtualKeyCode) {
            case VK_RETURN:
                line = encode(buffer);
                std::cout << '\n' << std::flush;
                remember(line);
                return true;
            case VK_LEFT: cursor = previous(buffer, cursor); break;
            case VK_RIGHT: cursor = next(buffer, cursor); break;
            case VK_BACK:
                if (cursor > 0) {
                    const auto start = previous(buffer, cursor);
                    buffer.erase(start, cursor - start); cursor = start;
                }
                break;
            case VK_DELETE:
                if (cursor < buffer.size()) { buffer.erase(cursor, next(buffer, cursor) - cursor); }
                break;
            case VK_UP:
                if (historyIndex > 0) {
                    if (historyIndex == history.size()) { draft = buffer; }
                    buffer = decode(history[--historyIndex]); cursor = buffer.size();
                }
                break;
            case VK_DOWN:
                if (historyIndex < history.size()) {
                    ++historyIndex;
                    buffer = historyIndex == history.size() ? draft : decode(history[historyIndex]);
                    cursor = buffer.size();
                }
                break;
            default:
                // AltGr can produce printable Unicode with both Ctrl and Alt set.
                if ((!control || alt) && key.uChar.UnicodeChar >= L' ' && key.uChar.UnicodeChar != 0x7F
                    && buffer.size() < 32766) {
                    buffer.insert(cursor++, 1, key.uChar.UnicodeChar);
                }
                break;
            }
        }
        redraw(prompt, buffer, cursor);
    }
}
} // namespace arcnova::core
