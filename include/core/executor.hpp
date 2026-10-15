#pragma once

#include <filesystem>
#include <string>

namespace arcnova::core {
// Executes the original command line synchronously. Native executables are tried
// first; unresolved/batch commands are delegated to the system cmd.exe.
int executeExternalCommand(const std::string& rawInput,
                           const std::filesystem::path& currentPath,
                           const std::string& redirectOutFile = {},
                           bool appendMode = false,
                           const std::string* pipedInput = nullptr,
                           bool captureOutput = false);
} // namespace arcnova::core
