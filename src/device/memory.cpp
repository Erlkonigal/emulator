#include "emulator/device/memory.h"

#include <fstream>

namespace {
bool isAccessValid(const std::vector<uint8_t> &storage,
                   const BusAccess &access) {
  if (access.size == 0 || access.size > sizeof(uint64_t)) {
    return false;
  }
  uint64_t size = static_cast<uint64_t>(storage.size());
  if (access.address >= size) {
    return false;
  }
  if (access.size > size - access.address) {
    return false;
  }
  return true;
}

BusResponse makeFault() {
  BusResponse response;
  response.error = BusErrorType::AccessFault;
  return response;
}

uint64_t readValue(const std::vector<uint8_t> &storage,
                   const BusAccess &access) {
  uint64_t value = 0;
  for (uint32_t i = 0; i < access.size; ++i) {
    value |=
        static_cast<uint64_t>(storage[static_cast<size_t>(access.address + i)])
        << (8 * i);
  }
  return value;
}

void writeValue(std::vector<uint8_t> *storage, const BusAccess &access) {
  if (storage == nullptr) {
    return;
  }
  uint64_t value = access.data;
  for (uint32_t i = 0; i < access.size; ++i) {
    (*storage)[static_cast<size_t>(access.address + i)] =
        static_cast<uint8_t>(value & 0xff);
    value >>= 8;
  }
}

} // namespace

MemoryDevice::MemoryDevice(uint64_t size, bool readOnly)
    : Device(), mStorage(size, 0), mReadOnly(readOnly) {}

bool MemoryDevice::loadImage(const std::string &path, uint64_t offset) {
  if (offset >= mStorage.size()) {
    return false;
  }
  std::ifstream input(path, std::ios::binary);
  if (!input.is_open()) {
    return false;
  }
  input.read(reinterpret_cast<char *>(mStorage.data() + offset),
             static_cast<std::streamsize>(mStorage.size() - offset));
  return input.good() || input.eof();
}

uint64_t MemoryDevice::getSize() const { return mStorage.size(); }

BusResponse MemoryDevice::read(const BusAccess &access) {
  if (!isAccessValid(mStorage, access)) {
    return makeFault();
  }
  BusResponse response;
  response.error = BusErrorType::None;
  response.data = readValue(mStorage, access);
  return response;
}

BusResponse MemoryDevice::write(const BusAccess &access) {
  if (!isAccessValid(mStorage, access)) {
    return makeFault();
  }
  if (mReadOnly) {
    return makeFault();
  }
  writeValue(&mStorage, access);
  BusResponse response;
  response.error = BusErrorType::None;
  return response;
}
