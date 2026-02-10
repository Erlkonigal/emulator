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

std::string formatAccessType(MemAccessType type) {
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

std::string defaultFormatter(const TraceRecord& record, const TraceOptions& options) {
    std::stringstream ss;

    if (options.logInstruction) {
        ss << "PC:0x" << std::hex << std::setw(8) << std::setfill('0') << record.pc << " ";
        ss << "Inst:0x" << std::hex << std::setw(8) << std::setfill('0') << record.inst << " ";
        if (!record.decoded.empty()) {
            ss << "(" << record.decoded << ")";
        }
        ss << " ";
    }

    if (options.logBranchPrediction && record.isBranch) {
        ss << "BP:(T:" << (record.branch.taken ? "1" : "0") << " "
           << "P:" << (record.branch.predictedTaken ? "1" : "0") << " "
           << "Target:0x" << std::hex << record.branch.target << " "
           << "PTarget:0x" << std::hex << record.branch.predictedTarget << ")";
        ss << " ";
    }

    if (options.logMemEvents && !record.memEvents.empty()) {
        ss << "Mem:[";
        bool first = true;
        for (const auto& event : record.memEvents) {
            if (event.type == MemAccessType::Fetch) continue;

            if (!first) ss << ", ";
            ss << formatAccessType(event.type) << ":0x" << std::hex << event.address
               << "=" << event.data;
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
    : mCpu(cpu), mBus(bus), mTraceFormatter(defaultFormatter) {
    registerCommands();
}

Debugger::~Debugger() = default;

void Debugger::registerCommands() {
    mCommands = {
        {"run", "Resume execution", &Debugger::cmdRun},
        {"step", "Execute N instructions (default 1)", &Debugger::cmdStep},
        {"pause", "Pause execution", &Debugger::cmdPause},
        {"quit", "Exit the emulator", &Debugger::cmdQuit},
        {"exit", "Exit the emulator", &Debugger::cmdQuit},
        {"regs", "Print register values", &Debugger::cmdRegs},
        {"mem", "Dump memory (mem <addr> <len>)", &Debugger::cmdMem},
        {"eval", "Evaluate an expression (eval <expr>)", &Debugger::cmdEval},
        {"bp", "Manage breakpoints (bp list|add <addr>|del <addr>)", &Debugger::cmdBp},
        {"log", "Set log level (log trace|debug|info|warn|error)", &Debugger::cmdLog},
        {"help", "Show this help message", &Debugger::cmdHelp}
    };
}

void Debugger::setCpuFrequency(uint32_t cpuFreq) {
    mCpuFrequency = cpuFreq;
    uint32_t minThreshold = 0xFFFFFFFF;
    bool anyDevice = false;

    if (mBus) {
        for (auto* device : mBus->getDevices()) {
            if (!device) continue;

            uint32_t freq = device->getUpdateFrequency();
            if (freq > 0) {
                uint32_t threshold = std::max(1u, mCpuFrequency / freq);
                device->setSyncThreshold(threshold);

                minThreshold = std::min(minThreshold, threshold);
                anyDevice = true;
            }
        }
    }

    if (anyDevice) {
        mSyncThresholdCycles = minThreshold;
    } else {
        if (mCpuFrequency > 0) {
            mSyncThresholdCycles = std::max(1u, mCpuFrequency / 60);
        } else {
            mSyncThresholdCycles = 1000;
        }
    }
}

void Debugger::configureTrace(const TraceOptions& options) {
    mTraceOptions = options;
}

void Debugger::setTraceFormatter(TraceFormatter formatter) {
    mTraceFormatter = std::move(formatter);
}

void Debugger::logTrace(const TraceRecord& record) {
    bool logIt = false;

    if (mTraceOptions.logBranchPrediction && record.isBranch) {
        logIt = true;
    }
    else if (mTraceOptions.logInstruction) {
        logIt = true;
    }
    else if (mTraceOptions.logMemEvents && !record.memEvents.empty()) {
        for (const auto& ev : record.memEvents) {
            if (ev.type != MemAccessType::Fetch) {
                logIt = true;
                break;
            }
        }
    }

    if (!logIt) {
        return;
    }

    std::string line;
    if (mTraceFormatter) {
        line = mTraceFormatter(record, mTraceOptions);
    } else {
        line = defaultFormatter(record, mTraceOptions);
    }

    if (!line.empty()) {
        LOG_TRACE("%s", line.c_str());
    }
}

const TraceOptions& Debugger::getTraceOptions() const {
    return mTraceOptions;
}

MemResponse Debugger::busRead(const MemAccess& access) {
    MemResponse response{};
    if (mBus) {
        response = mBus->read(access);
    } else {
        response.success = false;
    }
    return response;
}

MemResponse Debugger::busWrite(const MemAccess& access) {
    MemResponse response{};
    if (mBus) {
        response = mBus->write(access);
    } else {
        response.success = false;
    }
    return response;
}

uint64_t Debugger::getCpuCycle() {
    if (mCpu) {
        return mCpu->getCycle();
    }
    return 0;
}

void Debugger::setSdl(SdlDisplayDevice* sdl) {
    mSdl = sdl;
}

void Debugger::setRegisterCount(uint32_t count) {
    mRegisterCount = count;
}

void Debugger::run(bool interactive) {
    mIsInteractive = interactive;
    mState.state.store(interactive ? CpuState::Pause : CpuState::Running, std::memory_order_release);

    if (interactive) {
        mTerminal = std::make_unique<Terminal>();

        mTerminal->setOnCommand([this](const std::string& cmd) {
            std::lock_guard<std::mutex> lock(mMutex);
            mLastCommandSuccess = this->processCommand(cmd);
            this->updateStatusDisplay();
        });

        mTerminal->setOnInput([this](const std::string& data) {
            if (!mBus) return;
            UartDevice* uart = static_cast<UartDevice*>(mBus->getDevice("UART"));
            if (uart) {
                for (char c : data) {
                    uart->pushRx(static_cast<uint8_t>(c));
                }
            }
        });

        setupUart();
        setupLogging();
        updateStatusDisplay();
    }

    std::thread cpuThread(&Debugger::cpuThreadLoop, this);
    std::thread sdlThread;

    if (mSdl) {
        sdlThread = std::thread(&Debugger::sdlThreadLoop, this);
    }

    if (interactive) {
        mTerminal->runInputLoop();
    } else {
        inputLoop();
    }

    mState.shouldExit.store(true, std::memory_order_release);
    mControl.cv.notify_all();

    if (cpuThread.joinable()) cpuThread.join();
    if (sdlThread.joinable()) sdlThread.join();

    if (mSdl) {
        mSdl->shutdown();
    }

    mTerminal.reset();
}

void Debugger::setupUart() {
    if (!mTerminal || !mBus) return;

    UartDevice* uart = static_cast<UartDevice*>(mBus->getDevice("UART"));
    if (uart) {
        auto handler = [this](const std::string& text) {
            for (char ch : text) {
                mTerminal->printChar(static_cast<uint8_t>(ch));
            }
        };
        uart->setTxHandler(handler);
    }
}

void Debugger::setupLogging() {
    if (!mTerminal) return;

    auto outputHandler = [this](const char* msg) {
        mTerminal->printLog(msg);
    };
    logSetOutputHandler(outputHandler);
}

void Debugger::cpuThreadLoop() {
    if (mCpu == nullptr || mBus == nullptr) {
        return;
    }

    auto lastUpdate = std::chrono::steady_clock::now();
    mLastCpsTime = lastUpdate;
    mLastCpsCycles = mCpu->getCycle();

    while (!mState.shouldExit.load(std::memory_order_acquire)) {
        uint32_t steps = 0;
        bool stepping = false;
        {
            std::unique_lock<std::mutex> lock(mControl.mutex);
            mControl.cv.wait(lock, [&]() {
                CpuState current = mState.state.load(std::memory_order_acquire);
                uint32_t pending = mState.stepsPending.load(std::memory_order_acquire);
                bool shouldExit = mState.shouldExit.load(std::memory_order_acquire);
                return shouldExit ||
                    current == CpuState::Running ||
                    pending > 0;
            });
            if (mState.shouldExit.load(std::memory_order_acquire)) {
                break;
            }
            uint32_t pending = mState.stepsPending.load(std::memory_order_acquire);
            if (pending > 0) {
                steps = pending;
                mState.stepsPending.store(0, std::memory_order_release);
                stepping = true;
                if (mState.state.load(std::memory_order_acquire) != CpuState::Running) {
                    mState.state.store(CpuState::Running, std::memory_order_release);
                }
            } else if (mState.state.load(std::memory_order_acquire) == CpuState::Running) {
                steps = kInstructionsPerBatch;
            }
        }
            if (steps == 0) {
                continue;
            }

            StepResult result = mCpu->step(steps, mSyncThresholdCycles);

            mTotalInstructions += result.instructionsExecuted;

            if (!result.success) {
                mState.state.store(CpuState::Halted, std::memory_order_release);
                mControl.cv.notify_all();
                LOG_ERROR("CPU Halted or Encountered Error at 0x%llx", (unsigned long long)mCpu->getPc());
            }

            mBus->syncAll(mCpu->getCycle());

            if (stepping && !mState.shouldExit.load(std::memory_order_acquire)) {
                mState.state.store(CpuState::Pause, std::memory_order_release);
            }

            if (stepping || !result.success) {
                updateStatusDisplay();
            } else {
                auto now = std::chrono::steady_clock::now();

                if (now - lastUpdate > std::chrono::milliseconds(30)) {
                       double dt = std::chrono::duration<double>(now - lastUpdate).count();
                       uint64_t currentCycles = mCpu->getCycle();
                       if (dt > 0) {
                           double dCycles = static_cast<double>(currentCycles - mLastCpsCycles);
                           mCurrentCPS.store(dCycles / dt, std::memory_order_release);
                       }
                       mLastCpsCycles = currentCycles;

                       updateStatusDisplay();
                       lastUpdate = now;
                }
            }
    }
}

void Debugger::sdlThreadLoop() {
    if (mSdl == nullptr) {
        return;
    }
    auto lastPresent = std::chrono::steady_clock::now();
    while (!mState.shouldExit.load(std::memory_order_acquire)) {
        bool shouldWait = !mSdl->isDirty() && !mSdl->isPresentRequested();
        mSdl->pollEvents(shouldWait ? 8u : 0u);
        if (mSdl->isQuitRequested()) {
            mState.shouldExit.store(true, std::memory_order_release);
            mControl.cv.notify_all();
            break;
        }
        auto now = std::chrono::steady_clock::now();
        if (mSdl->consumePresentRequest()) {
            mSdl->present();
            lastPresent = now;
        } else if (mSdl->isDirty() && now - lastPresent >= kPresentInterval) {
            mSdl->present();
            lastPresent = now;
        }
    }
}

void Debugger::inputLoop() {
    if (mBus == nullptr) {
        return;
    }
    UartDevice* uart = static_cast<UartDevice*>(mBus->getDevice("UART"));
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

    while (!mState.shouldExit.load(std::memory_order_acquire)) {
         if (mState.state.load(std::memory_order_acquire) == CpuState::Halted) {
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
                         uart->pushRx(static_cast<uint8_t>(buf[i]));
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

std::vector<uint8_t> Debugger::scanMemory(uint64_t address, uint32_t length) {
    std::vector<uint8_t> data;
    if (mBus == nullptr || length == 0) {
        return data;
    }

    data.resize(length);
    for (uint32_t i = 0; i < length; ++i) {
        MemAccess access;
        access.address = address + i;
        access.size = 1;
        access.type = MemAccessType::Read;
        MemResponse response = mBus->read(access);
        if (response.success) {
            data[i] = static_cast<uint8_t>(response.data & 0xff);
        } else {
            data[i] = 0;
        }
    }
    return data;
}

std::vector<uint64_t> Debugger::readRegisters() {
    std::vector<uint64_t> regs;
    if (mCpu == nullptr) {
        return regs;
    }
    if (mRegisterCount == 0) {
        mRegisterCount = mCpu->getRegisterCount();
    }
    regs.resize(mRegisterCount);
    for (uint32_t regId = 0; regId < mRegisterCount; ++regId) {
        regs[regId] = mCpu->getRegister(regId);
    }
    return regs;
}

void Debugger::printRegisters() {
    std::vector<uint64_t> regs = readRegisters();
    for (uint32_t regId = 0; regId < regs.size(); ++regId) {
        LOG_INFO("r%u = 0x%llx", regId, (unsigned long long)regs[regId]);
    }
}

uint64_t Debugger::evalExpression(const std::string& expression) {
    if (expression.empty()) return 0;
    ExpressionParser parser(mCpu, mBus, expression);
    return parser.parse();
}

void Debugger::addBreakpoint(uint64_t address) {
    std::lock_guard<std::mutex> lock(mMutex);
    if (std::find(mBreakpoints.begin(), mBreakpoints.end(), address) == mBreakpoints.end()) {
        mBreakpoints.push_back(address);
    }
}

void Debugger::removeBreakpoint(uint64_t address) {
    std::lock_guard<std::mutex> lock(mMutex);
    mBreakpoints.erase(
        std::remove(mBreakpoints.begin(), mBreakpoints.end(), address),
        mBreakpoints.end());
}

bool Debugger::isBreakpoint(uint64_t address) {
    std::lock_guard<std::mutex> lock(mMutex);
    return std::find(mBreakpoints.begin(), mBreakpoints.end(), address) != mBreakpoints.end();
}

bool Debugger::hasBreakpoints() {
    std::lock_guard<std::mutex> lock(mMutex);
    return !mBreakpoints.empty();
}

bool Debugger::processCommand(const std::string& command) {
    std::string trimmed = command;
    trimInPlace(&trimmed);
    if (trimmed.empty()) {
        return true;
    }

    std::istringstream stream(trimmed);
    std::string verb;
    stream >> verb;

    for (const auto& cmd : mCommands) {
        if (cmd.name == verb) {
            return (this->*cmd.Handler)(stream);
        }
    }

    return false;
}

bool Debugger::cmdRun(std::istringstream& args) {
    (void)args;
    if (mState.state.load(std::memory_order_acquire) == CpuState::Halted) {
        LOG_INFO("CPU is halted. Cannot run.");
        return false;
    }
    mState.state.store(CpuState::Running, std::memory_order_release);
    mControl.cv.notify_all();
    return true;
}

bool Debugger::cmdStep(std::istringstream& args) {
    if (mState.state.load(std::memory_order_acquire) == CpuState::Halted) {
        LOG_INFO("CPU is halted. Cannot step.");
        return false;
    }
    uint32_t steps = 1;
    std::string arg;
    if (args >> arg) {
        uint64_t val = evalExpression(arg);
        if (val > 0) {
            steps = static_cast<uint32_t>(val);
        }
    }
    mState.stepsPending.fetch_add(steps, std::memory_order_release);
    mState.state.store(CpuState::Running, std::memory_order_release);
    mControl.cv.notify_all();
    return true;
}

bool Debugger::cmdPause(std::istringstream& args) {
    (void)args;
    
    if (mState.state.load(std::memory_order_acquire) == CpuState::Halted) {
        LOG_INFO("CPU is halted. Cannot pause.");
        return false;
    }

    mState.state.store(CpuState::Pause, std::memory_order_release);
    updateStatusDisplay();
    return true;
}

bool Debugger::cmdQuit(std::istringstream& args) {
    (void)args;
    mState.shouldExit.store(true, std::memory_order_release);
    mControl.cv.notify_all();
    
    if (mTerminal) {
        mTerminal->stop();
    }
    return true;
}

bool Debugger::cmdRegs(std::istringstream& args) {
    (void)args;
    printRegisters();
    return true;
}

bool Debugger::cmdMem(std::istringstream& args) {
    std::string addrStr;
    std::string lenStr;
    args >> addrStr >> lenStr;
    if (!addrStr.empty() && !lenStr.empty()) {
        uint64_t addr = evalExpression(addrStr);
        uint64_t len = evalExpression(lenStr);
        std::vector<uint8_t> data = scanMemory(addr, static_cast<uint32_t>(len));

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

bool Debugger::cmdEval(std::istringstream& args) {
    std::string expr;
    std::getline(args, expr);
    if (!expr.empty()) {
        uint64_t value = evalExpression(expr);
        LOG_INFO("0x%llx (%llu)", (unsigned long long)value, (unsigned long long)value);
        return true;
    }
    return false;
}

bool Debugger::cmdBp(std::istringstream& args) {
    std::string action;
    std::string addrStr;
    args >> action;

    if (action == "list" || action.empty()) {
        std::lock_guard<std::mutex> lock(mMutex);
        if (mBreakpoints.empty()) {
            LOG_INFO("No breakpoints.");
        } else {
            LOG_INFO("Breakpoints:");
            for (uint64_t bp : mBreakpoints) {
                LOG_INFO("  0x%llx", (unsigned long long)bp);
            }
        }
        return true;
    }

    args >> addrStr;
    if (action == "add" && !addrStr.empty()) {
        addBreakpoint(evalExpression(addrStr));
        return true;
    }
    if (action == "del" && !addrStr.empty()) {
        removeBreakpoint(evalExpression(addrStr));
        return true;
    }
    return false;
}

bool Debugger::cmdLog(std::istringstream& args) {
    std::string levelStr;
    args >> levelStr;
    std::string trimmed = toLower(levelStr);
    trimInPlace(&trimmed);

    LogLevel level = LogLevel::Info;
    bool valid = false;

    if (trimmed == "trace") { level = LogLevel::Trace; valid = true; }
    else if (trimmed == "debug") { level = LogLevel::Debug; valid = true; }
    else if (trimmed == "info") { level = LogLevel::Info; valid = true; }
    else if (trimmed == "warn") { level = LogLevel::Warn; valid = true; }
    else if (trimmed == "error") { level = LogLevel::Error; valid = true; }

    if (valid) {
        logSetLevel(level);
        LOG_INFO("Log level set to %s", levelStr.c_str());
        return true;
    }

    LOG_INFO("Usage: log [trace|debug|info|warn|error]");
    return true;
}

bool Debugger::cmdHelp(std::istringstream& args) {
    (void)args;
    LOG_INFO("Available commands:");

    size_t maxNameLen = 0;
    for (const auto& cmd : mCommands) {
        if (cmd.name.length() > maxNameLen) {
            maxNameLen = cmd.name.length();
        }
    }

    for (const auto& cmd : mCommands) {
        std::string padding(maxNameLen - cmd.name.length() + 2, ' ');
        LOG_INFO("  %s%s%s", cmd.name.c_str(), padding.c_str(), cmd.help.c_str());
    }
    return true;
}

void Debugger::updateStatusDisplay() {
    if (!mTerminal) return;

    std::string stateStr;
    CpuState s = mState.state.load(std::memory_order_acquire);
    switch (s) {
        case CpuState::Running: stateStr = "RUNNING"; break;
        case CpuState::Pause:   stateStr = "PAUSED "; break;
        case CpuState::Halted:  stateStr = "HALTED "; break;
    }

    uint64_t cycles = getCpuCycle();
    uint64_t instrs = mTotalInstructions.load(std::memory_order_acquire);
    uint64_t pc = mCpu ? mCpu->getPc() : 0;

    char buffer[256];

    double ipc = 0.0;
    if (cycles > 0) {
        ipc = static_cast<double>(instrs) / static_cast<double>(cycles);
    }

    double cps = mCurrentCPS.load(std::memory_order_acquire);

    char cpsBuf[32];
    if (cps >= 1000000.0) {
        snprintf(cpsBuf, sizeof(cpsBuf), "%.2fM", cps / 1000000.0);
    } else if (cps >= 1000.0) {
        snprintf(cpsBuf, sizeof(cpsBuf), "%.2fK", cps / 1000.0);
    } else {
        snprintf(cpsBuf, sizeof(cpsBuf), "%.0f", cps);
    }

    const char* cmdStatus = mLastCommandSuccess ? "OK" : "ERR";

    snprintf(buffer, sizeof(buffer), "CPU: %s | PC: 0x%llx | Cycles: %llu | Instrs: %llu | IPC: %.2f | CPS: %s | CMD: %s",
             stateStr.c_str(),
             (unsigned long long)pc,
             (unsigned long long)cycles,
             (unsigned long long)instrs,
             ipc,
             cpsBuf,
             cmdStatus);

    mTerminal->updateStatus(buffer);
}
