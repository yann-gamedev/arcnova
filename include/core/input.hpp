#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace arcnova::core {

class InputHandler {
public:
    InputHandler();
    static void setCommandNames(std::vector<std::string> names);
    // Returns false on EOF/read failure. Ctrl+C cancels the line and returns "".
    bool readLine(const std::string& prompt, std::string& line);
    [[nodiscard]] bool wasInterrupted() const noexcept { return interrupted; }
    void loadHistory();
    void saveHistory() const;

private:
    static std::vector<std::string> commandNames;
    bool interrupted = false;
    std::vector<std::string> history;
    std::filesystem::path historyPath;
    void remember(const std::string& line);
};

} // namespace arcnova::core
