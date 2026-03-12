#pragma once

#include <cstdint>
#include <memory>

namespace emulator {

class ICpuExecutor;

std::shared_ptr<ICpuExecutor> createCpuExecutor();

void EmulatorReset();
int RunEmulator(int argc, char **argv);

} // namespace emulator