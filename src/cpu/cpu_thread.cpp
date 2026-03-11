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
    mCpu->reset();
}

bool CpuThread::start() {
    setStarted(true);
    setRunning(true);
    mThread = std::thread(&CpuThread::threadLoop, this);
    return true;
}

bool CpuThread::stop() {
    setRunning(false);
    joinThread();
    return true;
}

bool CpuThread::reset() {
    stop();
    mCpu.reset();
    setStarted(false);
    return true;
}

void CpuThread::threadLoop() {
    auto& queue = CommitQueue::getInstance();
    while (isRunning()) {
        auto *array = (CommitArray *)queue.alloc(
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

        queue.push(numValidCommits);
    }
}