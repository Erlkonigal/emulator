#pragma once

#include "emulator/cpu/cpu.h"
#include "emulator/thread/i_thread.h"
#include "emulator/utils/singleton.h"

#include <memory>

class CpuThread : public IThread, public Singleton<CpuThread> {
public:
    void init(std::shared_ptr<ICpuExecutor> cpu);
    void start() override;
    void stop() override;
    void reset() override;

private:
    void threadLoop() override;

    std::shared_ptr<ICpuExecutor> mCpu;

    friend class Singleton<CpuThread>;
};