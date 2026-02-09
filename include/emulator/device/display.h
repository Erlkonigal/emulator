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

    bool Init(uint32_t width, uint32_t height, const char* title);
    bool InitHeadless(uint32_t width, uint32_t height);
    void Shutdown();

    bool IsReady() const;
    void PollEvents(uint32_t timeoutMs);
    bool IsQuitRequested() const;
    void PushKey(uint32_t key);

    uint32_t GetWidth() const;
    uint32_t GetHeight() const;
    uint32_t GetPitch() const;
    uint64_t GetFrameBufferSize() const;
    uint64_t GetMappedSize() const;

    bool IsDirty() const;
    bool IsPresentRequested() const;
    bool ConsumePresentRequest();
    void Present();

    uint32_t GetUpdateFrequency() const override;

private:
    struct SdlDisplayState;
    SdlDisplayState* State = nullptr;
    
    std::atomic<bool> Dirty{false};
    std::atomic<bool> PresentRequested{false};
    
    mutable std::mutex InputMutex;
    mutable std::mutex FrameMutex;
    bool QuitRequested = false;
    uint32_t LastKey = 0;
    std::deque<uint32_t> KeyQueue;

    bool ReadRegister(uint64_t offset, uint64_t* value);
    bool WriteRegister(uint64_t offset, uint64_t value);
    MemResponse HandleRead(const MemAccess& access);
    MemResponse HandleWrite(const MemAccess& access);
};

#endif
