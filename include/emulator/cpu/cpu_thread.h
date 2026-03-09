#pragma once

#include "emulator/cpu/cpu.h"
#include "emulator/utils/singleton.h"

#include <atomic>
#include <memory>
#include <thread>

class CpuThread : public Singleton<CpuThread> {
public:
  void init(std::shared_ptr<ICpuExecutor> cpu);
  void start();
  void stop();
  void reset();

private:
  void threadLoop();

  std::shared_ptr<ICpuExecutor> mCpu;
  std::thread mThread;
  std::atomic<bool> mRunning{false};

  friend class Singleton<CpuThread>;
};
