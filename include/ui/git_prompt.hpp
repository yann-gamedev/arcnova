#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

namespace arcnova::ui {

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
            if (head.compare(0, prefix.size(), prefix) == 0) {
                return head.substr(prefix.size());
            }
            if (head.size() >= 7
                && head.find_first_not_of("0123456789abcdefABCDEF") == std::string::npos) {
                return head.substr(0, 7);
            }
            return {}; // Missing or malformed HEAD must not fall back to an outer repository.
        }

        const auto parent = directory.parent_path();
        if (parent == directory) { break; }
        directory = parent;
    }
    return {};
}

} // namespace arcnova::ui
