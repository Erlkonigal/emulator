#pragma once

#include <cstdint>
#include <memory>

class ICpuExecutor;

std::shared_ptr<ICpuExecutor> createCpuExecutor();

void EmulatorReset();
int RunEmulator(int argc, char **argv);