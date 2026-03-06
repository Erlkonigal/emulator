#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

#include "emulator/app.h"
#include "emulator/cpu/cpu.h"
#include "toy/toy_cpu_executor.h"

std::shared_ptr<ICpuExecutor> createCpuExecutor(const std::vector<uint8_t>& romData) {
    auto cpu = std::make_shared<ToyCpuExecutor>();
    for (size_t i = 0; i + 3 < romData.size(); i += 4) {
        uint32_t word = static_cast<uint32_t>(romData[i]) |
                        (static_cast<uint32_t>(romData[i + 1]) << 8) |
                        (static_cast<uint32_t>(romData[i + 2]) << 16) |
                        (static_cast<uint32_t>(romData[i + 3]) << 24);
        cpu->writeMem(static_cast<uint64_t>(i), word);
    }
    return cpu;
}

static std::vector<uint8_t> ReadRom(const std::string& path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in.is_open()) {
        std::cerr << "Error: Cannot open ROM file: " << path << "\n";
        return {};
    }
    std::streamsize size = in.tellg();
    in.seekg(0, std::ios::beg);
    std::vector<uint8_t> data(size);
    if (!in.read(reinterpret_cast<char*>(data.data()), size)) {
        std::cerr << "Error: Failed to read ROM file\n";
        return {};
    }
    return data;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <rom_file> [--debug]\n";
        return 1;
    }

    std::string romPath = argv[1];
    bool debug = false;
    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--debug") {
            debug = true;
        }
    }

    std::vector<uint8_t> romData = ReadRom(romPath);
    if (romData.empty()) {
        return 1;
    }

    std::cout << "Loaded ROM: " << romPath << " (" << romData.size() << " bytes)\n";

    std::vector<std::string> args;
    args.push_back("demo");
    args.push_back("--rom");
    args.push_back(romPath);
    args.push_back("--ram-size");
    args.push_back("65536");
    if (debug) {
        args.push_back("--debug");
    }

    std::vector<char*> argv2;
    argv2.reserve(args.size());
    for (auto& s : args) {
        argv2.push_back(s.data());
    }

    int rc = RunEmulator(static_cast<int>(argv2.size()), argv2.data());

    ToyCpuExecutor* cpu = GetLastToyCpu();
    if (cpu != nullptr) {
        std::cout << "\n=== CPU State ===\n";
        std::cout << "PC: 0x" << std::hex << cpu->getPc() << std::dec << "\n";
        std::cout << "Cycle: " << cpu->getCycle() << "\n";
        std::cout << "Registers:\n";
        for (int i = 0; i < 16; i++) {
            std::cout << "  r" << i << ": 0x" << std::hex << cpu->getRegister(i) << std::dec << "\n";
        }
        std::cout << "\n=== Memory at 0x200 (results) ===\n";
        for (int i = 0; i < 10; i++) {
            uint32_t val = cpu->readMem(0x200 + i * 4);
            std::cout << "  mem[" << i << "] = " << val << "\n";
        }
    }

    return rc;
}