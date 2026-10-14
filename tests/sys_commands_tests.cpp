#include "commands/sys_commands.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

int main(int argc, char** argv) {
    assert(argc == 2); // Caller supplies a disposable test file path.
    const auto file = std::filesystem::u8path(argv[1]);
    { std::ofstream output(file); output << "Alpha alpha\nbeta\nALPHA\n"; assert(output.good()); }
    std::ostringstream output;
    auto* original = std::cout.rdbuf(output.rdbuf());
    using arcnova::core::ParsedCommand;
    assert(arcnova::cmd::grep(ParsedCommand{"grep", {"alpha", argv[1]}, "in", ""}) == 0);
    assert(output.str().find("1:") != std::string::npos);
    assert(output.str().find("3:") != std::string::npos);
    assert(output.str().find("\x1b[") != std::string::npos);
    assert(output.str().find("beta") == std::string::npos);
    assert(arcnova::cmd::grep(ParsedCommand{"grep", {"missing", argv[1]}, "", ""}) == 1);
    assert(arcnova::cmd::grep(ParsedCommand{"grep", {"", argv[1]}, "", ""}) == 0);
    output.str("");
    assert(arcnova::cmd::echo(ParsedCommand{"echo", {"hello", "world"}, "", ""}) == 0);
    assert(output.str() == "hello world\n");
    assert(arcnova::cmd::whoami(ParsedCommand{"whoami", {}, "", ""}) == 0);
    assert(arcnova::cmd::arcfetch(ParsedCommand{"arcfetch", {}, "", ""}) == 0);
    for (const auto* label : {"OS:", "Hostname:", "User:", "CPU:", "RAM:", "Uptime:"}) {
        assert(output.str().find(label) != std::string::npos);
    }
    std::cout.rdbuf(original);
    std::error_code ec;
    std::filesystem::remove(file, ec);
    assert(!ec);
    std::cout << "PASS: grep matches/status/highlights, echo, whoami, arcfetch fields\n";
}
