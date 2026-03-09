#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <thread>

#include "emulator/generated/hardware_config.h"
#include "emulator/utils/singleton.h"

class DebugServer : public Singleton<DebugServer> {
public:
    bool start(uint16_t port = kDefaultDebugPort);
    void stop();
    void requestStop() { mRunning.store(false); }
    bool isRunning() const { return mRunning.load(); }
    void broadcastLog(const char* message);

private:
    void serverThread();
    void handleClient(int clientFd);

    int mListenFd = -1;
    int mClientFd = -1;
    std::thread mThread;
    std::atomic<bool> mRunning{false};
    uint16_t mPort = kDefaultDebugPort;
    std::mutex mClientMutex;
    bool mNotifiedHalt = false;

    DebugServer() = default;
    friend class Singleton<DebugServer>;
};