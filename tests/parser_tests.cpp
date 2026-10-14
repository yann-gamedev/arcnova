#include "core/parser.hpp"

#include <cassert>
#include <cstdlib>
#include <iostream>

int main() {
    using arcnova::core::parseInput;
    const std::filesystem::path cwd = "C:/work";
    assert(parseInput(" \t\r\n", cwd).name.empty());
    const auto quoted = parseInput("mkdir \"Folder Baru\" 'Folder Lain'", cwd);
    assert(quoted.name == "mkdir");
    assert((quoted.args == std::vector<std::string>{"Folder Baru", "Folder Lain"}));
    const auto flags = parseInput("ls -l -a -rf file", cwd);
    assert(flags.flags == "larf");
    assert(flags.args.at(0) == "file");
    assert(flags.rawInput == "ls -l -a -rf file");
    const auto mixed = parseInput("echo pre\"two words\"post '' \"\" C:\\Users\\User", cwd);
    assert((mixed.args == std::vector<std::string>{"pretwo wordspost", "", "", "C:\\Users\\User"}));
    const auto dash = parseInput("echo - -- -la", cwd);
    assert(dash.flags.empty());
    assert((dash.args == std::vector<std::string>{"-", "-la"}));
    assert(parseInput("echo ~someone", cwd).args.at(0) == "~someone");
    for (const auto* input : {"echo 'unfinished", "echo \"unfinished", "''"}) {
        bool rejected = false;
        try {
            (void)parseInput(input, cwd);
        } catch (const arcnova::core::ParseError&) {
            rejected = true;
        }
        assert(rejected);
    }
    // Test with a controlled environment: USERPROFILE=C:\Users\Parser Test.
    const char* home = std::getenv("USERPROFILE");
    assert(home != nullptr);
    const auto expanded = parseInput("cd ~/Documents", cwd);
    const auto expected = std::filesystem::path(home).generic_u8string() + "/Documents";
    assert(expanded.args.at(0) == expected);
    assert(parseInput("cd '~'", cwd).args.at(0) == std::filesystem::path(home).generic_u8string());
    std::cout << "PASS: parser tests\n";
}
