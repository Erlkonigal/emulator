# AGENTS.md

Guidelines for AI coding agents working on this C++20 CPU emulator codebase.

## Build Commands

```bash
# Configure and build (debug)
cmake --preset debug
cmake --build --preset debug

# Configure and build (release)
cmake --preset release
cmake --build --preset release

# Run all tests
ctest --preset debug
ctest --preset release

# Build and run test executable directly
./build/debug/test/tests

# Run a single test (use test name from TEST() macro)
./build/debug/test/tests --filter=test_name
```

## Code Style Guidelines

### Naming Conventions

| Element | Convention | Example |
|---------|------------|---------|
| Classes/Structs/Enums | PascalCase | `MemoryBus`, `CpuState` |
| Constants | `k` + PascalCase | `kDefaultRomBase`, `kMaxNumRegisters` |
| Member variables | `m_` + camelCase | `mCpu`, `mLastSyncCycle` |
| Static variables | `s_` + camelCase | `sInstanceCount` |
| Functions/Methods | camelCase | `registerDevice()`, `findMapping()` |
| Parameters | camelCase | `access`, `device`, `base` |
| Local variables | camelCase | `mapping`, `devices` |
| Namespaces | lowercase | `namespace emulator { }` |

### Formatting

- Indent: 4 spaces (no tabs)
- Braces: K&R style (opening brace on same line)
- Max line length: 100 characters
- Empty function body: `void foo() {}`

### Header Guards

Use `#pragma once` for new headers. Legacy headers may use `#ifndef` guards.

## Imports and Includes

Order includes as follows, separated by blank lines:

```cpp
// 1. Standard library (alphabetically)
#include <cstdint>
#include <string>
#include <vector>

// 2. System headers (POSIX, Linux)
#include <sys/mman.h>
#include <unistd.h>

// 3. Project headers (quoted, with emulator/ prefix)
#include "emulator/cpu/cpu.h"
#include "emulator/utils/config.h"
```

## File Extensions

- Headers: `.h`
- Source files (src/): `.cpp`
- Source files (test/): `.cc`

## C++ Conventions

### Modern C++20 Features

- Use `std::format` for string formatting
- Use `constexpr` for compile-time constants
- Use `[[nodiscard]]`, `[[maybe_unused]]` attributes
- Use `auto` for type deduction where it improves readability

### Types

- Use fixed-width integer types: `uint32_t`, `uint64_t`, `int8_t`, etc.
- Use `size_t` for sizes and counts
- Use `std::string` and `std::string_view` for strings

### Const Correctness

- Mark member functions `const` when they don't modify state
- Use `const` references for non-trivial parameters
- Use `const` for variables that don't change

### Error Handling

Two patterns are used:

1. **Recoverable errors** - Return bool with error output parameter:
   ```cpp
   bool parseConfig(const std::string& path, Config* out, std::string* error);
   ```

2. **Fatal errors** - Throw `std::runtime_error` or use `assert()`:
   ```cpp
   throw std::runtime_error("memfd_create failed");
   assert(ptr != nullptr && "null pointer");
   ```

### Memory Management

- Prefer stack allocation and RAII
- Use `std::unique_ptr` for owning pointers
- Use raw pointers for non-owning references
- Delete copy/move for Singleton classes

## Testing Guidelines

### Test Framework

Custom framework in `test/test_framework.h`:

```cpp
TEST(test_name) {
    EXPECT_TRUE(condition);      // Continues on failure
    ASSERT_TRUE(condition);       // Returns on failure
    EXPECT_EQ(expected, actual);  // Continues on failure
    ASSERT_EQ(expected, actual);  // Returns on failure
    SKIP("reason");               // Skip test with reason
}
```

Tests auto-register via static initialization.

### Test File Organization

- Integration tests: `test/integration_tests.cc`
- Unit tests: `test/*_tests.cc`
- Test helpers: `test/test_helpers.h`, `test/test_helpers.cc`
- Toy ISA: `test/toy_isa.h` (for instruction emission in tests)

### Running Tests

```bash
# All tests
ctest --preset debug

# Verbose output
./build/debug/test/tests

# Filter by test name pattern
./build/debug/test/tests --filter=integration
```

## Architecture Patterns

### Singleton Pattern

Many components use Singleton pattern via `Singleton<T>` base class:

```cpp
class CommitThread : public Singleton<CommitThread> {
    friend class Singleton<CommitThread>;
    // ...
};
```

Access via `ClassName::getInstance()`.

### Threading Model

- **CpuThread**: Produces `CommitInfo` structures
- **CommitThread**: Consumes commits, updates architectural state
- Communication via lock-free SPSC `RingQueue` (double mmap for contiguous access)

### CommitInfo Structure

Each commit represents one instruction's architectural effects:
- PC and instruction bytes
- Register writes (`isRegWrite`, `regId`, `regData`)
- RAM writes (`isRamWrite`, `ramOffset`, `ramData`, `isUncached`)
- CSR access (`isCsrAccess`, `csrState`)
- Error handling (`errorType`, `errorMsg`)

## Important Notes

- This is a Linux-only codebase (uses `memfd_create`, `mmap`)
- Target C++20 standard
- No external dependencies beyond POSIX threads
- Use the existing `ToyCpuExecutor` and `toy::` ISA for writing tests

## Dependencies

- **python3-kconfiglib**: Required for configuration management
  ```bash
  apt install python3-kconfiglib
  ```

## Configuration

Configuration is managed via Kconfig:

```bash
# Interactive menuconfig
python3 scripts/config.py

# Generate header only (used by CMake)
python3 scripts/config.py --gen
```

Configuration files:
- `config/Kconfig` - Configuration options definition
- `config/.config` - Current configuration values
- `include/emulator/generated/hardware_config.h` - Generated C++ header

## Device Interface

All devices inherit from `IDevice` base class:

```cpp
class IDevice {
public:
    virtual ~IDevice() = default;
    virtual bool read(uint64_t offset, uint8_t* data, size_t len) = 0;
    virtual bool write(uint64_t offset, const uint8_t* data, size_t len) = 0;
    virtual void reset() = 0;
};
```

Device implementations:
- `Ram` - Random Access Memory
- `Rom` - Read-Only Memory
- `Uart` - Serial I/O

## Toy ISA

### Memory Map

| Region | Base Address | Size | Access |
|--------|-------------|------|--------|
| ROM | 0x00000000 | File size | Read-only |
| UART | 0x10000000 | 4KB | Byte access only |
| RAM | 0x80000000 | 256MB | Read/Write (all widths) |

### Instruction Set

| Opcode | Instruction | Format | Description |
|--------|-------------|--------|-------------|
| 0x00 | NOP | `NOP` | No operation |
| 0x01 | LUI | `LUI rd, imm16` | `rd = imm16 << 16` |
| 0x02 | ORI | `ORI rd, imm16` | `rd = rd \| imm16` |
| 0x03 | LW | `LW rd, [rs + off]` | `rd = mem[rs+off]` (32-bit) |
| 0x04 | SW | `SW rd, [rs + off]` | `mem[rs+off] = rd` (32-bit) |
| 0x05 | BEQ | `BEQ r0, r1, label` | Branch if equal |
| 0x06 | ADD | `ADD rd, rs, rt` | `rd = rs + rt` |
| 0x07 | SUB | `SUB rd, rs, rt` | `rd = rs - rt` |
| 0x08 | ANDI | `ANDI rd, imm16` | `rd = rd & imm16` |
| 0x09 | LB | `LB rd, [rs + off]` | `rd = SignExt(mem[rs+off][7:0])` |
| 0x0A | LH | `LH rd, [rs + off]` | `rd = SignExt(mem[rs+off][15:0])` |
| 0x0B | LBU | `LBU rd, [rs + off]` | `rd = ZeroExt(mem[rs+off][7:0])` |
| 0x0C | LHU | `LHU rd, [rs + off]` | `rd = ZeroExt(mem[rs+off][15:0])` |
| 0x0D | SB | `SB rd, [rs + off]` | `mem[rs+off][7:0] = rd[7:0]` |
| 0x0E | SH | `SH rd, [rs + off]` | `mem[rs+off][15:0] = rd[15:0]` |
| 0x10 | SRLI | `SRLI rd, rs, shamt` | `rd = rs >> shamt` (logical) |
| 0x11 | AND | `AND rd, rs, rt` | `rd = rs & rt` |
| 0x12 | SLLI | `SLLI rd, rs, shamt` | `rd = rs << shamt` |
| 0x7F | HALT | `HALT` | Stop execution |

### UART Access Notes

- Only byte-level access (LB, LBU, SB) is supported for UART
- Multi-byte accesses (LH, LHU, LW, SH, SW) to UART region are ignored
- Offset 0: DATA (RX read / TX write)
- Offset 4: STATUS (bit 0 = TX ready, bit 1 = RX ready)

### Register File

- 16 registers: r0 - r15
- r0 is always 0 (hardwired)
- 32-bit register width