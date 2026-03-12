#include "framework/test_framework.h"
#include <vector>
#include "emulator/device/uart.h"
#include "emulator/device/rom.h"
#include "emulator/device/ram.h"
#include "toy/toy_cpu_executor.h"
#include "toy/toy_isa.h"

using emulator::CommitArray;
using emulator::CpuErrorType;
using emulator::Ram;
using emulator::Rom;
using emulator::ToyCpuExecutor;
using emulator::Uart;

TEST(debug_uart_output_single_char) {
    std::vector<uint32_t> prog;
    
    // LUI r6, 0x1000 (UART base)
    toy::Emit(&prog, toy::Lui(6, 0x1000));
    
    // wait_tx: LBU r12, [r6, 4] (read status)
    toy::Emit(&prog, toy::Lbu(12, 6, 4));
    
    // ANDI r12, 0x0001 (check TX ready)
    toy::Emit(&prog, toy::Andi(12, 0x0001));
    
    // BEQ r12, r0, -3 (loop if not ready)
    toy::Emit(&prog, toy::Beq(12, 0, -3));
    
    // LUI r12, 0x0041 ('A')
    toy::Emit(&prog, toy::Lui(12, 0x0041));
    
    // SB r12, [r6, 0] (output)
    toy::Emit(&prog, toy::Sb(12, 6, 0));
    
    // HALT
    toy::Emit(&prog, toy::Halt());
    
    // Initialize
    std::vector<uint8_t> romData(prog.size() * 4);
    for (size_t i = 0; i < prog.size(); i++) {
        romData[i*4 + 0] = prog[i] & 0xff;
        romData[i*4 + 1] = (prog[i] >> 8) & 0xff;
        romData[i*4 + 2] = (prog[i] >> 16) & 0xff;
        romData[i*4 + 3] = (prog[i] >> 24) & 0xff;
    }
    
    Rom::getInstance().init(romData);
    Ram::getInstance().init(65536);
    Uart::getInstance().reset();
    
    auto cpu = std::make_shared<ToyCpuExecutor>();
    cpu->reset();
    cpu->setResetPc(0);
    
    // Run for max 100 cycles
    for (int i = 0; i < 100; i++) {
        CommitArray commits;
        cpu->cycle(commits);
        
        // Process UART TX
        auto& uart = Uart::getInstance();
        while (uart.hasTxData()) {
            uint8_t byte;
            uart.popTx(&byte);
        }
        
        if (cpu->getLastError() == CpuErrorType::Halt) {
            break;
        }
    }
    
    EXPECT_EQ(cpu->getLastError(), CpuErrorType::Halt);
    EXPECT_TRUE(Uart::getInstance().hasTxData() == false);
}

TEST(debug_srli_instruction) {
    std::vector<uint32_t> prog;
    
    // Test SRLI: r2 = 0x12345678 >> 4 = 0x01234567
    toy::Emit(&prog, toy::Lui(2, 0x1234));
    toy::Emit(&prog, toy::Ori(2, 0x5678));
    toy::Emit(&prog, toy::Srli(3, 2, 4));
    toy::Emit(&prog, toy::Halt());
    
    std::vector<uint8_t> romData(prog.size() * 4);
    for (size_t i = 0; i < prog.size(); i++) {
        romData[i*4 + 0] = prog[i] & 0xff;
        romData[i*4 + 1] = (prog[i] >> 8) & 0xff;
        romData[i*4 + 2] = (prog[i] >> 16) & 0xff;
        romData[i*4 + 3] = (prog[i] >> 24) & 0xff;
    }
    
    Rom::getInstance().init(romData);
    Ram::getInstance().init(65536);
    Uart::getInstance().reset();
    
    auto cpu = std::make_shared<ToyCpuExecutor>();
    cpu->reset();
    cpu->setResetPc(0);
    
    for (int i = 0; i < 10; i++) {
        CommitArray commits;
        cpu->cycle(commits);
        if (cpu->getLastError() == CpuErrorType::Halt) break;
    }
    
    EXPECT_EQ(cpu->getLastError(), CpuErrorType::Halt);
    EXPECT_EQ(cpu->getRegister(2), 0x12345678ULL);
    EXPECT_EQ(cpu->getRegister(3), 0x01234567ULL);
}

TEST(debug_srli_large_shift) {
    std::vector<uint32_t> prog;
    
    toy::Emit(&prog, toy::Lui(2, 0x1234));
    toy::Emit(&prog, toy::Ori(2, 0x5678));
    toy::Emit(&prog, toy::Srli(3, 2, 60));
    toy::Emit(&prog, toy::Halt());
    
    std::vector<uint8_t> romData(prog.size() * 4);
    for (size_t i = 0; i < prog.size(); i++) {
        romData[i*4 + 0] = prog[i] & 0xff;
        romData[i*4 + 1] = (prog[i] >> 8) & 0xff;
        romData[i*4 + 2] = (prog[i] >> 16) & 0xff;
        romData[i*4 + 3] = (prog[i] >> 24) & 0xff;
    }
    
    Rom::getInstance().init(romData);
    Ram::getInstance().init(65536);
    Uart::getInstance().reset();
    
    auto cpu = std::make_shared<ToyCpuExecutor>();
    cpu->reset();
    cpu->setResetPc(0);
    
    for (int i = 0; i < 10; i++) {
        CommitArray commits;
        cpu->cycle(commits);
        if (cpu->getLastError() == CpuErrorType::Halt) break;
    }
    
    EXPECT_EQ(cpu->getLastError(), CpuErrorType::Halt);
    EXPECT_EQ(cpu->getRegister(3), 1u);
}
