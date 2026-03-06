#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

#include "emulator/commit/commit_queue.h"
#include "emulator/cpu/cpu.h"
#include "emulator/utils/config.h"
#include "emulator/utils/singleton.h"

enum class CommitThreadState { Running, Step, Paused, Halted };

class CommitThread : public Singleton<CommitThread> {
public:
  CommitThread();
  ~CommitThread();

  void start();
  void stop();

  // State machine control
  void run();
  void pause();
  void step(uint32_t count);

  CommitThreadState getState() const;

private:
  void threadLoop();

  // return number of commits successfully processed
  size_t processCommits(const CommitInfo *commits, const size_t &numCommits,
                        CommitThreadState &next);
  std::thread mThread;

  std::atomic<CommitThreadState> mState{CommitThreadState::Halted};
  std::atomic<size_t> mStepCount;

  friend class Singleton<CommitThread>;
};
