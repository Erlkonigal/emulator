#include "emulator/debug/command/commands.h"
#include "emulator/debug/command/command_registry.h"
#include "emulator/debug/debug_context.h"
#include "emulator/debug/expression_parser.h"
#include "emulator/log/logger.h"
#include "emulator/log/trace_manager.h"
#include "emulator/log/tracer.h"
#include "emulator/utils/utils.h"

#include <iomanip>
#include <sstream>

namespace emulator {

void RunCommand::execute(std::istringstream& args, DebugContext& ctx) {
    (void)args;
    if (ctx.state() == DebuggerState::Halted) {
        ctx.output("CPU is halted. Cannot run.");
        return;
    }
    ctx.run();
    ctx.setState(DebuggerState::Running);
}

void StepCommand::execute(std::istringstream& args, DebugContext& ctx) {
    if (ctx.state() == DebuggerState::Halted) {
        ctx.output("CPU is halted. Cannot step.");
        return;
    }
    uint32_t count = 1;
    args >> count;
    ctx.step(count);
}

void PauseCommand::execute(std::istringstream& args, DebugContext& ctx) {
    (void)args;
    if (ctx.state() == DebuggerState::Halted) {
        ctx.output("CPU is halted. Cannot pause.");
        return;
    }
    ctx.pause();
    ctx.setState(DebuggerState::Paused);
}

void QuitCommand::execute(std::istringstream& args, DebugContext& ctx) {
    (void)args;
    ctx.quit();
}

void ExitCommand::execute(std::istringstream& args, DebugContext& ctx) {
    (void)args;
    ctx.quit();
}

void RegsCommand::execute(std::istringstream& args, DebugContext& ctx) {
    (void)args;
    for (uint32_t regId = 0; regId < 32; ++regId) {
        uint64_t val = ctx.readReg(regId);
        ctx.output("r%u = 0x%llx", regId, (unsigned long long)val);
    }
}

void MemCommand::execute(std::istringstream& args, DebugContext& ctx) {
    std::string addrStr;
    std::string lenStr;
    args >> addrStr >> lenStr;
    if (addrStr.empty() || lenStr.empty()) {
        ctx.output("Usage: mem <addr> <len>");
        return;
    }

    uint64_t addr = 0;
    uint64_t len = 0;
    if (!parseU64(addrStr, &addr) || !parseU64(lenStr, &len)) {
        ctx.output("Invalid address or length");
        return;
    }

    std::string line;
    for (uint64_t i = 0; i < len; ++i) {
        if (i % 16 == 0) {
            if (!line.empty()) {
                ctx.outputLine(line);
            }
            std::ostringstream oss;
            oss << std::hex << std::setfill('0') << std::setw(8) << (addr + i) << ": ";
            line = oss.str();
        }

        uint8_t byte = ctx.readMem(addr + i);
        std::ostringstream oss;
        oss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(byte) << " ";
        line += oss.str();
    }
    if (!line.empty()) {
        ctx.outputLine(line);
    }
}

void EvalCommand::execute(std::istringstream& args, DebugContext& ctx) {
    std::string expr;
    std::getline(args, expr);
    trimInPlace(&expr);
    if (expr.empty()) {
        return;
    }
    uint64_t value = ExpressionParser::getInstance().parse(expr);
    ctx.output("0x%llx (%llu)", (unsigned long long)value, (unsigned long long)value);
}

void BpCommand::execute(std::istringstream& args, DebugContext& ctx) {
    std::string action;
    args >> action;

    if (action == "list" || action.empty()) {
        auto lines = ctx.listBreakpoints();
        if (lines.empty()) {
            ctx.output("No breakpoints.");
        } else {
            ctx.output("Breakpoints:");
            for (const auto& line : lines) {
                ctx.output("  %s", line.c_str());
            }
        }
        return;
    }

    std::string addrStr;
    args >> addrStr;
    uint64_t addr = 0;
    if (!parseU64(addrStr, &addr)) {
        ctx.output("Invalid address");
        return;
    }

    if (action == "add") {
        ctx.addBreakpoint(addr);
        ctx.output("Breakpoint added at 0x%llx", (unsigned long long)addr);
        return;
    }
    if (action == "del") {
        ctx.removeBreakpoint(addr);
        ctx.output("Breakpoint removed at 0x%llx", (unsigned long long)addr);
        return;
    }

    ctx.output("Usage: bp list|add <addr>|del <addr>");
}

void LogCommand::execute(std::istringstream& args, DebugContext& ctx) {
    std::string levelStr;
    args >> levelStr;
    std::string trimmed = toLower(levelStr);
    trimInPlace(&trimmed);

    int level = -1;
    if (trimmed == "debug") {
        level = 0;
    } else if (trimmed == "info") {
        level = 1;
    } else if (trimmed == "warn") {
        level = 2;
    } else if (trimmed == "error") {
        level = 3;
    }

    if (level >= 0) {
        ctx.setLogLevel(level);
        ctx.output("Log level set to %s", levelStr.c_str());
        return;
    }

    ctx.output("Usage: log [debug|info|warn|error]");
}

void TraceCommand::execute(std::istringstream& args, DebugContext& ctx) {
    std::string action;
    args >> action;

if (action == "list") {
        auto names = TraceManager::getInstance().listTracers();
        if (names.empty()) {
            ctx.output("No tracers registered.");
        } else {
            ctx.output("Registered tracers:");
            for (const auto& name : names) {
                const char* status = TraceManager::getInstance().isEnabled(name) ? "ON" : "OFF";
                ctx.output("  %s [%s]", name.c_str(), status);
            }
        }
        return;
    }

    if (action != "on" && action != "off") {
        ctx.output("Usage: trace on|off <name>");
        ctx.output("       trace list");
        return;
    }

    std::string traceName;
    args >> traceName;
    if (traceName.empty()) {
        ctx.output("Usage: trace %s <name>", action.c_str());
        return;
    }

    if (!TraceManager::getInstance().hasTracer(traceName)) {
        ctx.output("Unknown tracer: %s", traceName.c_str());
        ctx.output("Use 'trace list' to see available tracers.");
        return;
    }

    bool enable = (action == "on");
    TraceManager::getInstance().setEnabled(traceName, enable);
    ctx.output("Tracer '%s' %s", traceName.c_str(), enable ? "enabled" : "disabled");
}

void HelpCommand::execute(std::istringstream& args, DebugContext& ctx) {
    (void)args;
    ctx.output("Available commands:");

    auto commands = CommandRegistry::getInstance().getCommandHelp();
    size_t maxNameLen = 0;
    for (const auto& [name, help] : commands) {
        if (name.length() > maxNameLen) {
            maxNameLen = name.length();
        }
    }

    for (const auto& [name, help] : commands) {
        std::string padding(maxNameLen - name.length() + 2, ' ');
        ctx.output("  %s%s%s", name.c_str(), padding.c_str(), help.c_str());
    }
}

void registerAllCommands() {
    auto& registry = CommandRegistry::getInstance();
    registry.registerCommand(std::make_unique<RunCommand>());
    registry.registerCommand(std::make_unique<StepCommand>());
    registry.registerCommand(std::make_unique<PauseCommand>());
    registry.registerCommand(std::make_unique<QuitCommand>());
    registry.registerCommand(std::make_unique<ExitCommand>());
    registry.registerCommand(std::make_unique<RegsCommand>());
    registry.registerCommand(std::make_unique<MemCommand>());
    registry.registerCommand(std::make_unique<EvalCommand>());
    registry.registerCommand(std::make_unique<BpCommand>());
    registry.registerCommand(std::make_unique<LogCommand>());
    registry.registerCommand(std::make_unique<TraceCommand>());
    registry.registerCommand(std::make_unique<HelpCommand>());
}

} // namespace emulator