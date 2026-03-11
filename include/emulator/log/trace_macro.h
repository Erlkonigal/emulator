#pragma once

#include "emulator/generated/hardware_config.h"

#if kEnableTrace

#include "emulator/log/trace_manager.h"

#define TRACE(name, fmt, ...) \
    do { \
        auto& _tm = TraceManager::getInstance(); \
        if (_tm.isEnabled(name)) { \
            _tm.trace(name, fmt, ##__VA_ARGS__); \
        } \
    } while (0)

#else

#define TRACE(name, fmt, ...) ((void)0)

#endif