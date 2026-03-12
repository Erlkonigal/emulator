#pragma once

namespace emulator {

enum class DebuggerState {
    Idle,
    Running,
    Paused,
    Halted
};

} // namespace emulator