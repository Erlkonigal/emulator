#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

#include "emulator/app.h"
#include "emulator/cpu/cpu.h"
#include "toy/toy_cpu_executor.h"

namespace emulator {

std::shared_ptr<ICpuExecutor> createCpuExecutor() {
    auto cpu = std::make_shared<ToyCpuExecutor>();
    return cpu;
}

} // namespace emulator

int main(int argc, char* argv[]) {
    int rc = emulator::RunEmulator(argc, argv);    
    emulator::ToyCpuExecutor* cpu = emulator::GetLastToyCpu();
    if (cpu != nullptr) {
        std::cout << "\n=== CPU State ===\n";
        std::cout << "PC: 0x" << std::hex << cpu->getPc() << std::dec << "\n";
        std::cout << "Cycle: " << cpu->getCycle() << "\n";
        std::cout << "Registers:\n";
        for (int i = 0; i < 16; i++) {
            std::cout << "  r" << i << ": 0x" << std::hex << cpu->getRegister(i) << std::dec << "\n";
        }
    }
    return rc;
}