#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

#include "emulator/device/device.h"
#include "emulator/utils/singleton.h"

class Rom : public Singleton<Rom>, public IDevice {
public:
    void init(const std::vector<uint8_t>& data);
    void init(const uint8_t* data, size_t size);
    ~Rom() = default;

    bool read(uint64_t offset, uint8_t* data, size_t len) override;
    bool write(uint64_t offset, const uint8_t* data, size_t len) override;

    void reset() override;
    uint64_t getSize() const { return mData.size(); }

private:
    std::vector<uint8_t> mData;

    Rom() = default;
    friend class Singleton<Rom>;
};