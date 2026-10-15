#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace arcnova::core {

class ConfigManager {
public:
    // Reads %USERPROFILE%/.arcnovarc; creates a commented default file on first run.
    // Errors are reported to std::cerr and never abort startup.
    void loadRcFile();

    // Replaces the first unquoted word of every `;`, `&&` and `|` segment with its
    // alias value. Single pass (no recursion), so `alias ls="ls -a"` cannot loop.
    // Quoting a command name (e.g. "ls") bypasses alias expansion.
    [[nodiscard]] std::string expandAlias(const std::string& input) const;

    // Accepts `name="command"`, `name='command'`, `name=command` or `name command`.
    bool defineAlias(const std::string& definition, std::string& error);
    bool removeAlias(const std::string& name);
    void clearAliases() noexcept;

    [[nodiscard]] const std::unordered_map<std::string, std::string>& getAliases() const noexcept;
    [[nodiscard]] std::vector<std::string> aliasNames() const; // Sorted.

private:
    std::unordered_map<std::string, std::string> aliases;
};

} // namespace arcnova::core
