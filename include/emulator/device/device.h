#pragma once

#include "emulator/device/bus.h"

class Device {
public:
  const char *name = nullptr;

  Device(const char *name) : name(name) {}
  virtual ~Device() = default;

  virtual BusResponse read(const BusAccess &access) = 0;
  virtual BusResponse write(const BusAccess &access) = 0;
  virtual void sync() = 0;
};
