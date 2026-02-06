#include "emulator/trace.h"

#include <cctype>
#include <cstdio>
#include <sstream>

namespace {
uint64_t AccumulateLatency(const TraceRecord& record) {
    uint64_t total = 0;
    for (const auto& event : record.MemEvents) {
        total += event.LatencyCycles;
    }
    return total;
}

size_t CountAccesses(const TraceRecord& record, MemAccessType type) {
    size_t count = 0;
    for (const auto& event : record.MemEvents) {
        if (event.Type == type) {
            ++count;
        }
    }
    return count;
}

void AppendField(std::vector<TraceField>* fields, const std::string& key,
    const std::string& value) {
    if (fields == nullptr) {
        return;
    }
    TraceField field;
    field.Key = key;
    field.Value = value;
    fields->push_back(field);
}

bool IsHex(const std::string& text) {
    if (text.size() < 3 || text[0] != '0' || (text[1] != 'x' && text[1] != 'X')) {
        return false;
    }
    for (size_t i = 2; i < text.size(); ++i) {
        if (!std::isxdigit(static_cast<unsigned char>(text[i]))) {
            return false;
        }
    }
    return true;
}

uint64_t ParseHex(const std::string& text) {
    uint64_t value = 0;
    for (size_t i = 2; i < text.size(); ++i) {
        char ch = text[i];
        value <<= 4;
        if (ch >= '0' && ch <= '9') {
            value |= static_cast<uint64_t>(ch - '0');
        } else if (ch >= 'a' && ch <= 'f') {
            value |= static_cast<uint64_t>(ch - 'a' + 10);
        } else if (ch >= 'A' && ch <= 'F') {
            value |= static_cast<uint64_t>(ch - 'A' + 10);
        }
    }
    return value;
}

uint64_t ParseDec(const std::string& text) {
    uint64_t value = 0;
    for (char ch : text) {
        if (ch < '0' || ch > '9') {
            break;
        }
        value = value * 10 + static_cast<uint64_t>(ch - '0');
    }
    return value;
}

uint64_t ParseNumber(const std::string& text) {
    if (IsHex(text)) {
        return ParseHex(text);
    }
    return ParseDec(text);
}

std::string FormatAccessType(MemAccessType type) {
    switch (type) {
    case MemAccessType::Read:
        return "read";
    case MemAccessType::Write:
        return "write";
    case MemAccessType::Fetch:
        return "fetch";
    }
    return "unknown";
}
}

void TraceSink::OnTrace(const TraceRecord& record) {
    if (Handler) {
        Handler(record);
    }
}

void TraceSink::SetTraceHandler(TraceHandler handler) {
    Handler = std::move(handler);
}

void AddTraceMetrics(TraceRecord* record) {
    if (record == nullptr) {
        return;
    }
    std::stringstream ss;
    ss << AccumulateLatency(*record);
    AppendField(&record->Extra, "mem_latency", ss.str());
    ss.str(std::string());
    ss.clear();
    ss << CountAccesses(*record, MemAccessType::Read);
    AppendField(&record->Extra, "mem_reads", ss.str());
    ss.str(std::string());
    ss.clear();
    ss << CountAccesses(*record, MemAccessType::Write);
    AppendField(&record->Extra, "mem_writes", ss.str());
    ss.str(std::string());
    ss.clear();
    ss << CountAccesses(*record, MemAccessType::Fetch);
    AppendField(&record->Extra, "mem_fetches", ss.str());

    for (const auto& field : record->Extra) {
        if (field.Key != "pc") {
            continue;
        }
        uint64_t pc = ParseNumber(field.Value);
        if (pc != 0) {
            std::stringstream pcStream;
            pcStream << "0x" << std::hex << pc;
            AppendField(&record->Extra, "pc_norm", pcStream.str());
        }
        break;
    }

    for (const auto& event : record->MemEvents) {
        std::stringstream addr;
        addr << "0x" << std::hex << event.Address;
        AppendField(&record->Extra, "mem_" + FormatAccessType(event.Type), addr.str());
    }
}
