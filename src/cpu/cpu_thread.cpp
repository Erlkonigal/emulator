#include "emulator/cpu/cpu_thread.h"
#include "emulator/commit/commit_queue.h"
#include "emulator/cpu/cpu.h"
#include "emulator/generated/hardware_config.h"
#include "emulator/log/logger.h"
#include "emulator/utils/utils.h"

#include <algorithm>

void CpuThread::init(std::shared_ptr<ICpuExecutor> cpu) {
    mCpu = cpu;
    if (!mCpu) {
        throw std::invalid_argument("ICpuExecutor cannot be null");
    }
}

void CpuThread::start() {
    setStarted(true);
    setRunning(true);
    mThread = std::thread(&CpuThread::threadLoop, this);
}

void CpuThread::stop() {
    setRunning(false);
    joinThread();
}

void CpuThread::reset() {
    stop();
    mCpu.reset();
    setStarted(false);
}

void CpuThread::threadLoop() {
    while (isRunning()) {
        auto *array = (CommitArray *)CommitQueue::getInstance().alloc(
            std::extent<CommitArray>::value);

        if (array == nullptr) {
            std::this_thread::yield();
            continue;
        }

        mCpu->cycle(*array);

        size_t numValidCommits = 0;
        for (auto &commit : *array) {
            if (commit.valid) {
                numValidCommits++;
            }
        }

        if (numValidCommits == 0) {
            continue;
        }

        CommitQueue::getInstance().push(numValidCommits);
    }
}