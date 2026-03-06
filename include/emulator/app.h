#pragma once

#include <cstdint>
#include <memory>
#include <vector>

class ICpuExecutor;

std::shared_ptr<ICpuExecutor> createCpuExecutor(const std::vector<uint8_t>& romData);

int RunEmulator(int argc, char** argv);