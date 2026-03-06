#pragma once

#include "emulator/device/device.h"
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

class UartDevice : public Device {
public:
  UartDevice();
  ~UartDevice();

  void pushRx(uint8_t ch);

private:
  std::deque<uint8_t> mRxBuffer;
  std::string mTxBuffer;
  mutable std::mutex mMutex;

  uint32_t getStatus() const;
  void sync() override;
  BusResponse read(const BusAccess &access) override;
  BusResponse write(const BusAccess &access) override;
};
