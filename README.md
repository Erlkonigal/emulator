# Emulator Framework

A modular emulator framework with a pluggable CPU architecture.

## Dependencies

- CMake 3.16+
- C++20 compiler (GCC 11+ or Clang 14+)
- libsdl2-image-dev
- libsdl2-ttf-dev
- libvterm
- libncurses

## Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

## Architecture

```
┌─────────────────────────────────────────┐
│              Application                │
│    (CLI, Terminal, VTerm, Logging)      │
├─────────────────────────────────────────┤
│               Debugger                  │
│   (Expression Parser, Breakpoints)      │
├──────────────────┬──────────────────────┤
│    MemoryBus     │       Devices         │
│  - Device regs   │  - MemoryDevice      │
│  - Addr mapping  │  - UartDevice        │
│  - R/W routing   │  - TimerDevice       │
└──────────────────┴──────────────────────┘
                   ↑
                   │ ICpuExecutor
                   ▼
         ┌─────────────────────┐
         │  CPU (user-provided) │
         └─────────────────────┘
```

## Naming Conventions

| Type | Convention | Example |
|------|------------|---------|
| Classes/Structs | PascalCase | `MemoryBus`, `Device` |
| Methods/Functions | camelCase | `registerDevice()`, `read()` |
| Member Variables | m_ + camelCase | `mCpu`, `mBus` |
| Constants | k + PascalCase | `kDefaultRomBase` |
| Local Variables | camelCase | `mapping`, `devices` |

## Devices

### CPU Executor Interface

Implement `ICpuExecutor` and export:
```cpp
extern "C" ICpuExecutor* CreateCpuExecutor();
```

### MemoryBus

- `registerDevice(Device* device, uint64_t base, uint64_t size, const std::string& name)`
- `findMapping(uint64_t address) const`
- `read(const MemAccess& access)`
- `write(const MemAccess& access)`
- `syncAll(uint64_t currentCycle)`
- `setDebugger(Debugger* debugger)`

### Available Devices

| Device | Base Address | Size |
|--------|--------------|------|
| Memory | configurable | configurable |
| UART | 0x20000000 | 0x100 |
| Timer | 0x20001000 | 0x100 |
| SDL Display | 0x30000000 | varies |

## CLI Usage

### Required
- `--rom <path>` : ROM file path

### Optional
- `--config <file>` (default: `emulator.conf`)
- `--debug` : Enable debugger
- `--width <pixels>` (default: 640)
- `--height <pixels>` (default: 480)
- `--sdl-base <addr>` (default: 0x30000000)
- `--ram-base <addr>` (default: 0x80000000)
- `--ram-size <bytes>` (default: 268435456)
- `--uart-base <addr>` (default: 0x20000000)
- `--timer-base <addr>` (default: 0x20001000)
- `--title <string>` (default: Emulator)

### Config File

Format: `key=value` per line, `#` or `;` comments, hex values supported.

Supported keys: `rom`, `debug`, `width`, `height`, `ram_base`, `ram_size`,
`uart_base`, `timer_base`, `sdl_base`, `title`.

## Memory Map

| Region | Base | Size |
|--------|------|------|
| ROM | 0x00000000 | ROM file size |
| RAM | 0x80000000 | 256 MiB default |
| UART | 0x20000000 | 0x100 |
| Timer | 0x20001000 | 0x100 |
| SDL Display | 0x30000000 | varies |

## Device Registers

### SDL Display (0x30000000)

| Offset | Register | Access | Description |
|--------|----------|--------|-------------|
| 0x00 | CTRL | W | Write 1 to present framebuffer |
| 0x04 | WIDTH | R | Display width |
| 0x08 | HEIGHT | R | Display height |
| 0x0C | PITCH | R | Bytes per row |
| 0x10 | STATUS | R | Bit0: ready, Bit1: dirty |
| 0x20 | KEY_DATA | R | Read pops key (SDL sym) |
| 0x24 | KEY_STATUS | R/W | Bit0: queue not empty, W clears queue |
| 0x28 | KEY_LAST | R | Last key pressed |

Framebuffer offset: base + 0x1000

### UART (0x20000000)

| Offset | Register | Access | Description |
|--------|----------|--------|-------------|
| 0x00 | DATA | R/W | RX read, TX write |
| 0x04 | STATUS | R | Bit0: RX ready, Bit1: TX ready |

### Timer (0x20001000)

| Offset | Register | Access | Description |
|--------|----------|--------|-------------|
| 0x00 | LOW | R | Lower 32 bits (microseconds) |
| 0x04 | HIGH | R | Upper 32 bits |
| 0x08 | CTRL | W | Reset counter |

## Running Tests

```bash
cd build/release
./test/tests
```
