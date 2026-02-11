#include "emulator/debugger/debugger.h"
#include "emulator/device/device.h"
#include "emulator/device/uart.h"
#include "emulator/device/display.h"
#include "emulator/app/app.h"
#include "emulator/app/utils.h"
#include "emulator/debugger/expression_parser.h"
#include "emulator/app/app.h"
#include "emulator/logging/logger.h"

#include "emulator/app/terminal.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
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
    constexpr size_t kReadBufferSize = 64;
    constexpr int kPollTimeoutMs = 10;

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
} // namespace

constexpr uint32_t kInstructionsPerBatch = 1000;
constexpr auto kPresentInterval = std::chrono::milliseconds(16);

Debugger::Debugger(ICpuExecutor* cpu, MemoryBus* bus)
    : mCpu(cpu), mBus(bus) {
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

void Debugger::configureTrace(const EmulatorConfig* config) {
    if (!config) return;
    mEnableITrace = config->iTrace;
    mEnableMTrace = config->mTrace;
    mEnableBpTrace = config->bpTrace;
}

void Debugger::initDbgArchFromCpu() {
    if (mCpu == nullptr) return;

    mDbgArch.pc = mCpu->getPc();
    mDbgArch.cycle = mCpu->getCycle();
    mDbgArch.regs.resize(mCpu->getRegisterCount());
    for (uint32_t i = 0; i < mCpu->getRegisterCount(); ++i) {
        mDbgArch.regs[i] = mCpu->getRegister(i);
    }
    mDbgArch.mem.bytes.clear();
}

void Debugger::updateDbgArch(const CommitInfo& commit) {
    mDbgArch.pc = commit.pc;

    for (const auto& regEvt : commit.regEvents) {
        if (regEvt.regId < mDbgArch.regs.size()) {
            mDbgArch.regs[regEvt.regId] = regEvt.newValue;
        }
    }

    for (const auto& memEvt : commit.memEvents) {
        if (memEvt.type == MemAccessType::Write) {
            for (uint32_t i = 0; i < memEvt.size; ++i) {
                uint64_t addr = memEvt.address + i;
                mDbgArch.mem.bytes[addr] = static_cast<uint8_t>((memEvt.data >> (i * 8)) & 0xff);
            }
        }
    }

    mDbgArch.cycle++;
}

void Debugger::flushTraceBatch() {
    if (mBatchCommits.empty()) return;
    if (!(mEnableITrace || mEnableMTrace || mEnableBpTrace)) return;

    for (const auto& commit : mBatchCommits) {
        std::string line = formatTrace(commit);
        if (!line.empty()) {
            TRACE("%s", line.c_str());
        }
    }
}

void Debugger::flushTrace() {
    flushTraceBatch();
}

std::string Debugger::formatTrace(const CommitInfo& commit) {
    std::stringstream ss;

    if (mEnableITrace) {
        ss << "PC:0x" << std::hex << std::setw(8) << std::setfill('0') << commit.pc << " ";
        ss << "Inst:0x" << std::hex << std::setw(8) << std::setfill('0') << commit.inst << " ";
        if (!commit.decoded.empty()) {
            ss << "(" << commit.decoded << ")";
        }
        ss << " ";
    }

    if (mEnableBpTrace && commit.isBranch) {
        ss << "BP:(T:" << (commit.branch.taken ? "1" : "0") << " "
           << "P:" << (commit.branch.predictedTaken ? "1" : "0") << " "
           << "Target:0x" << std::hex << commit.branch.target << " "
           << "PTarget:0x" << std::hex << commit.branch.predictedTarget << ")";
        ss << " ";
    }

    if (mEnableMTrace && !commit.memEvents.empty()) {
        ss << "Mem:[";
        bool first = true;
        for (const auto& event : commit.memEvents) {
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

void Debugger::setSdl(SdlDisplayDevice* sdl) {
    mSdl = sdl;
}

void Debugger::setRegisterCount(uint32_t count) {
    mRegisterCount = count;
}

void Debugger::run(bool interactive) {
    mRunHadError = false;
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

        updateStatusDisplay();
        setTerminalLogHandler();
    } else {
        setDefaultLogHandler();
    }

    std::thread cpuThread(&Debugger::cpuThreadLoop, this);
    std::thread sdlThread;

    if (mSdl) {
        sdlThread = std::thread(&Debugger::sdlThreadLoop, this);
    }

    if (interactive) {
        mTerminal->runCursesInputLoop();
    } else {
        runPlainInputLoop();
    }

    mState.state.store(CpuState::Halted, std::memory_order_release);
    mControl.cv.notify_all();

    if (cpuThread.joinable()) cpuThread.join();
    if (sdlThread.joinable()) sdlThread.join();

    if (mSdl) {
        mSdl->shutdown();
    }

    mTerminal.reset();
}

void Debugger::setTerminalLogHandler() {
    if (!mTerminal) return;

    auto outputHandler = [this](const char* msg) {
        mTerminal->printLog(msg);
    };

    auto deviceHandler = [this](const char* msg) {
        for (auto c : std::string(msg)) {
            mTerminal->printChar(c);
        }
    };
    logging::setOutputHandler(outputHandler, deviceHandler);
}

void Debugger::setDefaultLogHandler() {
    auto logHandler = [](const char* msg) {
        fprintf(stderr, "%s", msg);
        fflush(stderr);
    };
    auto deviceHandler = [](const char* msg) {
        fprintf(stdout, "%s", msg);
        fflush(stdout);
    };
    logging::setOutputHandler(logHandler, deviceHandler);
}

void Debugger::cpuThreadLoop() {
    initDbgArchFromCpu();

    if (mCpu == nullptr || mBus == nullptr) return;

    while (true) {
        uint32_t steps = 0;
        bool stepping = false;
        {
            std::unique_lock<std::mutex> lock(mControl.mutex);
            mControl.cv.wait(lock, [&]() {
                CpuState current = mState.state.load(std::memory_order_acquire);
                uint32_t pending = mState.stepsPending.load(std::memory_order_acquire);
                return current == CpuState::Halted ||
                    current == CpuState::Running ||
                    pending > 0;
            });

            auto state = mState.state.load(std::memory_order_acquire);
            if (state == CpuState::Halted) break;
            
            uint32_t pending = mState.stepsPending.load(std::memory_order_acquire);
            if (pending > 0) {
                steps = pending;
                mState.stepsPending.store(0, std::memory_order_release);
                stepping = true;
                if (state != CpuState::Running) {
                    mState.state.store(CpuState::Running, std::memory_order_release);
                }
            } else if (state == CpuState::Running) {
                steps = kInstructionsPerBatch;
            }
        }

        if (steps == 0) continue;

        mBatchCommits.clear();
        bool halted = false;

        for (uint32_t i = 0; i < steps && !halted; ++i) {
            CycleResult result;
            mCpu->cycle(result);

            for (const auto& commit : result.commits) {
                updateDbgArch(commit);

                if (isBreakpoint(commit.pc)) {
                    mState.state.store(CpuState::Pause, std::memory_order_release);
                    halted = true;
                    break;
                }
            }

            mBatchCommits.insert(mBatchCommits.end(),
                                  result.commits.begin(),
                                  result.commits.end());

            if (!result.errors.empty()) {
                mState.state.store(CpuState::Halted, std::memory_order_release);
                for (const auto& err : result.errors) {
                    if (err.type != CpuErrorType::None) {
                        mRunHadError = true;
                        ERROR("Error: Type=%d PC=0x%llx %s",
                                static_cast<int>(err.type),
                                (unsigned long long)err.pc,
                                err.message.c_str());
                    }
                }
                halted = true;
            }
        }

        flushTraceBatch();

        mBus->syncAll(mCpu->getCycle());

        if (stepping && mState.state.load(std::memory_order_acquire) != CpuState::Halted) {
            mState.state.store(CpuState::Pause, std::memory_order_release);
        }

        updateStatusDisplay();
    }
}

void Debugger::sdlThreadLoop() {
    if (mSdl == nullptr) {
        return;
    }
    auto lastPresent = std::chrono::steady_clock::now();
    while (mState.state.load(std::memory_order_acquire) != CpuState::Halted) {
        bool shouldWait = !mSdl->isDirty() && !mSdl->isPresentRequested();
        mSdl->pollEvents(shouldWait ? 8u : 0u);
        if (mSdl->isQuitRequested()) {
            mState.state.store(CpuState::Halted, std::memory_order_release);
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

void Debugger::runPlainInputLoop() {
    if (mBus == nullptr) {
        ERROR("Memory bus not initialized");
        return;
    }

    UartDevice* uart = static_cast<UartDevice*>(mBus->getDevice("UART"));
    if (uart == nullptr) {
        ERROR("UART device not found");
        return;
    }

    struct termios originalSettings{};
    bool isTty = isatty(STDIN_FILENO);

    if (isTty && tcgetattr(STDIN_FILENO, &originalSettings) != 0) {
        ERROR("Failed to get terminal attributes: %s", strerror(errno));
        return;
    }

    struct termios newSettings = originalSettings;
    newSettings.c_lflag &= ~(ICANON | ECHO);

    TermiosGuard guard(STDIN_FILENO, newSettings);
    if (!guard.isValid()) {
        return;
    }

    while (mState.state.load(std::memory_order_acquire) != CpuState::Halted) {
        if (mState.state.load(std::memory_order_acquire) == CpuState::Halted) {
            break;
        }

        struct pollfd pfd;
        pfd.fd = STDIN_FILENO;
        pfd.events = POLLIN;

        int ret = poll(&pfd, 1, kPollTimeoutMs);

        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            ERROR("Poll failed: %s", strerror(errno));
            break;
        }

        if (ret == 0) {
            continue;
        }

        if (pfd.revents & POLLIN) {
            std::array<char, kReadBufferSize> buffer{};
            ssize_t bytesRead = read(STDIN_FILENO, buffer.data(), buffer.size());

            if (bytesRead < 0) {
                if (errno == EINTR || errno == EAGAIN) {
                    continue;
                }
                ERROR("Read failed: %s", strerror(errno));
                break;
            }

            for (ssize_t i = 0; i < bytesRead; ++i) {
                uart->pushRx(static_cast<uint8_t>(buffer[i]));
            }
        }

        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
            ERROR("Terminal error event: 0x%x", pfd.revents);
            break;
        }
    }
}

std::vector<uint8_t> Debugger::scanMemory(uint64_t address, uint32_t length) {
    std::vector<uint8_t> data;
    if (length == 0) {
        return data;
    }

    data.resize(length);
    for (uint32_t i = 0; i < length; ++i) {
        auto it = mDbgArch.mem.bytes.find(address + i);
        data[i] = (it != mDbgArch.mem.bytes.end()) ? it->second : 0;
    }
    return data;
}

std::vector<uint64_t> Debugger::readRegisters() {
    std::vector<uint64_t> regs;
    if (mRegisterCount == 0) {
        if (mCpu) {
            mRegisterCount = mCpu->getRegisterCount();
        } else {
            return regs;
        }
    }
    regs.resize(mRegisterCount);
    for (uint32_t regId = 0; regId < mRegisterCount; ++regId) {
        regs[regId] = getDbgArch().regs[regId];
    }
    return regs;
}

void Debugger::printRegisters() {
    std::vector<uint64_t> regs = readRegisters();
    for (uint32_t regId = 0; regId < regs.size(); ++regId) {
        INFO("r%u = 0x%llx", regId, (unsigned long long)regs[regId]);
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
        INFO("CPU is halted. Cannot run.");
        return false;
    }
    mState.state.store(CpuState::Running, std::memory_order_release);
    mControl.cv.notify_all();
    return true;
}

bool Debugger::cmdStep(std::istringstream& args) {
    if (mState.state.load(std::memory_order_acquire) == CpuState::Halted) {
        INFO("CPU is halted. Cannot step.");
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
        INFO("CPU is halted. Cannot pause.");
        return false;
    }

    mState.state.store(CpuState::Pause, std::memory_order_release);
    updateStatusDisplay();
    return true;
}

bool Debugger::cmdQuit(std::istringstream& args) {
    (void)args;
    mState.state.store(CpuState::Halted, std::memory_order_release);
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
                INFO("%s", line.c_str());
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
        INFO("0x%llx (%llu)", (unsigned long long)value, (unsigned long long)value);
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
            INFO("No breakpoints.");
        } else {
            INFO("Breakpoints:");
            for (uint64_t bp : mBreakpoints) {
                INFO("  0x%llx", (unsigned long long)bp);
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

    logging::Level level = logging::Level::Info;
    bool valid = false;

    if (trimmed == "trace") { level = logging::Level::Trace; valid = true; }
    else if (trimmed == "debug") { level = logging::Level::Debug; valid = true; }
    else if (trimmed == "info") { level = logging::Level::Info; valid = true; }
    else if (trimmed == "warn") { level = logging::Level::Warn; valid = true; }
    else if (trimmed == "error") { level = logging::Level::Error; valid = true; }

    if (valid) {
        logging::level(level);
        INFO("Log level set to %s", levelStr.c_str());
        return true;
    }

    INFO("Usage: log [trace|debug|info|warn|error]");
    return true;
}

bool Debugger::cmdHelp(std::istringstream& args) {
    (void)args;
    INFO("Available commands:");

    size_t maxNameLen = 0;
    for (const auto& cmd : mCommands) {
        if (cmd.name.length() > maxNameLen) {
            maxNameLen = cmd.name.length();
        }
    }

    for (const auto& cmd : mCommands) {
        std::string padding(maxNameLen - cmd.name.length() + 2, ' ');
        INFO("  %s%s%s", cmd.name.c_str(), padding.c_str(), cmd.help.c_str());
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

    uint64_t cycles = mCpu ? mCpu->getCycle() : 0;
    uint64_t instrs = mTotalInstructions;
    uint64_t pc = mCpu ? mCpu->getPc() : 0;

    char buffer[256];

    double ipc = 0.0;
    if (cycles > 0) {
        ipc = static_cast<double>(instrs) / static_cast<double>(cycles);
    }

    double cps = 0;
    auto now = std::chrono::steady_clock::now();
    double dt = std::chrono::duration<double>(now - mLastCpsTime).count();
    if (dt > 0) {
        double dCycles = static_cast<double>(cycles - mLastCpsCycles);
        cps = dCycles / dt;
    }
    mLastCpsTime = now;
    mLastCpsCycles = cycles;

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
