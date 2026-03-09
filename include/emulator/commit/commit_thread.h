#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

#include "emulator/commit/commit_queue.h"
#include "emulator/cpu/cpu.h"
#include "emulator/generated/hardware_config.h"
#include "emulator/log/tracer.h"
#include "emulator/utils/singleton.h"

enum class CommitThreadState { Running, Step, Paused, Halted };

class CommitThread : public Singleton<CommitThread> {
public:
  Tracer iTrace;

  void start();
  void stop();
  void reset();

  void run();
  void pause();
  void step(uint32_t count);

  CommitThreadState getState() const;
  bool wasStarted() const { return mStarted.load(std::memory_order_acquire); }

private:
  void threadLoop();

  // return number of commits successfully processed
  size_t processCommits(const CommitInfo *commits, const size_t &numCommits,
                        CommitThreadState &next);
  std::thread mThread;

  std::atomic<CommitThreadState> mState{CommitThreadState::Halted};
  std::atomic<size_t> mStepCount;
  std::atomic<bool> mStarted{false};

  friend class Singleton<CommitThread>;
};
