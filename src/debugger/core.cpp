#include "emulator/debugger/debugger.h"
#include "emulator/device/device.h"
#include "emulator/device/uart.h"
#include "emulator/device/display.h"
#include "emulator/app/app.h"
#include "emulator/app/utils.h"
#include "emulator/debugger/expression_parser.h"
#include "emulator/logging/logging.h"

#include "emulator/app/terminal.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <sstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <termios.h>
#include <unistd.h>
#include <poll.h>
#include <iomanip>

namespace {

std::string FormatAccessType(MemAccessType type) {
    switch (type) {
    case MemAccessType::Read:
        return "R";
    case MemAccessType::Write:
        return "W";
    case MemAccessType::Fetch:
        return "F";
    }
    return "?";
}

std::string DefaultFormatter(const TraceRecord& record, const TraceOptions& options) {
    std::stringstream ss;

    if (options.LogInstruction) {
        ss << "PC:0x" << std::hex << std::setw(8) << std::setfill('0') << record.Pc << " ";
        ss << "Inst:0x" << std::hex << std::setw(8) << std::setfill('0') << record.Inst << " ";
        if (!record.Decoded.empty()) {
            ss << "(" << record.Decoded << ")";
        }
        ss << " ";
    }

    if (options.LogBranchPrediction && record.IsBranch) {
        ss << "BP:(T:" << (record.Branch.Taken ? "1" : "0") << " "
           << "P:" << (record.Branch.PredictedTaken ? "1" : "0") << " "
           << "Target:0x" << std::hex << record.Branch.Target << " "
           << "PTarget:0x" << std::hex << record.Branch.PredictedTarget << ")";
        ss << " ";
    }

    if (options.LogMemEvents && !record.MemEvents.empty()) {
        ss << "Mem:[";
        bool first = true;
        for (const auto& event : record.MemEvents) {
            if (event.Type == MemAccessType::Fetch) continue;

            if (!first) ss << ", ";
            ss << FormatAccessType(event.Type) << ":0x" << std::hex << event.Address
               << "=" << event.Data;
            first = false;
        }
        ss << "]";
        ss << " ";
    }

    return ss.str();
}

} // namespace

constexpr uint32_t kInstructionsPerBatch = 1000;
constexpr auto kPresentInterval = std::chrono::milliseconds(16);

Debugger::Debugger(ICpuExecutor* cpu, MemoryBus* bus)
    : Cpu(cpu), Bus(bus), m_TraceFormatter(DefaultFormatter) {
    RegisterCommands();
}

Debugger::~Debugger() = default;

void Debugger::RegisterCommands() {
    Commands = {
        {"run", "Resume execution", &Debugger::CmdRun},
        {"step", "Execute N instructions (default 1)", &Debugger::CmdStep},
        {"pause", "Pause execution", &Debugger::CmdPause},
        {"quit", "Exit the emulator", &Debugger::CmdQuit},
        {"exit", "Exit the emulator", &Debugger::CmdQuit},
        {"regs", "Print register values", &Debugger::CmdRegs},
        {"mem", "Dump memory (mem <addr> <len>)", &Debugger::CmdMem},
        {"eval", "Evaluate an expression (eval <expr>)", &Debugger::CmdEval},
        {"bp", "Manage breakpoints (bp list|add <addr>|del <addr>)", &Debugger::CmdBp},
        {"log", "Set log level (log trace|debug|info|warn|error)", &Debugger::CmdLog},
        {"help", "Show this help message", &Debugger::CmdHelp}
    };
}

void Debugger::SetCpuFrequency(uint32_t cpuFreq) {
    CpuFrequency = cpuFreq;
    uint32_t minThreshold = 0xFFFFFFFF;
    bool anyDevice = false;

    if (Bus) {
        for (auto* device : Bus->GetDevices()) {
            if (!device) continue;

            uint32_t freq = device->GetUpdateFrequency();
            if (freq > 0) {
                uint32_t threshold = std::max(1u, CpuFrequency / freq);
                device->SetSyncThreshold(threshold);

                minThreshold = std::min(minThreshold, threshold);
                anyDevice = true;
            }
        }
    }

    if (anyDevice) {
        SyncThresholdCycles = minThreshold;
    } else {
        if (CpuFrequency > 0) {
            SyncThresholdCycles = std::max(1u, CpuFrequency / 60);
        } else {
            SyncThresholdCycles = 1000;
        }
    }
}

void Debugger::ConfigureTrace(const TraceOptions& options) {
    m_TraceOptions = options;
}

void Debugger::SetTraceFormatter(TraceFormatter formatter) {
    m_TraceFormatter = std::move(formatter);
}

void Debugger::LogTrace(const TraceRecord& record) {
    bool logIt = false;

    if (m_TraceOptions.LogBranchPrediction && record.IsBranch) {
        logIt = true;
    }
    else if (m_TraceOptions.LogInstruction) {
        logIt = true;
    }
    else if (m_TraceOptions.LogMemEvents && !record.MemEvents.empty()) {
        for (const auto& ev : record.MemEvents) {
            if (ev.Type != MemAccessType::Fetch) {
                logIt = true;
                break;
            }
        }
    }

    if (!logIt) {
        return;
    }

    std::string line;
    if (m_TraceFormatter) {
        line = m_TraceFormatter(record, m_TraceOptions);
    } else {
        line = DefaultFormatter(record, m_TraceOptions);
    }

    if (!line.empty()) {
        LOG_TRACE("%s", line.c_str());
    }
}

const TraceOptions& Debugger::GetTraceOptions() const {
    return m_TraceOptions;
}

MemResponse Debugger::BusRead(const MemAccess& access) {
    MemResponse response{};
    if (Bus) {
        response = Bus->Read(access);
    } else {
        response.Success = false;
    }
    return response;
}

MemResponse Debugger::BusWrite(const MemAccess& access) {
    MemResponse response{};
    if (Bus) {
        response = Bus->Write(access);
    } else {
        response.Success = false;
    }
    return response;
}

uint64_t Debugger::GetCpuCycle() {
    if (Cpu) {
        return Cpu->GetCycle();
    }
    return 0;
}

void Debugger::SetSdl(SdlDisplayDevice* sdl) {
    Sdl = sdl;
}

void Debugger::SetRegisterCount(uint32_t count) {
    RegisterCount = count;
}

void Debugger::Run(bool interactive) {
    IsInteractive = interactive;
    State.State.store(interactive ? CpuState::Pause : CpuState::Running, std::memory_order_release);

    if (interactive) {
        m_Terminal = std::make_unique<Terminal>();

        m_Terminal->SetOnCommand([this](const std::string& cmd) {
            std::lock_guard<std::mutex> lock(Mutex);
            // LOG_INFO("> %s", cmd.c_str());
            bool result = this->ProcessCommand(cmd);
            m_Terminal->UpdateLastCommandSuccess(result);
            this->UpdateStatusDisplay();
        });

        m_Terminal->SetOnInput([this](const std::string& data) {
            if (!Bus) return;
            UartDevice* uart = static_cast<UartDevice*>(Bus->GetDevice("UART"));
            if (uart) {
                for (char c : data) {
                    uart->PushRx(static_cast<uint8_t>(c));
                }
            }
        });

        SetupUart();
        SetupLogging();
        UpdateStatusDisplay();
    }

    std::thread cpuThread(&Debugger::CpuThreadLoop, this);
    std::thread sdlThread;

    if (Sdl) {
        sdlThread = std::thread(&Debugger::SdlThreadLoop, this);
    }

    if (interactive) {
        m_Terminal->RunInputLoop();
    } else {
        InputLoop();
    }

    State.ShouldExit.store(true, std::memory_order_release);
    Control.Cv.notify_all();

    if (cpuThread.joinable()) cpuThread.join();
    if (sdlThread.joinable()) sdlThread.join();

    if (Sdl) {
        Sdl->Shutdown();
    }

    m_Terminal.reset();
}

void Debugger::SetupUart() {
    if (!m_Terminal || !Bus) return;

    UartDevice* uart = static_cast<UartDevice*>(Bus->GetDevice("UART"));
    if (uart) {
        auto handler = [this](const std::string& text) {
            for (char ch : text) {
                m_Terminal->PrintChar(static_cast<uint8_t>(ch));
            }
        };
        uart->SetTxHandler(handler);
    }
}

void Debugger::SetupLogging() {
    if (!m_Terminal) return;

    auto outputHandler = [this](const char* msg) {
        m_Terminal->PrintLog("INFO", msg);
    };
    LogSetOutputHandler(outputHandler);
}

void Debugger::CpuThreadLoop() {
    if (Cpu == nullptr || Bus == nullptr) {
        return;
    }

    auto lastUpdate = std::chrono::steady_clock::now();
    m_LastCpsTime = lastUpdate;
    m_LastCpsCycles = Cpu->GetCycle();

    while (!State.ShouldExit.load(std::memory_order_acquire)) {
        uint32_t steps = 0;
        bool stepping = false;
        {
            std::unique_lock<std::mutex> lock(Control.Mutex);
            Control.Cv.wait(lock, [&]() {
                CpuState current = State.State.load(std::memory_order_acquire);
                uint32_t pending = State.StepsPending.load(std::memory_order_acquire);
                bool shouldExit = State.ShouldExit.load(std::memory_order_acquire);
                return shouldExit ||
                    current == CpuState::Running ||
                    pending > 0;
            });
            if (State.ShouldExit.load(std::memory_order_acquire)) {
                break;
            }
            uint32_t pending = State.StepsPending.load(std::memory_order_acquire);
            if (pending > 0) {
                steps = pending;
                State.StepsPending.store(0, std::memory_order_release);
                stepping = true;
                if (State.State.load(std::memory_order_acquire) != CpuState::Running) {
                    State.State.store(CpuState::Running, std::memory_order_release);
                }
            } else if (State.State.load(std::memory_order_acquire) == CpuState::Running) {
                steps = kInstructionsPerBatch;
            }
        }
            if (steps == 0) {
                continue;
            }

            StepResult result = Cpu->Step(steps, SyncThresholdCycles);

            m_TotalInstructions += result.InstructionsExecuted;
            m_LastStepSuccess = result.Success;

            if (!result.Success) {
                State.State.store(CpuState::Halted, std::memory_order_release);
                Control.Cv.notify_all();
                LOG_ERROR("CPU Halted or Encountered Error at 0x%llx", (unsigned long long)Cpu->GetPc());
            }

            Bus->SyncAll(Cpu->GetCycle());

            if (stepping && !State.ShouldExit.load(std::memory_order_acquire)) {
                State.State.store(CpuState::Pause, std::memory_order_release);
            }

            if (stepping || !result.Success) {
                UpdateStatusDisplay();
            } else {
                auto now = std::chrono::steady_clock::now();

                if (now - lastUpdate > std::chrono::milliseconds(30)) {
                       double dt = std::chrono::duration<double>(now - lastUpdate).count();
                       uint64_t currentCycles = Cpu->GetCycle();
                       if (dt > 0) {
                           double dCycles = static_cast<double>(currentCycles - m_LastCpsCycles);
                           m_CurrentCPS.store(dCycles / dt, std::memory_order_release);
                       }
                       m_LastCpsCycles = currentCycles;

                       UpdateStatusDisplay();
                       lastUpdate = now;
                }
            }
    }
}

void Debugger::SdlThreadLoop() {
    if (Sdl == nullptr) {
        return;
    }
    auto lastPresent = std::chrono::steady_clock::now();
    while (!State.ShouldExit.load(std::memory_order_acquire)) {
        bool shouldWait = !Sdl->IsDirty() && !Sdl->IsPresentRequested();
        Sdl->PollEvents(shouldWait ? 8u : 0u);
        if (Sdl->IsQuitRequested()) {
            State.ShouldExit.store(true, std::memory_order_release);
            Control.Cv.notify_all();
            break;
        }
        auto now = std::chrono::steady_clock::now();
        if (Sdl->ConsumePresentRequest()) {
            Sdl->Present();
            lastPresent = now;
        } else if (Sdl->IsDirty() && now - lastPresent >= kPresentInterval) {
            Sdl->Present();
            lastPresent = now;
        }
    }
}

void Debugger::InputLoop() {
    if (Bus == nullptr) {
        return;
    }
    UartDevice* uart = static_cast<UartDevice*>(Bus->GetDevice("UART"));
    if (uart == nullptr) {
        return;
    }

    struct termios oldt, newt;
    bool isTty = isatty(STDIN_FILENO);
    if (isTty) {
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    }

    while (!State.ShouldExit.load(std::memory_order_acquire)) {
         if (State.State.load(std::memory_order_acquire) == CpuState::Halted) {
             break;
         }

         struct pollfd pfd;
         pfd.fd = STDIN_FILENO;
         pfd.events = POLLIN;

         int ret = poll(&pfd, 1, 10);

         if (ret > 0) {
             if (pfd.revents & POLLIN) {
                 char buf[64];
                 ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
                 if (n > 0) {
                     for (ssize_t i = 0; i < n; ++i) {
                         uart->PushRx(static_cast<uint8_t>(buf[i]));
                     }
                 } else if (n == 0) {
                     break;
                 }
             } else if (pfd.revents & (POLLERR | POLLHUP)) {
                 break;
             }
         } else if (ret < 0) {
             if (errno != EINTR) {
                 break;
             }
         }
    }

    if (isTty) {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    }
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
        if (response.Success) {
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
        LOG_INFO("r%u = 0x%llx", regId, (unsigned long long)regs[regId]);
    }
}

uint64_t Debugger::EvalExpression(const std::string& expression) {
    if (expression.empty()) return 0;
    ExpressionParser parser(Cpu, Bus, expression);
    return parser.Parse();
}

void Debugger::AddBreakpoint(uint64_t address) {
    std::lock_guard<std::mutex> lock(Mutex);
    if (std::find(Breakpoints.begin(), Breakpoints.end(), address) == Breakpoints.end()) {
        Breakpoints.push_back(address);
    }
}

void Debugger::RemoveBreakpoint(uint64_t address) {
    std::lock_guard<std::mutex> lock(Mutex);
    Breakpoints.erase(
        std::remove(Breakpoints.begin(), Breakpoints.end(), address),
        Breakpoints.end());
}

bool Debugger::IsBreakpoint(uint64_t address) {
    std::lock_guard<std::mutex> lock(Mutex);
    return std::find(Breakpoints.begin(), Breakpoints.end(), address) != Breakpoints.end();
}

bool Debugger::HasBreakpoints() {
    std::lock_guard<std::mutex> lock(Mutex);
    return !Breakpoints.empty();
}

bool Debugger::ProcessCommand(const std::string& command) {
    std::string trimmed = command;
    TrimInPlace(&trimmed);
    if (trimmed.empty()) {
        return true;
    }

    std::istringstream stream(trimmed);
    std::string verb;
    stream >> verb;

    for (const auto& cmd : Commands) {
        if (cmd.Name == verb) {
            return (this->*cmd.Handler)(stream);
        }
    }

    return false;
}

bool Debugger::CmdRun(std::istringstream& args) {
    (void)args;
    if (State.State.load(std::memory_order_acquire) == CpuState::Halted) {
        LOG_INFO("CPU is halted. Cannot run.");
        return true;
    }
    State.State.store(CpuState::Running, std::memory_order_release);
    Control.Cv.notify_all();
    return true;
}

bool Debugger::CmdStep(std::istringstream& args) {
    if (State.State.load(std::memory_order_acquire) == CpuState::Halted) {
        LOG_INFO("CPU is halted. Cannot step.");
        return true;
    }
    uint32_t steps = 1;
    std::string arg;
    if (args >> arg) {
        uint64_t val = EvalExpression(arg);
        if (val > 0) {
            steps = static_cast<uint32_t>(val);
        }
    }
    State.StepsPending.fetch_add(steps, std::memory_order_release);
    State.State.store(CpuState::Running, std::memory_order_release);
    Control.Cv.notify_all();
    return true;
}

bool Debugger::CmdPause(std::istringstream& args) {
    (void)args;
    
    if (State.State.load(std::memory_order_acquire) == CpuState::Halted) {
        return true;
    }

    State.State.store(CpuState::Pause, std::memory_order_release);
    UpdateStatusDisplay();
    return true;
}

bool Debugger::CmdQuit(std::istringstream& args) {
    (void)args;
    State.ShouldExit.store(true, std::memory_order_release);
    Control.Cv.notify_all();
    
    if (m_Terminal) {
        m_Terminal->Stop();
    }
    return true;
}

bool Debugger::CmdRegs(std::istringstream& args) {
    (void)args;
    PrintRegisters();
    return true;
}

bool Debugger::CmdMem(std::istringstream& args) {
    std::string addrStr;
    std::string lenStr;
    args >> addrStr >> lenStr;
    if (!addrStr.empty() && !lenStr.empty()) {
        uint64_t addr = EvalExpression(addrStr);
        uint64_t len = EvalExpression(lenStr);
        std::vector<uint8_t> data = ScanMemory(addr, static_cast<uint32_t>(len));

        std::string line;
        for (size_t i = 0; i < data.size(); ++i) {
            if (i % 16 == 0) {
                char header[32];
                snprintf(header, sizeof(header), "%08llx: ", (unsigned long long)(addr + i));
                line += header;
            }

            char byteStr[8];
            snprintf(byteStr, sizeof(byteStr), "%02x ", data[i]);
            line += byteStr;

            if (i % 16 == 15 || i + 1 == data.size()) {
                LOG_INFO("%s", line.c_str());
                line.clear();
            }
        }
        return true;
    }
    return false;
}

bool Debugger::CmdEval(std::istringstream& args) {
    std::string expr;
    std::getline(args, expr);
    if (!expr.empty()) {
        uint64_t value = EvalExpression(expr);
        LOG_INFO("0x%llx (%llu)", (unsigned long long)value, (unsigned long long)value);
        return true;
    }
    return false;
}

bool Debugger::CmdBp(std::istringstream& args) {
    std::string action;
    std::string addrStr;
    args >> action;

    if (action == "list" || action.empty()) {
        std::lock_guard<std::mutex> lock(Mutex);
        if (Breakpoints.empty()) {
            LOG_INFO("No breakpoints.");
        } else {
            LOG_INFO("Breakpoints:");
            for (uint64_t bp : Breakpoints) {
                LOG_INFO("  0x%llx", (unsigned long long)bp);
            }
        }
        return true;
    }

    args >> addrStr;
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

bool Debugger::CmdLog(std::istringstream& args) {
    std::string levelStr;
    args >> levelStr;
    std::string trimmed = ToLower(levelStr);
    TrimInPlace(&trimmed);

    LogLevel level = LogLevel::Info;
    bool valid = false;

    if (trimmed == "trace") { level = LogLevel::Trace; valid = true; }
    else if (trimmed == "debug") { level = LogLevel::Debug; valid = true; }
    else if (trimmed == "info") { level = LogLevel::Info; valid = true; }
    else if (trimmed == "warn") { level = LogLevel::Warn; valid = true; }
    else if (trimmed == "error") { level = LogLevel::Error; valid = true; }

    if (valid) {
        LogSetLevel(level);
        LOG_INFO("Log level set to %s", levelStr.c_str());
        return true;
    }

    LOG_INFO("Usage: log [trace|debug|info|warn|error]");
    return true;
}

bool Debugger::CmdHelp(std::istringstream& args) {
    (void)args;
    LOG_INFO("Available commands:");

    size_t maxNameLen = 0;
    for (const auto& cmd : Commands) {
        if (cmd.Name.length() > maxNameLen) {
            maxNameLen = cmd.Name.length();
        }
    }

    for (const auto& cmd : Commands) {
        std::string padding(maxNameLen - cmd.Name.length() + 2, ' ');
        LOG_INFO("  %s%s%s", cmd.Name.c_str(), padding.c_str(), cmd.Help.c_str());
    }
    return true;
}

void Debugger::UpdateStatusDisplay() {
    if (!m_Terminal) return;

    std::string stateStr;
    CpuState s = State.State.load(std::memory_order_acquire);
    switch (s) {
        case CpuState::Running: stateStr = "RUNNING"; break;
        case CpuState::Pause:   stateStr = "PAUSED "; break;
        case CpuState::Halted:  stateStr = "HALTED "; break;
    }

    uint64_t cycles = GetCpuCycle();
    uint64_t pc = Cpu ? Cpu->GetPc() : 0;

    char buffer[256];

    double ipc = 0.0;
    if (cycles > 0) {
        ipc = static_cast<double>(m_TotalInstructions.load(std::memory_order_acquire)) / static_cast<double>(cycles);
    }

    double cps = m_CurrentCPS.load(std::memory_order_acquire);

    char cpsBuf[32];
    if (cps >= 1000000.0) {
        snprintf(cpsBuf, sizeof(cpsBuf), "%.2fM", cps / 1000000.0);
    } else if (cps >= 1000.0) {
        snprintf(cpsBuf, sizeof(cpsBuf), "%.2fK", cps / 1000.0);
    } else {
        snprintf(cpsBuf, sizeof(cpsBuf), "%.0f", cps);
    }

    const char* focusStr = (m_Terminal->GetFocus() == FocusPanel::VTERM) ? "VTERM" : "DEBUG";
    const char* cmdStatus = m_LastStepSuccess ? "OK" : "ERR";

    snprintf(buffer, sizeof(buffer), "CPU: %s | Cycles: %llu | CPS: %s | Cmd: %s | Focus: %s",
             stateStr.c_str(),
             (unsigned long long)cycles,
             cpsBuf,
             cmdStatus,
             focusStr);

    m_Terminal->UpdateStatus(buffer);
}
