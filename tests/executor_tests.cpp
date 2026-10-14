#include "core/executor.hpp"
#include <cassert>
#include <iostream>

int main() {
    std::error_code ec;
    const auto cwd = std::filesystem::current_path(ec);
    assert(!ec);
    using arcnova::core::executeExternalCommand;
    assert(executeExternalCommand("", cwd) == 0);
    assert(executeExternalCommand("cmd.exe /d /c exit 7", cwd) == 7);
    assert(executeExternalCommand("cmd.exe /d /c exit 0", cwd) == 0);
    assert(executeExternalCommand("ver", cwd) == 0); // cmd builtin fallback
    assert(executeExternalCommand("cmd.exe /d /c echo test | findstr test", cwd) == 0);
    assert(executeExternalCommand("arcnova_nonexistent_executable_4897", cwd) != 0);
    assert(executeExternalCommand(std::string("bad\0command", 11), cwd) == 2);
    std::cout << "PASS: native execution, fallback, pipeline, exit status, invalid input\n";
}
