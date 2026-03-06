#include "emulator/cpu/cpu_thread.h"
#include "emulator/cpu/cpu.h"
#include "emulator/log/logger.h"
#include "emulator/utils/config.h"
#include "emulator/utils/utils.h"

#include <algorithm>

CpuThread::CpuThread(std::shared_ptr<ICpuExecutor> cpu,
                     std::shared_ptr<CommitQueue> commitQueue)
    : mCpu(cpu), mCommitQueue(commitQueue) {
  if (!mCpu)
    throw std::invalid_argument("ICpuExecutor cannot be null");
  if (!mCommitQueue)
    throw std::invalid_argument("CommitQueue cannot be null");
}

CpuThread::~CpuThread() { stop(); }

void CpuThread::start() {
  mRunning.store(true, std::memory_order_release);
  mThread = std::thread(&CpuThread::threadLoop, this);
}

void CpuThread::stop() {
  mRunning.store(false, std::memory_order_release);
  if (mThread.joinable()) {
    mThread.join();
  }
}

void CpuThread::threadLoop() {
  while (mRunning.load(std::memory_order_acquire)) {
    // non-block alloc
    auto *array =
        (CommitArray *)mCommitQueue->alloc(std::extent<CommitArray>::value);

    // when commit queue is full, we skip this loop
    if (array == nullptr) {
      std::this_thread::yield();
      continue;
    }

    // proceed 1 cycle to get commit infos
    mCpu->cycle(*array);

    // we assume commit infos in the array don't have bubble
    // and already sorted-by execution order
    // so we can commit them to queue directly

    size_t numValidCommits = 0;
    for (auto &commit : *array) {
      if (commit.valid) {
        numValidCommits++;
      }
    }

    if (numValidCommits == 0)
      continue;

    // commit to queue
    mCommitQueue->push(numValidCommits);
  }
}
