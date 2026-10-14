#pragma once

#include "core/parser.hpp"
#include <filesystem>

namespace arcnova::cmd {
int pwd(const core::ParsedCommand& command);
int cd(const core::ParsedCommand& command, std::filesystem::path& previousPath);
int ls(const core::ParsedCommand& command);
int touch(const core::ParsedCommand& command);
int mkdir(const core::ParsedCommand& command);
int rm(const core::ParsedCommand& command);
int cp(const core::ParsedCommand& command);
int mv(const core::ParsedCommand& command);
int cat(const core::ParsedCommand& command);
} // namespace arcnova::cmd
