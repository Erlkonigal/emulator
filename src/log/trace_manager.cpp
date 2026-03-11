#include "emulator/log/trace_manager.h"

#include <cstdarg>

void TraceManager::setTraceFile(const std::string& path) {
    mTraceFile = path;
}

const std::string& TraceManager::getTraceFile() const {
    return mTraceFile;
}

void TraceManager::removeTracer(std::string_view name) {
    std::string key(name);
    mTracers.erase(key);
    mEnabledSet.erase(key);
}

void TraceManager::trace(std::string_view name, const char* fmt, ...) {
    auto it = mTracers.find(std::string(name));
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

bool TraceManager::setEnabled(std::string_view name, bool enabled) {
    std::string key(name);
    if (enabled) {
        mEnabledSet.insert(key);
    } else {
        mEnabledSet.erase(key);
    }
    return true;
}

bool TraceManager::isEnabled(std::string_view name) const {
    return mEnabledSet.find(std::string(name)) != mEnabledSet.end();
}

Tracer* TraceManager::getTracer(std::string_view name) const {
    auto it = mTracers.find(std::string(name));
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

bool TraceManager::hasTracer(std::string_view name) const {
    return mTracers.find(std::string(name)) != mTracers.end();
}