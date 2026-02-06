## Dependency
- libreadline
- libsdl2-image-dev
- libsdl2-ttf-dev

## Run

### CLI
- Required: `--rom <path>`
- Optional: `--config <file>` (default: `emulator.conf`)
- Optional flags/values:
  - `--debug`
  - `--width <pixels>` (default: 640)
  - `--height <pixels>` (default: 480)
  - `--sdl-base <addr>` (default: 0x30000000)
  - `--ram-base <addr>` (default: 0x80000000)
  - `--ram-size <bytes>` (default: 268435456)
  - `--uart-base <addr>` (default: 0x20000000)
  - `--timer-base <addr>` (default: 0x20001000)
  - `--title <string>` (default: Emulator)

### CPU Executor
- This project expects the user to provide a CPU implementation and export
  `extern "C" ICpuExecutor* CreateCpuExecutor();` for the main entry point.

### Config file
- Format: `key=value` per line, supports `#` or `;` comments and hex values (e.g. `0x80000000`).
- Supported keys: `rom`, `debug`, `width`, `height`, `ram_base`, `ram_size`, `uart_base`,
  `timer_base`, `sdl_base`, `title`.

## Memory Map
- ROM: base `0x00000000`, size = ROM file size (must not overlap other devices).
- RAM: base `0x80000000`, size = 256 MiB by default.
- UART: base `0x20000000`, size = 0x100.
- Timer: base `0x20001000`, size = 0x100.
- SDL Device: base `0x30000000`
  - control region size = 0x1000
  - framebuffer at base + 0x1000
  - keyboard registers in control region (see offsets below)

### SDL Display Registers
- `0x00 CTRL` (write 1 = present)
- `0x04 WIDTH` (read-only)
- `0x08 HEIGHT` (read-only)
- `0x0C PITCH` (read-only)
- `0x10 STATUS` (read-only; bit0 ready, bit1 dirty)
- `0x20 KEY_DATA` (read pops next key; value is SDL key sym)
- `0x24 KEY_STATUS` (read-only; bit0 ready if queue not empty; write clears queue)
- `0x28 KEY_LAST` (read-only; last key pressed)

## Devices

### Memory Device
- Raw image format: binary output from `objcopy -O bin`.
- Image is loaded from base offset 0 by default; data beyond memory size is truncated.
- Supports 1/2/4/8 byte reads and writes; ROM rejects writes.

### UART Device
- Registers (offsets):
  - 0x0 DATA (read RX byte, write TX byte)
  - 0x4 STATUS (bit0: RX ready, bit1: TX ready)
- TX writes are printed to stdout.

### Timer Device
- Real-time counter in microseconds.
- Registers (offsets):
  - 0x0 LOW (lower 32 bits)
  - 0x4 HIGH (upper 32 bits)
  - 0x8 CTRL (write to reset counter)
