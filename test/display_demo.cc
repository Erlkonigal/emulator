#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "emulator/app/app.h"

#include "rom_util.h"
#include "toy_isa.h"

namespace {

std::filesystem::path MakeRomPath() {
    return std::filesystem::path("test") / "build" / "rom" / "display_demo.bin";
}

void EmitWrite32(std::vector<uint32_t>* prog, uint32_t addr, uint32_t value) {
    // r1 = addr
    toy::Emit(prog, toy::Lui(1, static_cast<uint16_t>((addr >> 16) & 0xffffu)));
    toy::Emit(prog, toy::Ori(1, static_cast<uint16_t>(addr & 0xffffu)));
    // r2 = value
    toy::Emit(prog, toy::Lui(2, static_cast<uint16_t>((value >> 16) & 0xffffu)));
    toy::Emit(prog, toy::Ori(2, static_cast<uint16_t>(value & 0xffffu)));
    toy::Emit(prog, toy::Sw(2, 1, 0));
}

void EmitPrint(std::vector<uint32_t>* prog, uint32_t uartBase, const std::string& msg) {
    // r1 = uartBase
    toy::Emit(prog, toy::Lui(1, static_cast<uint16_t>((uartBase >> 16) & 0xffffu)));
    toy::Emit(prog, toy::Ori(1, static_cast<uint16_t>(uartBase & 0xffffu)));
    
    for (char c : msg) {
        // r2 = char
        toy::Emit(prog, toy::Lui(2, 0));
        toy::Emit(prog, toy::Ori(2, static_cast<uint16_t>(c)));
        // Sw r2, [r1 + 0]
        toy::Emit(prog, toy::Sw(2, 1, 0));
    }
}

} // namespace

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    // Configuration
    // const uint32_t romBase = 0x00000000u;
    const uint32_t uartBase = 0x20000000u;
    // const uint32_t timerBase = 0x20001000u;
    const uint32_t sdlBase = 0x30000000u;
    // const uint32_t ramBase = 0x80000000u;
    
    const uint32_t fbBase = sdlBase + 0x1000u;
    const uint32_t width = 320;
    const uint32_t height = 240;
    const uint32_t pitch = width * 4;

    std::vector<uint32_t> prog;

    // 1. Hello World to UART
    EmitPrint(&prog, uartBase, "Display Demo Started.\r\n");
    EmitPrint(&prog, uartBase, "Initializing Display...\r\n");

    // 2. Draw Gradient
    // Fill a small top-left rectangle with a gradient (ARGB8888), 32-bit stores.
    for (uint32_t y = 0; y < 64; ++y) {
        for (uint32_t x = 0; x < 96; ++x) {
            uint8_t r = static_cast<uint8_t>((x * 255) / 95);
            uint8_t g = static_cast<uint8_t>((y * 255) / 63);
            uint8_t b = 0x40;
            uint32_t argb = 0xff000000u | (static_cast<uint32_t>(r) << 16) |
                (static_cast<uint32_t>(g) << 8) | b;
            uint32_t addr = fbBase + y * pitch + x * 4;
            EmitWrite32(&prog, addr, argb);
        }
    }

    // Request present.
    EmitWrite32(&prog, sdlBase + 0x00u, 1u);

    EmitPrint(&prog, uartBase, "Display Initialized.\r\n");
    EmitPrint(&prog, uartBase, "Press any key in the window to exit.\r\n");

    // 3. Main Loop: Poll UART and Wait for Key Press
    // Register Setup:
    // r11 = SdlKeyStatusAddr
    // r4 = UartBase
    // r8 = 3 (UART Status Mask: RX_READY | TX_READY)

    uint32_t keyStatusAddr = sdlBase + 0x24;
    toy::Emit(&prog, toy::Lui(11, static_cast<uint16_t>((keyStatusAddr >> 16) & 0xffffu)));
    toy::Emit(&prog, toy::Ori(11, static_cast<uint16_t>(keyStatusAddr & 0xffffu)));

    // r4 = uartBase
    toy::Emit(&prog, toy::Lui(4, static_cast<uint16_t>((uartBase >> 16) & 0xffffu)));
    toy::Emit(&prog, toy::Ori(4, static_cast<uint16_t>(uartBase & 0xffffu)));

    // r8 = 3
    toy::Emit(&prog, toy::Lui(8, 0));
    toy::Emit(&prog, toy::Ori(8, 3));

    // Clear any existing key press (Write to Status Register clears it)
    toy::Emit(&prog, toy::Sw(0, 11, 0));

    // Loop Start (Relative Index 0)
    // 0: Read UART Status: Lw r5, [r4 + 4]
    toy::Emit(&prog, toy::Lw(5, 4, 4));

    // 1: Check if Data Ready (Status == 3). Beq r5, r8, +1 (Goto Echo)
    //    Current=1, Next=2. Target=3. 2 + off = 3 => off=1.
    toy::Emit(&prog, toy::Beq(5, 8, 1));

    // 2: Else Jump to Key Check. Beq r0, r0, +3 (Goto KeyCheck)
    //    Current=2, Next=3. Target=6. 3 + off = 6 => off=3.
    toy::Emit(&prog, toy::Beq(0, 0, 3));

    // --- Echo Routine ---
    // 3: Read Char: Lw r6, [r4 + 0]
    toy::Emit(&prog, toy::Lw(6, 4, 0));

    // 4: Write Char: Sw r6, [r4 + 0]
    toy::Emit(&prog, toy::Sw(6, 4, 0));

    // 5: Jump back to Start. Beq r0, r0, -6 (Goto 0)
    //    Current=5, Next=6. Target=0. 6 + off = 0 => off=-6.
    toy::Emit(&prog, toy::Beq(0, 0, -6));

    // --- Key Check Routine ---
    // 6: Read Key Status: Lw r3, [r11 + 0]
    toy::Emit(&prog, toy::Lw(3, 11, 0));

    // 7: If No Key (0), Skip Halt. Beq r3, r0, +1 (Goto Sleep)
    //    Current=7, Next=8. Target=9. 8 + off = 9 => off=1.
    toy::Emit(&prog, toy::Beq(3, 0, 1));

    // 8: Halt
    toy::Emit(&prog, toy::Halt());

    // 9: Jump back to Start. Beq r0, r0, -10 (Goto 0)
    //    Current=9, Next=10. Target=0. 10 + off = 0 => off=-10.
    toy::Emit(&prog, toy::Beq(0, 0, -10));

    // End of ROM generation

    std::string err;
    auto romPath = MakeRomPath();
    if (!rom::WriteRomU32LE(romPath, prog, &err)) {
        return 1;
    }

    std::vector<std::string> args;
    args.push_back("display_demo");
    args.push_back("--rom");
    args.push_back(romPath.string());
    args.push_back("--width");
    args.push_back(std::to_string(width));
    args.push_back("--height");
    args.push_back(std::to_string(height));
    args.push_back("--ram-size");
    args.push_back("65536");
    args.push_back("--title");
    args.push_back("Emulator Display Demo");
    
    // Optional: Enable trace if you want to see what's happening
    args.push_back("--log-level");
    args.push_back("trace");
    // args.push_back("--mtrace");
    // args.push_back("--itrace");
    // args.push_back("--bptrace");

    args.push_back("--debug");

    std::vector<char*> argv2;
    argv2.reserve(args.size());
    for (auto& s : args) {
        argv2.push_back(s.data());
    }
    return RunEmulator(static_cast<int>(argv2.size()), argv2.data());
}
