#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "emulator/app.h"

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

} // namespace

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    // Generate a ROM that draws a simple gradient into the framebuffer and then keeps running.
    // Close the SDL window to exit.
    const uint32_t sdlBase = 0x30000000u;
    const uint32_t fbBase = sdlBase + 0x1000u;
    const uint32_t width = 320;
    const uint32_t height = 240;
    const uint32_t pitch = width * 4;

    std::vector<uint32_t> prog;

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

    // Keep CPU alive at low duty.
    for (int i = 0; i < 1000000; ++i) {
        toy::Emit(&prog, toy::SleepMs(16));
    }
    toy::Emit(&prog, toy::Halt());

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

    std::vector<char*> argv2;
    argv2.reserve(args.size());
    for (auto& s : args) {
        argv2.push_back(s.data());
    }
    return RunEmulator(static_cast<int>(argv2.size()), argv2.data());
}
