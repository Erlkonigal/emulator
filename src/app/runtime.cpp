#include "emulator/app.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

#include "emulator/app.h"
#include "emulator/bus.h"
#include "emulator/debugger.h"
#include "emulator/device.h"

namespace {
constexpr uint32_t kInstructionsPerTick = 1000;
constexpr auto kPresentInterval = std::chrono::milliseconds(16);
}

void PumpTerminalInput(UartDevice* uart) {
    if (uart == nullptr) {
        return;
    }
    if (!std::cin.good()) {
        return;
    }
    while (std::cin.rdbuf()->in_avail() > 0) {
        int ch = std::cin.get();
        if (ch == EOF) {
            break;
        }
        uart->PushRx(static_cast<uint8_t>(ch & 0xff));
    }
}

void RunCpuThread(ICpuExecutor* cpu, TimerDevice* timer, EmulatorRunState* state,
    CpuControl* control) {
    if (cpu == nullptr || state == nullptr || control == nullptr) {
        return;
    }
    while (state->State.load(std::memory_order_acquire) != CpuState::Finish) {
        uint32_t steps = 0;
        bool stepping = false;
        {
            std::unique_lock<std::mutex> lock(control->Mutex);
            control->Cv.wait(lock, [&]() {
                CpuState current = state->State.load(std::memory_order_acquire);
                uint32_t pending = state->StepsPending.load(std::memory_order_acquire);
                return current == CpuState::Finish ||
                    current == CpuState::Running ||
                    pending > 0;
            });
            if (state->State.load(std::memory_order_acquire) == CpuState::Finish) {
                break;
            }
            uint32_t pending = state->StepsPending.load(std::memory_order_acquire);
            if (pending > 0) {
                steps = pending;
                state->StepsPending.store(0, std::memory_order_release);
                stepping = true;
                if (state->State.load(std::memory_order_acquire) != CpuState::Running) {
                    state->State.store(CpuState::Running, std::memory_order_release);
                }
            } else if (state->State.load(std::memory_order_acquire) == CpuState::Running) {
                steps = kInstructionsPerTick;
            }
        }
        if (steps == 0) {
            continue;
        }
        uint32_t executed = 0;
        for (uint32_t i = 0; i < steps &&
            state->State.load(std::memory_order_acquire) == CpuState::Running; ++i) {
            CpuResult result = cpu->StepInstruction();
            ++executed;
            if (result == CpuResult::Error) {
                state->State.store(CpuState::Finish, std::memory_order_release);
                control->Cv.notify_all();
                break;
            }
        }
        if (timer != nullptr && executed > 0) {
            timer->Tick(executed);
        }
        if (stepping && state->State.load(std::memory_order_acquire) != CpuState::Finish) {
            state->State.store(CpuState::Pause, std::memory_order_release);
        }
    }
}

void RunSdlThread(SdlDisplayDevice* sdl, EmulatorRunState* state, CpuControl* control) {
    if (sdl == nullptr || state == nullptr) {
        return;
    }
    auto lastPresent = std::chrono::steady_clock::now();
    while (state->State.load(std::memory_order_acquire) != CpuState::Finish) {
        bool shouldWait = !sdl->IsDirty() && !sdl->IsPresentRequested();
        sdl->PollEvents(shouldWait ? 8u : 0u);
        if (sdl->IsQuitRequested()) {
            state->State.store(CpuState::Finish, std::memory_order_release);
            if (control != nullptr) {
                control->Cv.notify_all();
            }
            break;
        }
        auto now = std::chrono::steady_clock::now();
        if (sdl->ConsumePresentRequest()) {
            sdl->Present();
            lastPresent = now;
        } else if (sdl->IsDirty() && now - lastPresent >= kPresentInterval) {
            sdl->Present();
            lastPresent = now;
        }
    }
}

void RunDebugger(ICpuExecutor* cpu, MemoryBus* bus, UartDevice* uart, EmulatorRunState* state,
    CpuControl* control) {
    Debugger debugger(cpu, bus);
    debugger.SetRegisterCount(cpu->GetRegisterCount());
    std::string line;
    auto txHandler = [](const std::string& text) {
        if (text.empty()) {
            return;
        }
        std::fwrite("\r\n", 1, 2, stdout);
        std::fwrite(text.data(), 1, text.size(), stdout);
        if (text.back() != '\n') {
            std::fwrite("\n", 1, 1, stdout);
        }
        std::fwrite("dbg> ", 1, 5, stdout);
        std::fflush(stdout);
    };
    if (uart != nullptr) {
        uart->SetTxHandler(txHandler);
    }
    while (state->State.load(std::memory_order_acquire) != CpuState::Finish) {
        std::fprintf(stdout, "dbg> ");
        std::fflush(stdout);
        if (!std::getline(std::cin, line)) {
            state->State.store(CpuState::Finish, std::memory_order_release);
            control->Cv.notify_all();
            break;
        }
        std::string trimmed = line;
        TrimInPlace(&trimmed);
        if (trimmed == "quit" || trimmed == "exit") {
            state->State.store(CpuState::Finish, std::memory_order_release);
            control->Cv.notify_all();
            break;
        }
        if (trimmed == "run") {
            state->State.store(CpuState::Running, std::memory_order_release);
            control->Cv.notify_all();
            continue;
        }
        if (trimmed == "step") {
            state->StepsPending.fetch_add(1u, std::memory_order_release);
            state->State.store(CpuState::Running, std::memory_order_release);
            control->Cv.notify_all();
            continue;
        }
        if (trimmed == "pause") {
            state->State.store(CpuState::Pause, std::memory_order_release);
            continue;
        }
        std::istringstream stream(trimmed);
        std::string verb;
        stream >> verb;
        if (verb == "input") {
            std::string payload;
            std::getline(stream, payload);
            if (!payload.empty() && payload.front() == ' ') {
                payload.erase(payload.begin());
            }
            if (uart != nullptr) {
                for (char ch : payload) {
                    uart->PushRx(static_cast<uint8_t>(ch));
                }
            }
        } else if (!debugger.ProcessCommand(line)) {
            std::fprintf(stdout, "Unknown command\n");
        }
        if (state->State.load(std::memory_order_acquire) == CpuState::Finish) {
            control->Cv.notify_all();
            break;
        }
    }
    if (uart != nullptr) {
        uart->SetTxHandler(nullptr);
    }
}
