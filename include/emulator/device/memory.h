#pragma once

#include "emulator/device/device.h"
#include <string>
#include <vector>

class MemoryDevice : public Device {
public:
  MemoryDevice(uint64_t size, bool readOnly);
  bool loadImage(const std::string &path, uint64_t offset = 0);
  uint64_t getSize() const;

private:
  std::vector<uint8_t> mStorage;
  bool mReadOnly;

  BusResponse read(const BusAccess &access) override;
  BusResponse write(const BusAccess &access) override;
  void sync() override {}
};
