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
    std::mutex Mutex;
    std::condition_variable Cv;
};

struct EmulatorRunState {
    std::atomic<CpuState> State{CpuState::Pause};
    std::atomic<bool> ShouldExit{false};
    std::atomic<uint32_t> StepsPending{0};
};

class Debugger : public ICpuDebugger {
public:
    Debugger(ICpuExecutor* cpu, MemoryBus* bus);
    ~Debugger() override;

    void SetSdl(SdlDisplayDevice* sdl);
    void Run(bool interactive);

    MemResponse BusRead(const MemAccess& access) override;
    MemResponse BusWrite(const MemAccess& access) override;
    uint64_t GetCpuCycle();

    std::vector<uint8_t> ScanMemory(uint64_t address, uint32_t length);
    std::vector<uint64_t> ReadRegisters();
    void PrintRegisters();
    uint64_t EvalExpression(const std::string& expression);
    void AddBreakpoint(uint64_t address);
    void RemoveBreakpoint(uint64_t address);
    bool IsBreakpoint(uint64_t address) override;
    bool HasBreakpoints() override;
    bool ProcessCommand(const std::string& command);

    void SetRegisterCount(uint32_t count);
    void SetCpuFrequency(uint32_t cpuFreq);

    void ConfigureTrace(const TraceOptions& options) override;
    void SetTraceFormatter(TraceFormatter formatter) override;
    void LogTrace(const TraceRecord& record) override;
    const TraceOptions& GetTraceOptions() const override;

private:
    ICpuExecutor* Cpu = nullptr;
    MemoryBus* Bus = nullptr;
    SdlDisplayDevice* Sdl = nullptr;
    uint32_t RegisterCount = 0;
    uint32_t CpuFrequency = 1000000;
    uint32_t SyncThresholdCycles = 1000;
    std::vector<uint64_t> Breakpoints;
    std::mutex Mutex;

    EmulatorRunState State;
    CpuControl Control;
    bool IsInteractive = false;

    struct CommandEntry {
        std::string Name;
        std::string Help;
        bool (Debugger::*Handler)(std::istringstream&);
    };
    std::vector<CommandEntry> Commands;
    void RegisterCommands();

    void CpuThreadLoop();
    void SdlThreadLoop();
    void InputLoop();

    void SetupUart();
    void SetupLogging();

    bool CmdRun(std::istringstream& args);
    bool CmdStep(std::istringstream& args);
    bool CmdPause(std::istringstream& args);
    bool CmdQuit(std::istringstream& args);
    bool CmdRegs(std::istringstream& args);
    bool CmdMem(std::istringstream& args);
    bool CmdEval(std::istringstream& args);
    bool CmdBp(std::istringstream& args);
    bool CmdLog(std::istringstream& args);
    bool CmdHelp(std::istringstream& args);

    void UpdateStatusDisplay();

    std::unique_ptr<Terminal> m_Terminal;
    std::atomic<uint64_t> m_TotalInstructions{0};
    bool m_LastStepSuccess = true;

    TraceOptions m_TraceOptions;
    TraceFormatter m_TraceFormatter;

    std::atomic<double> m_CurrentCPS{0.0};
    std::chrono::steady_clock::time_point m_LastCpsTime;
    uint64_t m_LastCpsCycles = 0;
};

#endif
