#include "emulator/device/uart.h"
#include "emulator/log/logger.h"
#include "emulator/utils/config.h"

#include <cstdio>
#include <string>
#include <utility>

namespace {
constexpr uint64_t kUartDataOffset = 0x0;
constexpr uint64_t kUartStatusOffset = 0x4;
constexpr uint32_t kUartStatusRxReady = 1u << 0;
constexpr uint32_t kUartStatusTxReady = 1u << 1;
constexpr uint32_t kUartRegSize = 4;

bool isValidAccess(const BusAccess &access) {
  return access.size == kUartRegSize;
}

uint8_t extractByte(uint64_t value) {
  return static_cast<uint8_t>(value & 0xff);
}

BusResponse makeFault() {
  BusResponse response;
  response.error = BusErrorType::AccessFault;
  return response;
}
} // namespace

UartDevice::UartDevice() : Device(kUartDeviceName) {}

UartDevice::~UartDevice() {
  std::lock_guard<std::mutex> lock(mMutex);
  sync();
}

void UartDevice::pushRx(uint8_t ch) {
  std::lock_guard<std::mutex> lock(mMutex);
  mRxBuffer.push_back(ch);
}

uint32_t UartDevice::getStatus() const {
  std::lock_guard<std::mutex> lock(mMutex);
  uint32_t status = kUartStatusTxReady;
  if (!mRxBuffer.empty()) {
    status |= kUartStatusRxReady;
  }
  return status;
}

BusResponse UartDevice::read(const BusAccess &access) {
  if (!isValidAccess(access)) {
    return makeFault();
  }
  if (access.address == kUartStatusOffset) {
    BusResponse response;
    response.error = BusErrorType::None;
    response.data = getStatus();
    return response;
  }
  if (access.address == kUartDataOffset) {
    BusResponse response;
    response.error = BusErrorType::None;
    std::lock_guard<std::mutex> lock(mMutex);
    if (mRxBuffer.empty()) {
      response.data = 0;
    } else {
      response.data = mRxBuffer.front();
      mRxBuffer.pop_front();
    }
    return response;
  }
  return makeFault();
}

BusResponse UartDevice::write(const BusAccess &access) {
  if (!isValidAccess(access)) {
    return makeFault();
  }
  if (access.address == kUartDataOffset) {
    uint8_t ch = extractByte(access.data);
    std::lock_guard<std::mutex> lock(mMutex);
    mTxBuffer.push_back(static_cast<char>(ch));
    BusResponse response;
    response.error = BusErrorType::None;
    return response;
  }
  return makeFault();
}

void UartDevice::sync() {
  std::lock_guard<std::mutex> lock(mMutex);
  if (mTxBuffer.empty()) {
    return;
  }
  logging::device("%s", mTxBuffer.c_str());
  mTxBuffer.clear();
}
