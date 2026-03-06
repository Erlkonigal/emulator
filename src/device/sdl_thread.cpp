#include "emulator/device/sdl_thread.h"
#include "emulator/device/bus.h"
#include "emulator/device/display.h"

#include <chrono>

namespace {
constexpr auto kPresentInterval = std::chrono::milliseconds(16);
}

SdlThread::SdlThread(std::shared_ptr<SdlDisplayDevice> sdl) : mSdl(sdl) {}

SdlThread::~SdlThread() { stop(); }

void SdlThread::start() {
  if (mRunning.load(std::memory_order_acquire)) {
    return;
  }
  mRunning.store(true, std::memory_order_release);
  mThread = std::thread(&SdlThread::threadLoop, this);
}

void SdlThread::stop() {
  mRunning.store(false, std::memory_order_release);
  if (mThread.joinable()) {
    mThread.join();
  }
}

void SdlThread::threadLoop() {
  if (mSdl == nullptr) {
    return;
  }

  auto lastPresent = std::chrono::steady_clock::now();

  while (mRunning.load(std::memory_order_acquire)) {
    if (mSdl->isQuitRequested())
      return;

    bool shouldWait = !mSdl->isDirty() && !mSdl->isPresentRequested();
    mSdl->pollEvents(shouldWait ? 8u : 0u);

    auto now = std::chrono::steady_clock::now();
    if (mSdl->consumePresentRequest() ||
        (mSdl->isDirty() && now - lastPresent >= kPresentInterval)) {
      mSdl->present();
      Bus::getInstance().sync();
      lastPresent = now;
    }
  }
}
