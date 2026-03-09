#include "emulator/log/trace_manager.h"

#include <cstdarg>

void TraceManager::setTraceFile(const std::string& path) {
    mTraceFile = path;
}

const std::string& TraceManager::getTraceFile() const {
    return mTraceFile;
}

void TraceManager::removeTracer(const std::string& name) {
    mTracers.erase(name);
    mEnabledSet.erase(name);
}

void TraceManager::trace(const std::string& name, const char* fmt, ...) {
    if (!isEnabled(name)) {
        return;
    }
    
    auto it = mTracers.find(name);
    if (it == mTracers.end()) {
        return;
    }
    
    va_list args;
    va_start(args, fmt);
    
    char buffer[4096];
    std::vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    
    it->second->trace("%s", buffer);
}

bool TraceManager::setEnabled(const std::string& name, bool enabled) {
    if (enabled) {
        mEnabledSet.insert(name);
    } else {
        mEnabledSet.erase(name);
    }
    return true;
}

bool TraceManager::isEnabled(const std::string& name) const {
    return mEnabledSet.find(name) != mEnabledSet.end();
}

Tracer* TraceManager::getTracer(const std::string& name) const {
    auto it = mTracers.find(name);
    if (it == mTracers.end()) {
        return nullptr;
    }
    return it->second.get();
}

std::vector<std::string> TraceManager::listTracers() const {
    std::vector<std::string> names;
    names.reserve(mTracers.size());
    for (const auto& [name, tracer] : mTracers) {
        names.push_back(name);
    }
    return names;
}

bool TraceManager::hasTracer(const std::string& name) const {
    return mTracers.find(name) != mTracers.end();
}