#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "emulator/log/tracer.h"
#include "emulator/utils/singleton.h"

class TraceManager : public Singleton<TraceManager> {
public:
    void setTraceFile(const std::string& path);
    const std::string& getTraceFile() const;
    
    template<typename T>
    T* createTracer(const std::string& name);
    
    void removeTracer(const std::string& name);
    
    void trace(const std::string& name, const char* fmt, ...);
    bool setEnabled(const std::string& name, bool enabled);
    bool isEnabled(const std::string& name) const;
    Tracer* getTracer(const std::string& name) const;
    std::vector<std::string> listTracers() const;
    bool hasTracer(const std::string& name) const;

private:
    std::unordered_map<std::string, std::unique_ptr<Tracer>> mTracers;
    std::unordered_set<std::string> mEnabledSet;
    std::string mTraceFile;
    
    friend class Singleton<TraceManager>;
    TraceManager() = default;
};

template<typename T>
T* TraceManager::createTracer(const std::string& name) {
    static_assert(std::is_base_of_v<Tracer, T>, "T must derive from Tracer");
    
    if (name.empty()) {
        return nullptr;
    }
    
    auto tracer = std::make_unique<T>();
    tracer->init({.name = name});
    
    T* rawPtr = tracer.get();
    mTracers[name] = std::move(tracer);
    
    return rawPtr;
}