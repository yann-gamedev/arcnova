// g++ -std=c++17 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror -Iinclude tests/git_prompt_tests.cpp -o git_prompt_tests.exe
#include "ui/git_prompt.hpp"

#include <iostream>

namespace {

struct TemporaryDirectory {
    std::filesystem::path path;
    ~TemporaryDirectory() {
        std::error_code ec;
        if (!path.empty()) { std::filesystem::remove_all(path, ec); }
    }
};

} // namespace

int main() {
    namespace fs = std::filesystem;
    std::error_code ec;
    const auto temp = fs::temp_directory_path(ec);
    if (ec) {
        std::cerr << "Cannot find temporary directory: " << ec.message() << '\n';
        return 1;
    }
    TemporaryDirectory fixture;
    const auto candidate = temp / ("arcnova-git-prompt-" + std::to_string(GetCurrentProcessId())
                                  + "-" + std::to_string(GetTickCount64()));
    if (!fs::create_directory(candidate, ec) || ec) { return 1; }
    fixture.path = candidate;
    const auto repo = fixture.path / "repo with spaces";
    fs::create_directories(repo / ".git", ec);
    if (ec) { return 1; }
    fs::create_directories(repo / "src/nested", ec);
    if (ec) { return 1; }

    int failures = 0;
    const auto writeHead = [&](const std::string& text) {
        std::ofstream head(repo / ".git/HEAD", std::ios::binary);
        head << text;
        head.close();
        if (!head) { ++failures; }
    };
    const auto expect = [&](const fs::path& path, const std::string& expected) {
        const auto actual = arcnova::ui::getGitPromptInfo(path);
        if (actual != expected) {
            std::cerr << "FAIL " << path << ": expected '" << expected
                      << "', got '" << actual << "'\n";
            ++failures;
        }
    };

    // HEAD-only fixtures are not valid repositories for git status: errors must be silent.
    writeHead("ref: refs/heads/feature/v1.4\n");
    expect(repo, "feature/v1.4");
    expect(repo / "src/nested", "feature/v1.4");
    writeHead("ref: refs/heads/feature/v1.4\r\n");
    expect(repo / "src", "feature/v1.4");
    writeHead("a1b2c3d45678901234567890123456789012345678\n");
    expect(repo, "a1b2c3d");
    writeHead("not a valid HEAD\n");
    expect(repo, "");
    writeHead("ref: refs/heads/\n");
    expect(repo, "");
    expect({}, "");
    expect(fixture.path, "");

    std::cout << (failures == 0 ? "PASS" : "FAIL") << ": Git prompt unit tests\n";
    return failures == 0 ? 0 : 1;
}
