#pragma once

#include <atomic>
#include <cstddef>

#include "emulator/utils/singleton.h"

namespace emulator {

class Terminal : public Singleton<Terminal> {
public:
    void setup(bool enableSignals = false);
    void restore();
    void setInterruptFlag(std::atomic<bool>* flag);
    void processIo();

    bool wasInterrupted() const { return mInterrupted.load(); }
    void clearInterrupt() { mInterrupted.store(false); }

private:
    void* mOriginalTermios = nullptr;
    std::atomic<bool>* mInterruptFlag = nullptr;
    std::atomic<bool> mInterrupted{false};
    bool mConfigured = false;

    Terminal() = default;
    friend class Singleton<Terminal>;
};

} // namespace emulator