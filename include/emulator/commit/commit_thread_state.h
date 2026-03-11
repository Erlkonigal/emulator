#pragma once

#include <set>
#include <utility>

enum class CommitThreadState {
    Init,
    Halted,
    Paused,
    Running,
    Step
};

inline std::set<std::pair<CommitThreadState, CommitThreadState>>
getCommitThreadTransitions() {
    return {
        {CommitThreadState::Init, CommitThreadState::Paused},
        {CommitThreadState::Paused, CommitThreadState::Running},
        {CommitThreadState::Paused, CommitThreadState::Step},
        {CommitThreadState::Running, CommitThreadState::Paused},
        {CommitThreadState::Running, CommitThreadState::Halted},
        {CommitThreadState::Step, CommitThreadState::Paused},
        {CommitThreadState::Step, CommitThreadState::Halted},
    };
}