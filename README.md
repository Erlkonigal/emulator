# Emulator

A modular hardware emulator framework written in C++20. This project provides a flexible architecture for emulating SoC (System-on-Chip) systems with support for pluggable devices, interactive debugging, and configurable memory mappings.

## Overview

Emulator implements a clean separation between CPU execution, memory bus arbitration, and peripheral devices. The framework is designed to be architecture-agnostic, with the CPU implementation provided externally through a well-defined interface. This makes it suitable for emulating various processor architectures by implementing the `ICpuExecutor` interface.

## Architecture

### Directory Structure

```
emulator/
├── include/emulator/          # Public API headers
│   ├── app/                  # Application layer (configuration, terminal, CLI)
│   ├── bus/                  # Memory bus management
│   ├── cpu/                  # CPU interface definitions
│   ├── debugger/             # Debugger implementation
│   ├── device/               # Device drivers
│   └── logging/              # Logging infrastructure
├── src/                       # Implementation files
├── test/                      # Test suite and utilities
└── CMakeLists.txt            # Build configuration
```

### Core Components

**Memory Bus** (`include/emulator/bus/bus.h`)
- Manages device registration and memory region mapping
- Provides address translation and device lookup
- Supports overlapping region validation
- Synchronizes devices based on CPU cycle count

**CPU Interface** (`include/emulator/cpu/cpu.h`)
- `ICpuExecutor`: CPU execution interface (reset, step, register access)
- `ICpuDebugger`: Debugger callback interface (breakpoints, tracing)
- `TraceRecord`: Instruction trace and memory event recording
- `MemAccess`: Unified memory access request/response structure

**Device Model** (`include/emulator/device/device.h`)
- Base class for all emulated peripherals
- Memory-mapped I/O with read/write handlers
- Periodic tick for time-based device updates

## SoC Memory Map

| Device  | Base Address   | Size    | Description                |
|---------|----------------|---------|----------------------------|
| ROM     | 0x00000000     | Variable| Boot ROM (read-only)       |
| UART    | 0x20000000     | 0x100   | Serial console             |
| TIMER   | 0x20001000     | 0x100   | Cycle counter              |
| SDL     | 0x30000000     | Variable| Framebuffer and display    |
| RAM     | 0x80000000     | Variable| Main system RAM            |

## Device Specifications

### UART Device

Memory-mapped registers at `0x20000000`:

| Offset | Access | Description                        |
|--------|--------|------------------------------------|
| 0x0    | R/W    | Data register (8-bit)              |
| 0x4    | R      | Status register                    |

Status register bits:
- Bit 0: RX Ready (receive buffer has data)
- Bit 1: TX Ready (transmit buffer ready)

### Timer Device

Memory-mapped registers at `0x20001000`:

| Offset | Access | Description                        |
|--------|--------|------------------------------------|
| 0x0    | R      | Counter low 32 bits                |
| 0x4    | R      | Counter high 32 bits               |
| 0x8    | W      | Control (write to reset counter)   |

The timer increments by one for each CPU cycle. Reading the counter provides elapsed time in microseconds.

### SDL Display Device

Control region at `0x30000000` (4KB reserved), framebuffer at `0x30001000`:

| Offset  | Access | Description                        |
|---------|--------|------------------------------------|
| 0x00    | W      | Control (bit 0: present frame)     |
| 0x04    | R      | Screen width in pixels             |
| 0x08    | R      | Screen height in pixels            |
| 0x0c    | R      | Pitch (bytes per line)             |
| 0x10    | R      | Status (bit 0: ready, bit 1: dirty)|
| 0x20    | R      | Keyboard data                      |
| 0x24    | R      | Keyboard status (bit 0: ready)     |
| 0x28    | R      | Last pressed key                   |

Framebuffer format: 32-bit ARGB per pixel, row-major order.

## Building

### Prerequisites

- CMake 3.16 or later
- C++20 compatible compiler (GCC 10+, Clang 12+)
- ncurses development libraries
- SDL2 development libraries
- libvterm development libraries

### Build Configuration

The project supports three build presets:

```bash
# Debug build (unoptimized with debug symbols)
cmake --preset debug
cmake --build build/debug

# Release build (optimized)
cmake --preset release
cmake --build build/release

# Profile build (maximum optimization with profiling)
cmake --preset profile
cmake --build build/profile
```

### Build Options

Standard CMake options can be used:

```bash
cmake -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release
```

## Usage

### Basic Invocation

```bash
./emulator --rom <path-to-rom-binary> [options]
```

### Command-Line Options

| Option              | Default              | Description                          |
|---------------------|----------------------|--------------------------------------|
| `--rom <path>`      | (required)           | ROM image path                       |
| `--config <file>`   | `emulator.conf`      | Configuration file path              |
| `--debug`           | false                | Start in interactive debugger mode   |
| `--width <pixels>`  | 640                  | SDL window width                     |
| `--height <pixels>` | 480                  | SDL window height                    |
| `--sdl-base <addr>` | 0x30000000           | SDL base address                     |
| `--ram-base <addr>` | 0x80000000           | RAM base address                     |
| `--ram-size <bytes>`| 0x10000000           | RAM size                             |
| `--uart-base <addr>`| 0x20000000           | UART base address                    |
| `--timer-base <addr>`| 0x20001000          | Timer base address                   |
| `--title <string>`  | `Emulator`           | Window title                         |
| `--headless`        | false                | Run without SDL window               |
| `--itrace`          | false                | Enable instruction tracing           |
| `--mtrace`          | false                | Enable memory access tracing         |
| `--bptrace`         | false                | Enable branch prediction tracing     |
| `--log-level <lvl>` | `info`               | Log level (trace/debug/info/warn/error)|
| `--log-filename <path>`| (none)            | Log output file prefix (creates .out and .err files) |

### Configuration File

The emulator can be configured via a configuration file (default: `emulator.conf`):

```ini
# Example emulator.conf
rom = firmware.bin
width = 800
height = 600
ram-size = 0x8000000
cpu-frequency = 1000000
debug = false
```

## Interactive Debugger

When started with `--debug`, the emulator enters interactive mode with a curses-based terminal interface. The debugger provides real-time status display and command input.

### Debugger Commands

| Command       | Description                                  |
|---------------|----------------------------------------------|
| `run`         | Resume CPU execution                         |
| `step [N]`    | Execute N instructions (default: 1)          |
| `pause`       | Pause CPU execution                          |
| `quit` / `exit` | Exit the emulator                          |
| `regs`        | Display all register values                  |
| `mem <addr> <len>` | Dump memory content                       |
| `eval <expr>` | Evaluate an expression                       |
| `bp list`     | List all breakpoints                         |
| `bp add <addr>`| Add a breakpoint                            |
| `bp del <addr>`| Remove a breakpoint                         |
| `log <level>` | Set log level (trace/debug/info/warn/error)  |
| `help`        | Show available commands                      |

### Expression Syntax

The `eval` command supports C-style expressions with:
- Integer constants (decimal and hexadecimal: `0xABCD`)
- Arithmetic operators: `+`, `-`, `*`, `/`, `%`
- Bitwise operators: `&`, `|`, `^`, `<<`, `>>`
- Parentheses for grouping
- Register references: `$reg` (e.g., `$pc`, `$sp`)

## Implementing a Custom CPU

To emulate a different processor architecture, implement the `ICpuExecutor` interface and provide a `CreateCpuExecutor()` factory function:

```cpp
#include "emulator/cpu/cpu.h"

class CustomCpuExecutor : public ICpuExecutor {
public:
    void reset() override;
    StepResult step(uint64_t maxInstructions, uint64_t maxCycles) override;
    CpuErrorDetail getLastError() const override;
    uint64_t getPc() const override;
    void setPc(uint64_t pc) override;
    uint64_t getCycle() const override;
    uint64_t getRegister(uint32_t regId) const override;
    void setRegister(uint32_t regId, uint64_t value) override;
    void setDebugger(ICpuDebugger* debugger) override;
    uint32_t getRegisterCount() const override;
};

extern "C" ICpuExecutor* CreateCpuExecutor() {
    return new CustomCpuExecutor();
}
```

Link the custom CPU implementation with the emulator framework to create a complete system emulator.

## Testing

The project includes a comprehensive test suite:

```bash
# Run all tests
cmake --build build/release --target all
ctest --test-dir build/release

# Run specific test categories
ctest --test-dir build/release -L device
ctest --test-dir build/release -L integration
```

### Test Components

- **device_tests.cc**: Device driver validation
- **integration_tests.cc**: System integration tests
- **trace_tests.cc**: Instruction tracing verification
- **display_demo.cc**: SDL display demonstration
- **toy_cpu_executor.cc**: Example CPU implementation for testing

## Logging System

The emulator implements a hierarchical logging system with the following levels:

| Level   | Description                                      |
|---------|--------------------------------------------------|
| Trace   | Detailed execution trace (instruction, memory)   |
| Debug   | Debugging information                            |
| Info    | General operational information                  |
| Warn    | Non-fatal warning conditions                    |
| Error   | Error conditions (non-fatal)                     |

### Output Configuration

By default, logs are written to stdout (device output) and stderr (other logs). When `--log-filename <path>` is specified, the emulator writes:

- `<path>.out`: Device output (UART, etc.)
- `<path>.err`: Other log messages

This works in both normal and debug modes. In debug mode, logs are displayed in the ncurses interface **and** saved to files simultaneously.

### Usage Examples

```bash
# Screen output only (default)
./emulator --rom firmware.bin

# Screen output + file logging
./emulator --rom firmware.bin --log-filename logs/emulator

# Debug mode with file logging (ncurses interface + files)
./emulator --rom firmware.bin --debug --log-filename logs/emulator
```

## Coding Standards

The project follows C++20 best practices:

- **Naming**: PascalCase for types, k-prefixed constants, m_-prefixed members
- **Style**: K&R braces, 4-space indentation, 100-character line limit
- **Modern C++**: `std::format`, `constexpr`, `[[nodiscard]]`, `[[maybe_unused]]`
- **Thread Safety**: Mutex-protected shared state, atomic operations

See `CODING_STYLE.md` for detailed guidelines.

## License

This project is provided as-is for educational and development purposes.
