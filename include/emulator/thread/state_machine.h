#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <set>
#include <unordered_map>

template <typename State> class StateMachine {
public:
    using TransitionKey = std::pair<State, State>;
    using TransitionCallback = std::function<void(State, State)>;

    explicit StateMachine(State initial, std::set<TransitionKey> transitions = {})
        : mCurrentState(initial), mValidTransitions(std::move(transitions)) {}

    State getState() const { return mCurrentState.load(std::memory_order_acquire); }

    bool transition(State next) {
        State current = mCurrentState.load(std::memory_order_acquire);
        return transitionFrom(current, next);
    }

    bool transitionFrom(State current, State next) {
        if (!isValidTransition(current, next)) {
            return false;
        }

        std::lock_guard<std::mutex> lock(mMutex);
        mCurrentState.store(next, std::memory_order_release);

        if (mOnTransition) {
            mOnTransition(current, next);
        }

        mCondVar.notify_all();
        return true;
    }

    bool forceTransition(State next) {
        std::lock_guard<std::mutex> lock(mMutex);
        State current = mCurrentState.load(std::memory_order_acquire);
        mCurrentState.store(next, std::memory_order_release);

        if (mOnTransition) {
            mOnTransition(current, next);
        }

        mCondVar.notify_all();
        return true;
    }

    bool isValidTransition(State from, State to) const {
        if (from == to) {
            return true;
        }
        return mValidTransitions.find({from, to}) != mValidTransitions.end();
    }

    void waitForStateChange(State current, uint32_t timeoutMs = 0) {
        std::unique_lock<std::mutex> lock(mMutex);
        if (timeoutMs > 0) {
            mCondVar.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this, current]() {
                return mCurrentState.load(std::memory_order_acquire) != current;
            });
        } else {
            mCondVar.wait(lock, [this, current]() {
                return mCurrentState.load(std::memory_order_acquire) != current;
            });
        }
    }

    void setTransitionCallback(TransitionCallback callback) {
        mOnTransition = std::move(callback);
    }

    void addValidTransition(State from, State to) {
        mValidTransitions.insert({from, to});
    }

protected:
    std::atomic<State> mCurrentState;
    std::set<TransitionKey> mValidTransitions;
    mutable std::mutex mMutex;
    std::condition_variable mCondVar;
    TransitionCallback mOnTransition;
};