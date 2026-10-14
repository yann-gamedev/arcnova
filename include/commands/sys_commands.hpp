#pragma once

#include "core/parser.hpp"

namespace arcnova::cmd {
int grep(const core::ParsedCommand& command);
int echo(const core::ParsedCommand& command);
int whoami(const core::ParsedCommand& command);
int arcfetch(const core::ParsedCommand& command);
} // namespace arcnova::cmd
