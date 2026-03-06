#include "emulator/device/display.h"
#include "SDL_render.h"
#include "emulator/utils/config.h"
#include <SDL2/SDL.h>
#include <mutex>

namespace {
constexpr uint64_t kRegCtrl = 0x00;
constexpr uint64_t kRegWidth = 0x04;
constexpr uint64_t kRegHeight = 0x08;
constexpr uint64_t kRegPitch = 0x0c;
constexpr uint64_t kRegStatus = 0x10;
constexpr uint64_t kRegKeyData = 0x20;
constexpr uint64_t kRegKeyStatus = 0x24;
constexpr uint64_t kRegKeyLast = 0x28;

constexpr uint64_t kStatusReady = 1ull << 0;
constexpr uint64_t kStatusDirty = 1ull << 1;

constexpr uint32_t kKeyStatusReady = 1u << 0;
} // namespace

SdlDisplayDevice::SdlDisplayDevice() : Device(kDisplayDeviceName) {
  if (kDisplayHeadless) {
    initHeadless(kDisplayWidth, kDisplayHeight);
  } else {
    init(kDisplayWidth, kDisplayHeight, kDisplayWindowTitle);
  }
}

SdlDisplayDevice::~SdlDisplayDevice() { shutdown(); }

bool SdlDisplayDevice::init(uint32_t width, uint32_t height,
                            const char *title) {
  if (width == 0 || height == 0) {
    return false;
  }
  shutdown();
  if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
      return false;
    }
  }
  mState.width = width;
  mState.height = height;
  mState.window = SDL_CreateWindow(
      title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
      static_cast<int>(width), static_cast<int>(height), SDL_WINDOW_SHOWN);
  if (mState.window == nullptr) {
    return false;
  }
  mState.renderer =
      SDL_CreateRenderer(mState.window, -1, SDL_RENDERER_SOFTWARE);
  if (mState.renderer == nullptr) {
    return false;
  }
  mState.texture = SDL_CreateTexture(
      mState.renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
      static_cast<int>(width), static_cast<int>(height));
  if (mState.texture == nullptr) {
    return false;
  }
  mState.frameBuffer.assign(static_cast<size_t>(width) * height * 4, 0);
  mState.ready = true;
  mState.headless = false;
  mDirty.store(true, std::memory_order_release);
  return true;
}

bool SdlDisplayDevice::initHeadless(uint32_t width, uint32_t height) {
  if (width == 0 || height == 0) {
    return false;
  }
  shutdown();
  mState.width = width;
  mState.height = height;
  mState.frameBuffer.assign(static_cast<size_t>(width) * height * 4, 0);
  mState.ready = true;
  mState.headless = true;
  mDirty.store(true, std::memory_order_release);
  return true;
}

void SdlDisplayDevice::shutdown() {
  if (mState.headless) {
    return;
  }
  mState.headless = false;
  mState.frameBuffer.clear();
  if (mState.texture != nullptr) {
    SDL_DestroyTexture(mState.texture);
    mState.texture = nullptr;
  }
  if (mState.renderer != nullptr) {
    SDL_DestroyRenderer(mState.renderer);
    mState.renderer = nullptr;
  }
  if (mState.window != nullptr) {
    SDL_DestroyWindow(mState.window);
    mState.window = nullptr;
  }
  mState.width = 0;
  mState.height = 0;
  mState.ready = false;
  mDirty.store(false, std::memory_order_release);
  mPresentRequested.store(false, std::memory_order_release);
}

bool SdlDisplayDevice::isReady() const { return mState.ready; }

void SdlDisplayDevice::pollEvents(uint32_t timeoutMs) {
  if (!mState.ready) {
    return;
  }
  if (mState.headless) {
    return;
  }
  SDL_Event event;
  if (timeoutMs > 0) {
    if (SDL_WaitEventTimeout(&event, static_cast<int>(timeoutMs)) != 0) {
      std::lock_guard<std::mutex> lock(mInputMutex);
      if (event.type == SDL_QUIT) {
        mQuitRequested = true;
      }
      if (event.type == SDL_KEYDOWN) {
        mLastKey = static_cast<uint32_t>(event.key.keysym.sym);
        mKeyQueue.push_back(mLastKey);
      }
    }
  }
  while (SDL_PollEvent(&event)) {
    std::lock_guard<std::mutex> lock(mInputMutex);
    if (event.type == SDL_QUIT) {
      mQuitRequested = true;
    }
    if (event.type == SDL_KEYDOWN) {
      mLastKey = static_cast<uint32_t>(event.key.keysym.sym);
      mKeyQueue.push_back(mLastKey);
    }
  }
}

bool SdlDisplayDevice::isQuitRequested() const {
  std::lock_guard<std::mutex> lock(mInputMutex);
  return mQuitRequested;
}

void SdlDisplayDevice::pushKey(uint32_t key) {
  std::lock_guard<std::mutex> lock(mInputMutex);
  mLastKey = key;
  mKeyQueue.push_back(key);
}

uint32_t SdlDisplayDevice::getWidth() const { return mState.width; }

uint32_t SdlDisplayDevice::getHeight() const { return mState.height; }

uint32_t SdlDisplayDevice::getPitch() const { return mState.width * 4u; }

uint64_t SdlDisplayDevice::getFrameBufferSize() const {
  if (!mState.ready) {
    return 0;
  }
  return static_cast<uint64_t>(mState.width) * mState.height * 4u;
}

uint64_t SdlDisplayDevice::getMappedSize() const {
  return kDisplayFrameBufferOffset + getFrameBufferSize();
}

bool SdlDisplayDevice::isDirty() const {
  return mDirty.load(std::memory_order_acquire);
}

bool SdlDisplayDevice::isPresentRequested() const {
  return mPresentRequested.load(std::memory_order_acquire);
}

bool SdlDisplayDevice::consumePresentRequest() {
  return mPresentRequested.exchange(false, std::memory_order_acq_rel);
}

void SdlDisplayDevice::present() {
  if (!mState.ready) {
    return;
  }
  if (mState.headless) {
    mDirty.store(false, std::memory_order_release);
    return;
  }
  std::lock_guard<std::mutex> lock(mFrameMutex);
  SDL_UpdateTexture(mState.texture, nullptr, mState.frameBuffer.data(),
                    static_cast<int>(mState.width) * 4);
  SDL_RenderClear(mState.renderer);
  SDL_RenderCopy(mState.renderer, mState.texture, nullptr, nullptr);
  SDL_RenderPresent(mState.renderer);
  mDirty.store(false, std::memory_order_release);
}

bool SdlDisplayDevice::readRegister(uint64_t offset, uint64_t &value) {
  if (offset == kRegKeyData) {
    std::lock_guard<std::mutex> lock(mInputMutex);
    if (mKeyQueue.empty()) {
      value = 0;
      return true;
    }
    value = mKeyQueue.front();
    mKeyQueue.pop_front();
    return true;
  }
  if (offset == kRegKeyStatus) {
    std::lock_guard<std::mutex> lock(mInputMutex);
    value = mKeyQueue.empty() ? 0u : kKeyStatusReady;
    return true;
  }
  if (offset == kRegKeyLast) {
    std::lock_guard<std::mutex> lock(mInputMutex);
    value = mLastKey;
    return true;
  }
  if (offset == kRegWidth) {
    value = getWidth();
    return true;
  }
  if (offset == kRegHeight) {
    value = getHeight();
    return true;
  }
  if (offset == kRegPitch) {
    value = getPitch();
    return true;
  }
  if (offset == kRegStatus) {
    uint64_t status = 0;
    if (isReady()) {
      status |= kStatusReady;
    }
    if (mDirty.load(std::memory_order_acquire)) {
      status |= kStatusDirty;
    }
    value = status;
    return true;
  }
  return false;
}

bool SdlDisplayDevice::writeRegister(uint64_t offset, uint64_t value) {
  if (offset == kRegCtrl) {
    if ((value & 1u) != 0) {
      mPresentRequested.store(true, std::memory_order_release);
    }
    return true;
  }
  if (offset == kRegKeyStatus) {
    std::lock_guard<std::mutex> lock(mInputMutex);
    mKeyQueue.clear();
    mLastKey = 0;
    return true;
  }
  return false;
}

BusResponse SdlDisplayDevice::read(const BusAccess &access) {
  BusResponse response;
  if (access.size == 0 || access.size > sizeof(uint64_t)) {
    response.error = BusErrorType::AccessFault;
    return response;
  }
  uint64_t mappedSize = getMappedSize();
  if (access.address >= mappedSize ||
      access.address > mappedSize - access.size) {
    response.error = BusErrorType::AccessFault;
    return response;
  }
  if (access.address < kDisplayFrameBufferOffset) {
    uint64_t value = 0;
    if (!readRegister(access.address, value)) {
      response.error = BusErrorType::AccessFault;
      return response;
    }
    response.data = value;
    return response;
  }
  uint64_t fbOffset = access.address - kDisplayFrameBufferOffset;
  std::lock_guard<std::mutex> lock(mFrameMutex);
  uint64_t value = 0;
  for (uint32_t i = 0; i < access.size; ++i) {
    value |= static_cast<uint64_t>(
                 mState.frameBuffer[static_cast<size_t>(fbOffset + i)])
             << (8 * i);
  }
  response.data = value;
  return response;
}

BusResponse SdlDisplayDevice::write(const BusAccess &access) {
  BusResponse response;
  if (access.size == 0 || access.size > sizeof(uint64_t)) {
    response.error = BusErrorType::AccessFault;
    return response;
  }
  uint64_t mappedSize = getMappedSize();
  if (access.address >= mappedSize ||
      access.address > mappedSize - access.size) {
    response.error = BusErrorType::AccessFault;
    return response;
  }
  if (access.address < kDisplayFrameBufferOffset) {
    if (!writeRegister(access.address, access.data)) {
      response.error = BusErrorType::AccessFault;
      return response;
    }
    return response;
  }
  uint64_t fbOffset = access.address - kDisplayFrameBufferOffset;
  std::lock_guard<std::mutex> lock(mFrameMutex);
  uint64_t value = access.data;
  for (uint32_t i = 0; i < access.size; ++i) {
    mState.frameBuffer[static_cast<size_t>(fbOffset + i)] =
        static_cast<uint8_t>(value & 0xff);
    value >>= 8;
  }
  mDirty.store(true, std::memory_order_release);
  return response;
}
