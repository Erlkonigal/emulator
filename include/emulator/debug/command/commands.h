#pragma once

#include "emulator/debug/command/command.h"

namespace emulator {

class RunCommand : public ICommand {
public:
    std::string name() const override { return "run"; }
    std::string help() const override { return "Resume execution"; }
    void execute(std::istringstream& args, DebugContext& ctx) override;
};

class StepCommand : public ICommand {
public:
    std::string name() const override { return "step"; }
    std::string help() const override { return "Execute N instructions (default 1)"; }
    void execute(std::istringstream& args, DebugContext& ctx) override;
};

class PauseCommand : public ICommand {
public:
    std::string name() const override { return "pause"; }
    std::string help() const override { return "Pause execution"; }
    void execute(std::istringstream& args, DebugContext& ctx) override;
};

class QuitCommand : public ICommand {
public:
    std::string name() const override { return "quit"; }
    std::string help() const override { return "Exit the emulator"; }
    void execute(std::istringstream& args, DebugContext& ctx) override;
};

class ExitCommand : public ICommand {
public:
    std::string name() const override { return "exit"; }
    std::string help() const override { return "Exit the emulator"; }
    void execute(std::istringstream& args, DebugContext& ctx) override;
};

class RegsCommand : public ICommand {
public:
    std::string name() const override { return "regs"; }
    std::string help() const override { return "Print register values"; }
    void execute(std::istringstream& args, DebugContext& ctx) override;
};

class MemCommand : public ICommand {
public:
    std::string name() const override { return "mem"; }
    std::string help() const override { return "Dump memory (mem <addr> <len>)"; }
    void execute(std::istringstream& args, DebugContext& ctx) override;
};

class EvalCommand : public ICommand {
public:
    std::string name() const override { return "eval"; }
    std::string help() const override { return "Evaluate an expression (eval <expr>)"; }
    void execute(std::istringstream& args, DebugContext& ctx) override;
};

class BpCommand : public ICommand {
public:
    std::string name() const override { return "bp"; }
    std::string help() const override { return "Manage breakpoints (bp list|add <addr>|del <addr>)"; }
    void execute(std::istringstream& args, DebugContext& ctx) override;
};

class LogCommand : public ICommand {
public:
    std::string name() const override { return "log"; }
    std::string help() const override { return "Set log level (log debug|info|warn|error)"; }
    void execute(std::istringstream& args, DebugContext& ctx) override;
};

class TraceCommand : public ICommand {
public:
    std::string name() const override { return "trace"; }
    std::string help() const override { return "Manage tracers (trace on|off <name>|list)"; }
    void execute(std::istringstream& args, DebugContext& ctx) override;
};

class HelpCommand : public ICommand {
public:
    std::string name() const override { return "help"; }
    std::string help() const override { return "Show this help message"; }
    void execute(std::istringstream& args, DebugContext& ctx) override;
};

void registerAllCommands();

} // namespace emulator