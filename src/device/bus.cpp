#include "emulator/device/bus.h"
#include "emulator/device/device.h"

#include <cstring>

void Bus::getRegions(std::vector<AddressRegion> &regions) const {
  for (const auto &region : mMappings) {
    regions.push_back(region.first);
  }
}

bool Bus::computeRegionEnd(uint64_t base, uint64_t size, uint64_t &end) const {
  if (size == 0) {
    return false;
  }
  if (base > std::numeric_limits<uint64_t>::max() - size) {
    return false;
  }
  end = base + size;
  return true;
}

bool Bus::isRegionsOverlap(const AddressRegion &a,
                           const AddressRegion &b) const {
  uint64_t endA = 0;
  uint64_t endB = 0;
  if (!computeRegionEnd(a.base, a.size, endA) ||
      !computeRegionEnd(b.base, b.size, endB)) {
    return true;
  }
  return a.base < endB && b.base < endA;
}

bool Bus::validateRegion(const AddressRegion &region,
                         std::string &error) const {
  uint64_t end = 0;
  if (!computeRegionEnd(region.base, region.size, end)) {
    error = std::string("Invalid region: ") + region.name;
    return false;
  }

  std::vector<AddressRegion> newRegions;
  getRegions(newRegions);
  newRegions.push_back(region);

  for (const auto &m : newRegions) {
    if (isRegionsOverlap(region, m)) {
      error =
          std::string("Overlapping regions: ") + region.name + " and " + m.name;
      return false;
    }
  }

  return true;
}

void Bus::registerDevice(std::unique_ptr<Device> device,
                         const AddressRegion &region) {
  std::string error;
  if (!validateRegion(region, error)) {
    throw std::invalid_argument(error);
  }
  mMappings[region] = std::move(device);
}

bool Bus::contains(const uint64_t &address, const char *deviceName) const {
  for (const auto &device : mMappings) {
    if (address >= device.first.base &&
        address < device.first.base + device.first.size &&
        strcmp(device.first.name, deviceName) == 0) {
      return true;
    }
  }
  return false;
}

Device &Bus::findDevice(const uint64_t &address) {
  for (const auto &device : mMappings) {
    if (address >= device.first.base &&
        address < device.first.base + device.first.size) {
      return *device.second;
    }
  }
  throw std::invalid_argument("Device not found");
}

Device &Bus::findDevice(const std::string &name) {
  for (const auto &device : mMappings) {
    if (strcmp(device.first.name, name.c_str()) == 0) {
      return *device.second;
    }
  }
  throw std::invalid_argument("Device not found");
}

BusResponse Bus::read(const BusAccess &access) {
  return findDevice(access.address).read(access);
}

BusResponse Bus::write(const BusAccess &access) {
  return findDevice(access.address).write(access);
}

void Bus::sync() {
  for (const auto &device : mMappings) {
    device.second->sync();
  }
}