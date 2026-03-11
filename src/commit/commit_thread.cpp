#include "emulator/commit/commit_thread.h"
#include "emulator/commit/shadow_arch.h"
#include "emulator/cpu/cpu.h"
#include "emulator/debug/breakpoint.h"
#include "emulator/log/logger.h"
#include "emulator/log/trace_macro.h"
#include "emulator/log/trace_manager.h"
#include "emulator/runtime_config.h"

#include <iostream>
#include <thread>
#include <vector>

void CommitThread::init() {
    TraceManager::getInstance().createTracer<Tracer>("itrace");
}

bool CommitThread::start() {
    if (!mStateMachine.transition(CommitThreadState::Paused)) {
        return false;
    }
    setStarted(true);
    mThread = std::thread(&CommitThread::threadLoop, this);
    return true;
}

bool CommitThread::stop() {
    CommitThreadState state = mStateMachine.getState();
    if (state != CommitThreadState::Init && state != CommitThreadState::Halted) {
        mStateMachine.forceTransition(CommitThreadState::Halted);
    }
    joinThread();
    return true;
}

bool CommitThread::reset() {
    CommitThreadState state = mStateMachine.getState();
    if (state != CommitThreadState::Init && state != CommitThreadState::Halted) {
        mStateMachine.forceTransition(CommitThreadState::Halted);
    }
    joinThread();
    mStepCount.store(0, std::memory_order_release);
    setStarted(false);
    mStateMachine.forceTransition(CommitThreadState::Init);
    return true;
}

bool CommitThread::run() {
    return mStateMachine.transition(CommitThreadState::Running);
}

bool CommitThread::step(uint32_t count) {
    mStepCount.store(count, std::memory_order_release);
    return mStateMachine.transition(CommitThreadState::Step);
}

bool CommitThread::pause() {
    CommitThreadState state = mStateMachine.getState();
    if (state == CommitThreadState::Running || state == CommitThreadState::Step) {
        return mStateMachine.transitionFrom(state, CommitThreadState::Paused);
    }
    return false;
}

void CommitThread::threadLoop() {
    auto& queue = CommitQueue::getInstance();
    
    while (true) {
        CommitThreadState state = mStateMachine.getState();

        switch (state) {
        case CommitThreadState::Running: {
            size_t numFrontCommits = 0;
            CommitInfo *commits = queue.front(
                kMaxNumCommitsPerConsumption, numFrontCommits);
            if (numFrontCommits == 0) {
                std::this_thread::yield();
                continue;
            }

            auto result = processCommits(commits, numFrontCommits, state);
            queue.pop(result.processedCount);

            if (result.nextState != state) {
                mStateMachine.forceTransition(result.nextState);
            }
            break;
        }

        case CommitThreadState::Step: {
            size_t remaining = mStepCount.load(std::memory_order_acquire);
            if (remaining == 0) {
                mStateMachine.forceTransition(CommitThreadState::Paused);
                break;
            }

            size_t numFrontCommits = 0;
            CommitInfo *commits =
                queue.front(remaining, numFrontCommits);
            if (numFrontCommits == 0) {
                std::this_thread::yield();
                continue;
            }

            auto result = processCommits(commits, numFrontCommits, state);
            queue.pop(result.processedCount);

            if (result.nextState != CommitThreadState::Step) {
                mStepCount.store(0, std::memory_order_release);
                mStateMachine.forceTransition(result.nextState);
            } else {
                mStepCount.fetch_sub(result.processedCount, std::memory_order_release);
            }
            break;
        }

        case CommitThreadState::Paused:
            std::this_thread::yield();
            break;

        case CommitThreadState::Init:
        case CommitThreadState::Halted:
            return;
        }
    }
}

CommitThread::ProcessResult CommitThread::processCommits(const CommitInfo *commits,
                                                          size_t numCommits,
                                                          CommitThreadState currentState) {
    ProcessResult result{0, currentState};

    for (size_t i = 0; i < numCommits; i++) {
        const auto &commit = commits[i];

        if (commit.valid) {
            TRACE("itrace", "PC=0x%08lx INST=0x%08x %s",
                  (unsigned long)commit.pc, commit.inst, commit.decode);
            if (commit.isRegWrite) {
                TRACE("itrace", "         REG x%u <- 0x%08x", commit.regId, commit.regData);
            }
            if (commit.isRamWrite) {
                TRACE("itrace", "         RAM [0x%08lx] <- 0x%08x",
                      (unsigned long)commit.ramOffset, commit.ramData);
            }
        }

        bool breakpointHit =
            BreakPointController::getInstance().contains(commit.pc);
        if (breakpointHit) {
            INFO("Breakpoint hit at pc: 0x%lx", commit.pc);
            result.nextState = CommitThreadState::Paused;
            result.processedCount++;
            return result;
        }

        ShadowArch::getInstance().update(commit);

        switch (commit.errorType) {
        case CpuErrorType::None:
            result.processedCount++;
            break;
        case CpuErrorType::Stop:
            INFO("Commit Stop");
            result.nextState = CommitThreadState::Halted;
            result.processedCount++;
            return result;
        case CpuErrorType::Halt:
            ERROR("Commit Halt");
            result.nextState = CommitThreadState::Halted;
            result.processedCount++;
            return result;
        case CpuErrorType::Assert:
            ERROR("Commit Assert: %s", commit.errorMsg);
            result.nextState = CommitThreadState::Halted;
            result.processedCount++;
            return result;
        default:
            assert(false);
        }
    }

    return result;
}