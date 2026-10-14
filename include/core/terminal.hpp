#pragma once

namespace arcnova::core {

// Throws std::runtime_error if console initialization fails.
void initConsole();

// Consume the Ctrl+C notification on the main thread, never in the handler.
[[nodiscard]] bool consumeInterrupt() noexcept;

} // namespace arcnova::core
