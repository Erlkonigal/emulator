#include "emulator/debug/debugger.h"
#include "emulator/debug/command/command_registry.h"
#include "emulator/debug/command/commands.h"

namespace emulator {

Debugger::Debugger() {
    registerAllCommands();
}

void Debugger::reset() {
    mContext.reset();
    mHadError.store(false, std::memory_order_release);
}

void Debugger::processCommand(const std::string& command) {
    CommandRegistry::getInstance().execute(command, mContext);
}

} // namespace emulator