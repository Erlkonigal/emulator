#pragma once

#include "emulator/log/trace_manager.h"

#define TRACE(name, fmt, ...) TraceManager::getInstance().trace(name, fmt, ##__VA_ARGS__)