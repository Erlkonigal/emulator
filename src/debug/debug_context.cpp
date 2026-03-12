#include "emulator/debug/debug_context.h"
#include "emulator/debug/breakpoint.h"
#include "emulator/commit/commit_thread.h"
#include "emulator/commit/shadow_arch.h"
#include "emulator/log/logger.h"
#include "emulator/log/trace_manager.h"
#include "emulator/generated/hardware_config.h"

#include <cstdarg>

namespace emulator {

void DebugContext::reset() {
    mState.store(DebuggerState::Idle, std::memory_order_release);
    mQuitRequested.store(false, std::memory_order_release);
    BreakPointController::getInstance().reset();
}

void DebugContext::output(const char* fmt, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    if (mOutputHandler) {
        mOutputHandler(std::string(buffer) + "\n");
    }
}

void DebugContext::outputLine(const std::string& line) {
    if (mOutputHandler) {
        mOutputHandler(line + "\n");
    }
}

bool DebugContext::run() {
    if (mOnRun) {
        return mOnRun();
    }
    return false;
}

bool DebugContext::step(uint32_t count) {
    if (mOnStep) {
        return mOnStep(count);
    }
    return false;
}

bool DebugContext::pause() {
    if (mOnPause) {
        return mOnPause();
    }
    return false;
}

bool DebugContext::quit() {
    mQuitRequested.store(true, std::memory_order_release);
    if (mOnQuit) {
        return mOnQuit();
    }
    return true;
}

uint64_t DebugContext::readReg(uint32_t id) {
    return ShadowArch::getInstance().readReg(id);
}

uint8_t DebugContext::readMem(uint64_t addr) {
    return ShadowArch::getInstance().readMem(addr);
}

uint64_t DebugContext::readPc() {
    return ShadowArch::getInstance().readPc();
}

void DebugContext::addBreakpoint(uint64_t addr) {
    BreakPointController::getInstance().add(addr);
}

void DebugContext::removeBreakpoint(uint64_t addr) {
    BreakPointController::getInstance().remove(addr);
}

bool DebugContext::hasBreakpoint(uint64_t addr) {
    return BreakPointController::getInstance().contains(addr);
}

std::vector<std::string> DebugContext::listBreakpoints() {
    std::vector<std::string> result;
    BreakPointController::getInstance().list(result);
    return result;
}

void DebugContext::setLogLevel(int level) {
    Logger::Level lvl = Logger::Level::Info;
    switch (level) {
        case 0: lvl = Logger::Level::Debug; break;
        case 1: lvl = Logger::Level::Info; break;
        case 2: lvl = Logger::Level::Warn; break;
        case 3: lvl = Logger::Level::Error; break;
        default: break;
    }
    Logger::getInstance().setLevel(lvl);
}

void DebugContext::setTraceEnabled(const std::string& name, bool enabled) {
    TraceManager::getInstance().setEnabled(name, enabled);
}

} // namespace emulator