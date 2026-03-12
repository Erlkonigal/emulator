#pragma once

#include "emulator/generated/hardware_config.h"
#include "emulator/thread/i_thread.h"
#include "emulator/utils/singleton.h"

#include <cstdint>
#include <mutex>

namespace emulator {

class NetworkInputHandler : public IThread, public Singleton<NetworkInputHandler> {
public:
    bool start(uint16_t port = kDefaultDebugPort);
    bool stop() override;
    bool reset() override { return stop(); }

    void requestStop() { setRunning(false); }
    void broadcast(const std::string& message);

private:
    bool start() override { return false; }
    void threadLoop() override;
    void handleClient(int clientFd);

    int mListenFd = -1;
    int mClientFd = -1;
    uint16_t mPort = kDefaultDebugPort;
    std::mutex mClientMutex;
    bool mNotifiedHalt = false;

    NetworkInputHandler() = default;
    friend class Singleton<NetworkInputHandler>;
};

} // namespace emulator