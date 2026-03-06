#pragma once

#include "emulator/cpu/cpu.h"
#include "emulator/utils/ring_queue.h"

#include <atomic>
#include <memory>
#include <thread>

class CpuThread {
public:
  using CommitQueue = RingQueue<CommitInfo>;
  CpuThread(std::shared_ptr<ICpuExecutor> cpu,
            std::shared_ptr<CommitQueue> commitQueue);
  ~CpuThread();

  void start();
  void stop();

private:
  void threadLoop();

  std::shared_ptr<ICpuExecutor> mCpu;
  std::shared_ptr<CommitQueue> mCommitQueue;
  std::thread mThread;
  std::atomic<bool> mRunning{false};
};
