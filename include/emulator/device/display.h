#pragma once

#include "emulator/device/device.h"
#include <atomic>
#include <deque>
#include <mutex>

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

class SdlDisplayDevice : public Device {
public:
  SdlDisplayDevice();
  ~SdlDisplayDevice();

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

private:
  struct SdlDisplayState {
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    SDL_Texture *texture = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<uint8_t> frameBuffer;
    bool ready = false;
    bool headless = false;
  };

  SdlDisplayState mState;

  std::atomic<bool> mDirty{false};
  std::atomic<bool> mPresentRequested{false};

  mutable std::mutex mInputMutex;
  mutable std::mutex mFrameMutex;
  bool mQuitRequested = false;
  uint32_t mLastKey = 0;
  std::deque<uint32_t> mKeyQueue;

  bool init(uint32_t width, uint32_t height, const char *title);
  bool initHeadless(uint32_t width, uint32_t height);
  void shutdown();
  bool readRegister(uint64_t offset, uint64_t &value);
  bool writeRegister(uint64_t offset, uint64_t value);
  BusResponse read(const BusAccess &access) override;
  BusResponse write(const BusAccess &access) override;
  void sync() override {}
};
