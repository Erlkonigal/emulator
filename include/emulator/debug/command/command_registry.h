#pragma once

#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "emulator/debug/command/command.h"
#include "emulator/utils/singleton.h"

namespace emulator {

class DebugContext;

class CommandRegistry : public Singleton<CommandRegistry> {
public:
    void registerCommand(std::unique_ptr<ICommand> cmd);
    bool execute(const std::string& input, DebugContext& ctx);
    std::vector<std::pair<std::string, std::string>> getCommandHelp() const;

    void reset();

private:
    std::unordered_map<std::string, std::unique_ptr<ICommand>> mCommands;

    CommandRegistry() = default;
    friend class Singleton<CommandRegistry>;
};

} // namespace emulator