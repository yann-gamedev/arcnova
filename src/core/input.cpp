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
#include <utility>

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

bool startsWith(const std::wstring& text, const std::wstring& prefix) {
    return prefix.empty() || (text.size() >= prefix.size()
        && CompareStringOrdinal(text.data(), static_cast<int>(prefix.size()),
                                prefix.data(), static_cast<int>(prefix.size()), TRUE) == CSTR_EQUAL);
}

struct Completion {
    std::vector<std::wstring> candidates;
    std::size_t start = 0, length = 0, choice = 0;
};

Completion findCompletions(const std::wstring& buffer, std::size_t cursor,
                           const std::vector<std::string>& commands,
                           const std::vector<std::string>& aliases) {
    Completion result;
    // Locate the full token containing the cursor, respecting either quote style.
    std::size_t position = 0, tokenIndex = 0;
    while (position < cursor || position == cursor) {
        while (position < buffer.size() && (buffer[position] == L' ' || buffer[position] == L'\t')) {
            if (position == cursor) { break; }
            ++position;
        }
        const auto start = position;
        wchar_t quote = L'\0';
        while (position < buffer.size()) {
            const auto ch = buffer[position];
            if (quote != L'\0') { if (ch == quote) { quote = L'\0'; } }
            else if (ch == L'"' || ch == L'\'') { quote = ch; }
            else if (ch == L' ' || ch == L'\t') { break; }
            ++position;
        }
        if (cursor <= position) { result.start = start; result.length = position - start; break; }
        ++tokenIndex;
    }
    std::wstring prefix;
    for (auto index = result.start; index < cursor; ++index) {
        if (buffer[index] != L'"' && buffer[index] != L'\'') { prefix += buffer[index]; }
    }
    if (tokenIndex == 0) {
        for (const auto* names : {&commands, &aliases}) {
            for (const auto& name : *names) {
                const auto wide = decode(name);
                if (startsWith(wide, prefix)) { result.candidates.push_back(wide); }
            }
        }
        // An alias may shadow a built-in of the same name; offer it only once.
        std::sort(result.candidates.begin(), result.candidates.end());
        result.candidates.erase(std::unique(result.candidates.begin(), result.candidates.end()),
                                result.candidates.end());
    } else {
        try {
            const auto separator = prefix.find_last_of(L"/\\");
            const auto parent = separator == std::wstring::npos ? std::wstring{} : prefix.substr(0, separator + 1);
            const auto leaf = separator == std::wstring::npos ? prefix : prefix.substr(separator + 1);
            std::wstring directory = parent.empty() ? L"." : parent;
            const bool home = prefix == L"~" || directory == L"~" || directory.rfind(L"~/", 0) == 0
                              || directory.rfind(L"~\\", 0) == 0;
            std::wstring displayParent = parent;
            std::wstring match = leaf;
            if (home) {
                const DWORD size = GetEnvironmentVariableW(L"USERPROFILE", nullptr, 0);
                if (size == 0) { return result; }
                std::vector<wchar_t> value(size);
                const DWORD count = GetEnvironmentVariableW(L"USERPROFILE", value.data(), size);
                if (count == 0 || count >= size) { return result; }
                if (prefix == L"~") { displayParent = L"~/"; match.clear(); directory = value.data(); }
                else { directory = std::wstring(value.data()) + directory.substr(1); }
            }
            std::error_code ec;
            std::filesystem::directory_iterator it(std::filesystem::path(directory), ec), end;
            if (ec) { return result; }
            for (; it != end; it.increment(ec)) {
                if (ec) { break; }
                const auto name = it->path().filename().wstring();
                if (!startsWith(name, match)) { continue; }
                const bool isDirectory = it->is_directory(ec);
                if (ec) { ec.clear(); continue; }
                auto candidate = displayParent + name + (isDirectory ? L"/" : L"");
                // Quote apostrophes too, since the parser treats them as delimiters.
                if (candidate.find_first_of(L" \t'") != std::wstring::npos) {
                    candidate = L"\"" + candidate + L"\"";
                }
                result.candidates.push_back(std::move(candidate));
            }
        } catch (const std::filesystem::filesystem_error&) {
            result.candidates.clear(); // Failed completion must not disrupt the input line.
        }
    }
    std::sort(result.candidates.begin(), result.candidates.end());
    return result;
}
} // namespace

std::vector<std::string> InputHandler::commandNames;
std::vector<std::string> InputHandler::aliasNames;

void InputHandler::setCommandNames(std::vector<std::string> names) {
    commandNames = std::move(names);
}

void InputHandler::setAliasNames(std::vector<std::string> names) {
    aliasNames = std::move(names);
}

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
    Completion completion;
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
        if (key.wVirtualKeyCode != VK_TAB) { completion = {}; }
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
            case VK_TAB:
                if (completion.candidates.empty()) {
                    completion = findCompletions(buffer, cursor, commandNames, aliasNames);
                }
                if (!completion.candidates.empty()) {
                    const auto& candidate = completion.candidates[completion.choice];
                    if (buffer.size() - completion.length + candidate.size() <= 32766) {
                        buffer.replace(completion.start, completion.length, candidate);
                        completion.length = candidate.size();
                        cursor = completion.start + completion.length;
                    }
                    completion.choice = (completion.choice + 1) % completion.candidates.size();
                }
                break;
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
