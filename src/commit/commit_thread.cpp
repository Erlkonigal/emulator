#include "emulator/commit/commit_thread.h"
#include "emulator/commit/shadow_arch.h"
#include "emulator/cpu/cpu.h"
#include "emulator/debug/breakpoint.h"
#include "emulator/log/logger.h"

#include <iostream>
#include <thread>
#include <vector>

CommitThread::CommitThread() {}

CommitThread::~CommitThread() { stop(); }

CommitThreadState CommitThread::getState() const {
  return mState.load(std::memory_order_acquire);
}

void CommitThread::start() {
  mState.store(CommitThreadState::Paused, std::memory_order_release);
  mThread = std::thread(&CommitThread::threadLoop, this);
}

void CommitThread::stop() {
  mState.store(CommitThreadState::Halted, std::memory_order_release);
  if (mThread.joinable()) {
    mThread.join();
  }
}

void CommitThread::run() {
  mState.store(CommitThreadState::Running, std::memory_order_release);
}

void CommitThread::step(uint32_t count) {
  mState.store(CommitThreadState::Step, std::memory_order_release);
  mStepCount.store(count, std::memory_order_release);
}

void CommitThread::pause() {
  mState.store(CommitThreadState::Paused, std::memory_order_release);
}

void CommitThread::threadLoop() {
  CommitThreadState state;
  while (true) {
    state = mState.load(std::memory_order_acquire);
    switch (state) {
    case CommitThreadState::Running: {
      size_t numFrontCommits = 0;
      CommitInfo *commits = CommitQueue::getInstance().front(
          kMaxNumCommitsPerConsumption, numFrontCommits);
      if (numFrontCommits == 0) {
        std::this_thread::yield();
        continue;
      }
      size_t successCommits = processCommits(commits, numFrontCommits, state);
      CommitQueue::getInstance().pop(successCommits);
      if (successCommits < numFrontCommits) {
        mState.store(state, std::memory_order_release);
      }
      break;
    }
    case CommitThreadState::Step: {
      size_t remaining = mStepCount.load(std::memory_order_acquire);
      if (remaining > 0) {
        size_t numFrontCommits = 0;
        CommitInfo *commits =
            CommitQueue::getInstance().front(remaining, numFrontCommits);
        if (numFrontCommits == 0) {
          std::this_thread::yield();
          continue;
        }
        size_t successCommits = processCommits(commits, numFrontCommits, state);
        CommitQueue::getInstance().pop(successCommits);
        if (successCommits < numFrontCommits) {
          mStepCount.store(0, std::memory_order_release);
          mState.store(state, std::memory_order_release);
        } else {
          mStepCount.fetch_sub(successCommits, std::memory_order_release);
        }
      } else {
        mState.store(CommitThreadState::Paused, std::memory_order_release);
      }
      break;
    }
    case CommitThreadState::Paused:
      std::this_thread::yield();
      break;

    case CommitThreadState::Halted:
      return;
    }
  }
}

size_t CommitThread::processCommits(const CommitInfo *commits,
                                    const size_t &numCommits,
                                    CommitThreadState &next) {
  size_t processed = 0;
  for (size_t i = 0; i < numCommits; i++) {
    const auto &commit = commits[i];

    // handle breakpoint
    bool breakpointHit =
        BreakPointController::getInstance().contains(commit.pc);
    if (breakpointHit) {
      INFO("Breakpoint hit at pc: 0x%lx", commit.pc);
      next = CommitThreadState::Paused;
      goto _exit;
    }

    ShadowArch::getInstance().update(commit);

    switch (commit.errorType) {
    case CpuErrorType::None:
      processed++;
      break;
    case CpuErrorType::Stop:
      INFO("Commit Stop");
      next = CommitThreadState::Halted;
      processed++;
      goto _exit;
    case CpuErrorType::Halt:
      ERROR("Commit Halt");
      next = CommitThreadState::Halted;
      processed++;
      goto _exit;
    case CpuErrorType::Assert:
      ERROR("Commit Assert: %s", commit.errorMsg);
      next = CommitThreadState::Halted;
      processed++;
      goto _exit;
    default:
      assert(false);
    }
  }

_exit:
  return processed;
}
