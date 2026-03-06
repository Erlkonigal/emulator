#pragma once

#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "emulator/utils/singleton.h"

class Device;

enum class BusAccessType {
  None,
  Read,
  Write,
};

enum class BusErrorType {
  None,
  AccessFault,
};

struct BusAccess {
  BusAccessType type = BusAccessType::None;
  uint64_t address = 0;
  uint32_t size = 0;
  uint64_t data = 0;
};

struct BusResponse {
  BusErrorType error = BusErrorType::None;
  uint64_t data = 0;
  uint32_t latencyCycles = 0;
};

struct AddressRegion {
  uint64_t base = 0;
  uint64_t size = 0;

  bool operator<(const AddressRegion &other) const { return base < other.base; }
};

class IBus {
public:
  virtual ~IBus() = default;

  virtual BusResponse read(const BusAccess &access) = 0;
  virtual BusResponse write(const BusAccess &access) = 0;
  virtual bool interrupt() = 0;
};

class Bus : public IBus, public Singleton<Bus> {
public:
  void registerDevice(std::unique_ptr<Device> device,
                      const AddressRegion &region);
  bool validateRegion(const AddressRegion &region, std::string &error) const;
  bool contains(const uint64_t &address, const char *deviceName) const;
  Device &findDevice(const uint64_t &address);
  Device &findDevice(const std::string &name);
  BusResponse read(const BusAccess &access) override;
  BusResponse write(const BusAccess &access) override;
  bool interrupt() override { return false; } // not used for now
  void sync();

private:
  Bus();
  ~Bus() = default;
  std::map<AddressRegion, std::unique_ptr<Device>> mMappings;
  bool computeRegionEnd(uint64_t base, uint64_t size, uint64_t &end) const;
  bool isRegionsOverlap(const AddressRegion &a, const AddressRegion &b) const;
  void getRegions(std::vector<AddressRegion> &regions) const;

  friend class Singleton<Bus>;
};
