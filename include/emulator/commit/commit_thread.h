#pragma once

#include "emulator/commit/commit_queue.h"
#include "emulator/commit/commit_thread_state.h"
#include "emulator/cpu/cpu.h"
#include "emulator/generated/hardware_config.h"
#include "emulator/thread/i_thread.h"
#include "emulator/thread/state_machine.h"
#include "emulator/utils/singleton.h"

class CommitThread : public IThread, public Singleton<CommitThread> {
public:
    void init();
    void start() override;
    void stop() override;
    void reset() override;

    void run();
    void pause();
    void step(uint32_t count);

    CommitThreadState getState() const { return mStateMachine.getState(); }

private:
    void threadLoop() override;

    struct ProcessResult {
        size_t processedCount;
        CommitThreadState nextState;
    };

    ProcessResult processCommits(const CommitInfo *commits, size_t numCommits,
                                 CommitThreadState currentState);

    StateMachine<CommitThreadState> mStateMachine{CommitThreadState::Halted,
                                                   getCommitThreadTransitions()};
    std::atomic<size_t> mStepCount{0};

    friend class Singleton<CommitThread>;
};