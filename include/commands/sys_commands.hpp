#pragma once

#include "core/config.hpp"
#include "core/parser.hpp"

namespace arcnova::cmd {
int grep(const core::ParsedCommand& command);
int echo(const core::ParsedCommand& command);
int whoami(const core::ParsedCommand& command);
int arcfetch(const core::ParsedCommand& command);
// alias | alias <name> | alias <name>="<command>" | alias <name> <command...>
int alias(const core::ParsedCommand& command, core::ConfigManager& config);
// unalias <name...> | unalias -a
int unalias(const core::ParsedCommand& command, core::ConfigManager& config);
} // namespace arcnova::cmd
