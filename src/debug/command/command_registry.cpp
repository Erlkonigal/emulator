#include "emulator/debug/command/command_registry.h"
#include "emulator/debug/debug_context.h"
#include "emulator/log/logger.h"
#include "emulator/utils/utils.h"

namespace emulator {

void CommandRegistry::registerCommand(std::unique_ptr<ICommand> cmd) {
    if (cmd) {
        mCommands[cmd->name()] = std::move(cmd);
    }
}

bool CommandRegistry::execute(const std::string& input, DebugContext& ctx) {
    std::string trimmed = input;
    trimInPlace(&trimmed);
    if (trimmed.empty()) {
        return true;
    }

    std::istringstream stream(trimmed);
    std::string verb;
    stream >> verb;

    auto it = mCommands.find(verb);
    if (it != mCommands.end()) {
        it->second->execute(stream, ctx);
        return true;
    }

    ctx.output("Unknown command: %s", verb.c_str());
    return false;
}

std::vector<std::pair<std::string, std::string>> CommandRegistry::getCommandHelp() const {
    std::vector<std::pair<std::string, std::string>> result;
    for (const auto& [name, cmd] : mCommands) {
        result.emplace_back(name, cmd->help());
    }
    return result;
}

void CommandRegistry::reset() {
    mCommands.clear();
}

} // namespace emulator