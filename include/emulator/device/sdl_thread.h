#pragma once

#include <atomic>
#include <memory>
#include <thread>

class SdlDisplayDevice;

class SdlThread {
public:
  explicit SdlThread(std::shared_ptr<SdlDisplayDevice> sdl);
  ~SdlThread();

  void start();
  void stop();
  bool isRunning() const { return mRunning.load(std::memory_order_acquire); }

private:
  void threadLoop();

  std::shared_ptr<SdlDisplayDevice> mSdl;
  std::thread mThread;
  std::atomic<bool> mRunning{false};
};
