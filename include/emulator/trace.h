#ifndef EMULATOR_TRACE_H
#define EMULATOR_TRACE_H

#include <functional>
#include <string>
#include <vector>

#include "emulator/cpu.h"

struct MemAccessEvent {
    MemAccessType Type = MemAccessType::Read;
    uint64_t Address = 0;
    uint32_t Size = 0;
    uint64_t Data = 0;
    uint32_t LatencyCycles = 0;
};

struct TraceField {
    std::string Key;
    std::string Value;
};

struct TraceRecord {
    uint64_t Pc = 0;
    uint32_t Inst = 0;
    std::string Decoded;
    uint64_t CycleBegin = 0;
    uint64_t CycleEnd = 0;
    std::vector<MemAccessEvent> MemEvents;
    std::vector<TraceField> Extra;
};

class TraceSink {
public:
    using TraceHandler = std::function<void(const TraceRecord& record)>;

    TraceSink() = default;

    void OnTrace(const TraceRecord& record);
    void SetTraceHandler(TraceHandler handler);

private:
    TraceHandler Handler;
};

void AddTraceMetrics(TraceRecord* record);

#endif
