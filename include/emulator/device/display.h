#ifndef EMULATOR_DEVICE_DISPLAY_H
#define EMULATOR_DEVICE_DISPLAY_H

#include <atomic>
#include <deque>
#include <mutex>
#include "emulator/device/device.h"

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

class SdlDisplayDevice : public Device {
public:
    static constexpr uint64_t kControlRegionSize = 0x1000;
    static constexpr uint64_t kFrameBufferOffset = kControlRegionSize;

    SdlDisplayDevice();
    ~SdlDisplayDevice();

    bool init(uint32_t width, uint32_t height, const char* title);
    bool initHeadless(uint32_t width, uint32_t height);
    void shutdown();

    bool isReady() const;
    void pollEvents(uint32_t timeoutMs);
    bool isQuitRequested() const;
    void pushKey(uint32_t key);

    uint32_t getWidth() const;
    uint32_t getHeight() const;
    uint32_t getPitch() const;
    uint64_t getFrameBufferSize() const;
    uint64_t getMappedSize() const;

    bool isDirty() const;
    bool isPresentRequested() const;
    bool consumePresentRequest();
    void present();

    uint32_t getUpdateFrequency() const override;

private:
    struct SdlDisplayState;
    SdlDisplayState* mState = nullptr;
    
    std::atomic<bool> mDirty{false};
    std::atomic<bool> mPresentRequested{false};
    
    mutable std::mutex mInputMutex;
    mutable std::mutex mFrameMutex;
    bool mQuitRequested = false;
    uint32_t mLastKey = 0;
    std::deque<uint32_t> mKeyQueue;

    bool readRegister(uint64_t offset, uint64_t* value);
    bool writeRegister(uint64_t offset, uint64_t value);
    MemResponse handleRead(const MemAccess& access);
    MemResponse handleWrite(const MemAccess& access);
};

#endif
