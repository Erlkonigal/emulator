#ifndef EMULATOR_DEBUGGER_DEBUGGER_H
#define EMULATOR_DEBUGGER_DEBUGGER_H

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>
#include <memory>

#include "emulator/bus/bus.h"
#include "emulator/cpu/cpu.h"

class SdlDisplayDevice;
class Terminal;
enum class FocusPanel;

struct CpuControl {
    std::mutex mutex;
    std::condition_variable cv;
};

struct EmulatorRunState {
    std::atomic<CpuState> state{CpuState::Pause};
    std::atomic<bool> shouldExit{false};
    std::atomic<uint32_t> stepsPending{0};
};

class Debugger : public ICpuDebugger {
public:
    Debugger(ICpuExecutor* cpu, MemoryBus* bus);
    ~Debugger() override;

    void setSdl(SdlDisplayDevice* sdl);
    void run(bool interactive);

    MemResponse busRead(const MemAccess& access) override;
    MemResponse busWrite(const MemAccess& access) override;

    std::vector<uint8_t> scanMemory(uint64_t address, uint32_t length);
    std::vector<uint64_t> readRegisters();
    void printRegisters();
    uint64_t evalExpression(const std::string& expression);
    void addBreakpoint(uint64_t address);
    void removeBreakpoint(uint64_t address);
    bool isBreakpoint(uint64_t address) override;
    bool hasBreakpoints() override;
    bool processCommand(const std::string& command);

    void setRegisterCount(uint32_t count);
    void setCpuFrequency(uint32_t cpuFreq);

    void configureTrace(const TraceOptions& options) override;
    void setTraceFormatter(TraceFormatter formatter) override;
    void logTrace(const TraceRecord& record) override;
    const TraceOptions& getTraceOptions() const override;

private:
    ICpuExecutor* mCpu = nullptr;
    MemoryBus* mBus = nullptr;
    SdlDisplayDevice* mSdl = nullptr;
    uint32_t mRegisterCount = 0;
    uint32_t mCpuFrequency = 1000000;
    uint32_t mSyncThresholdCycles = 1000;
    std::vector<uint64_t> mBreakpoints;
    std::mutex mMutex;

    EmulatorRunState mState;
    CpuControl mControl;
    bool mIsInteractive = false;

    struct CommandEntry {
        std::string name;
        std::string help;
        bool (Debugger::*Handler)(std::istringstream&);
    };
    std::vector<CommandEntry> mCommands;
    void registerCommands();

    void cpuThreadLoop();
    void sdlThreadLoop();
    void runPlainInputLoop();

    void setupUart();
    void setupLogging();

    bool cmdRun(std::istringstream& args);
    bool cmdStep(std::istringstream& args);
    bool cmdPause(std::istringstream& args);
    bool cmdQuit(std::istringstream& args);
    bool cmdRegs(std::istringstream& args);
    bool cmdMem(std::istringstream& args);
    bool cmdEval(std::istringstream& args);
    bool cmdBp(std::istringstream& args);
    bool cmdLog(std::istringstream& args);
    bool cmdHelp(std::istringstream& args);

    void updateStatusDisplay();

    std::unique_ptr<Terminal> mTerminal;
    uint64_t mTotalInstructions = 0;
    bool mLastCommandSuccess = true;

    TraceOptions mTraceOptions;
    TraceFormatter mTraceFormatter;

    std::chrono::steady_clock::time_point mLastCpsTime;
    uint64_t mLastCpsCycles = 0;
};

#endif
