#include "emulator/debugger.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <sstream>

Debugger::Debugger(ICpuExecutor* cpu, MemoryBus* bus)
    : Cpu(cpu), Bus(bus) {
}

void Debugger::RunUntilHalt() {
    if (Cpu == nullptr) {
        return;
    }
    while (true) {
        uint64_t pc = Cpu->GetPc();
        if (std::find(Breakpoints.begin(), Breakpoints.end(), pc) != Breakpoints.end()) {
            return;
        }
        CpuResult result = Cpu->StepInstruction();
        if (result == CpuResult::Error) {
            return;
        }
    }
}

void Debugger::StepInstruction() {
    if (Cpu == nullptr) {
        return;
    }
    Cpu->StepInstruction();
}

std::vector<uint8_t> Debugger::ScanMemory(uint64_t address, uint32_t length) {
    std::vector<uint8_t> data;
    if (Bus == nullptr || length == 0) {
        return data;
    }

    data.resize(length);
    for (uint32_t i = 0; i < length; ++i) {
        MemAccess access;
        access.Address = address + i;
        access.Size = 1;
        access.Type = MemAccessType::Read;
        MemResponse response = Bus->Read(access);
        if (response.Result == CpuResult::Success) {
            data[i] = static_cast<uint8_t>(response.Data & 0xff);
        } else {
            data[i] = 0;
        }
    }
    return data;
}

std::vector<uint64_t> Debugger::ReadRegisters() {
    std::vector<uint64_t> regs;
    if (Cpu == nullptr) {
        return regs;
    }
    if (RegisterCount == 0) {
        RegisterCount = Cpu->GetRegisterCount();
    }
    regs.resize(RegisterCount);
    for (uint32_t regId = 0; regId < RegisterCount; ++regId) {
        regs[regId] = Cpu->GetRegister(regId);
    }
    return regs;
}

void Debugger::PrintRegisters() {
    std::vector<uint64_t> regs = ReadRegisters();
    for (uint32_t regId = 0; regId < regs.size(); ++regId) {
        std::printf("r%u = 0x%llx\n", regId, static_cast<unsigned long long>(regs[regId]));
    }
}

uint64_t Debugger::EvalExpression(const std::string& expression) {
    uint64_t value = 0;
    bool isHex = false;
    size_t index = 0;
    while (index < expression.size() && std::isspace(static_cast<unsigned char>(expression[index]))) {
        ++index;
    }
    if (index + 2 <= expression.size() && expression[index] == '0' &&
        (expression[index + 1] == 'x' || expression[index + 1] == 'X')) {
        isHex = true;
        index += 2;
    }
    for (; index < expression.size(); ++index) {
        char ch = expression[index];
        if (std::isspace(static_cast<unsigned char>(ch))) {
            break;
        }
        if (isHex) {
            if (ch >= '0' && ch <= '9') {
                value = (value << 4) + static_cast<uint64_t>(ch - '0');
            } else if (ch >= 'a' && ch <= 'f') {
                value = (value << 4) + static_cast<uint64_t>(ch - 'a' + 10);
            } else if (ch >= 'A' && ch <= 'F') {
                value = (value << 4) + static_cast<uint64_t>(ch - 'A' + 10);
            } else {
                break;
            }
        } else {
            if (ch >= '0' && ch <= '9') {
                value = value * 10 + static_cast<uint64_t>(ch - '0');
            } else {
                break;
            }
        }
    }
    return value;
}

void Debugger::AddBreakpoint(uint64_t address) {
    if (std::find(Breakpoints.begin(), Breakpoints.end(), address) == Breakpoints.end()) {
        Breakpoints.push_back(address);
    }
}

void Debugger::RemoveBreakpoint(uint64_t address) {
    Breakpoints.erase(
        std::remove(Breakpoints.begin(), Breakpoints.end(), address),
        Breakpoints.end());
}

bool Debugger::ProcessCommand(const std::string& command) {
    std::string trimmed = command;
    trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
    trimmed.erase(std::find_if(trimmed.rbegin(), trimmed.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), trimmed.end());

    std::istringstream stream(trimmed);
    std::string verb;
    stream >> verb;
    if (verb == "run") {
        RunUntilHalt();
        return true;
    }
    if (verb == "step") {
        StepInstruction();
        return true;
    }
    if (verb == "regs") {
        PrintRegisters();
        return true;
    }
    if (verb == "mem") {
        std::string addrStr;
        std::string lenStr;
        stream >> addrStr >> lenStr;
        if (!addrStr.empty() && !lenStr.empty()) {
            uint64_t addr = EvalExpression(addrStr);
            uint64_t len = EvalExpression(lenStr);
            std::vector<uint8_t> data = ScanMemory(addr, static_cast<uint32_t>(len));
            for (size_t i = 0; i < data.size(); ++i) {
                if (i % 16 == 0) {
                    std::printf("%08llx: ", static_cast<unsigned long long>(addr + i));
                }
                std::printf("%02x ", data[i]);
                if (i % 16 == 15 || i + 1 == data.size()) {
                    std::printf("\n");
                }
            }
            return true;
        }
        return false;
    }
    if (verb == "eval") {
        std::string expr;
        std::getline(stream, expr);
        if (!expr.empty()) {
            uint64_t value = EvalExpression(expr);
            std::printf("0x%llx (%llu)\n", static_cast<unsigned long long>(value),
                static_cast<unsigned long long>(value));
            return true;
        }
        return false;
    }
    if (verb == "bp") {
        std::string action;
        std::string addrStr;
        stream >> action >> addrStr;
        if (action == "add" && !addrStr.empty()) {
            AddBreakpoint(EvalExpression(addrStr));
            return true;
        }
        if (action == "del" && !addrStr.empty()) {
            RemoveBreakpoint(EvalExpression(addrStr));
            return true;
        }
        return false;
    }
    return false;
}


void Debugger::SetRegisterCount(uint32_t count) {
    RegisterCount = count;
}
