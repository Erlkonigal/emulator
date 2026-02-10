#ifndef EMULATOR_DEVICE_DEVICE_H
#define EMULATOR_DEVICE_DEVICE_H

#include <cstdint>
#include <functional>
#include <string>

#include "emulator/bus/bus.h"
#include "emulator/cpu/cpu.h"

enum class DeviceType {
    Ram,
    Rom,
    Display,
    Timer,
    Uart,
    Other
};

class Device {
public:
    using ReadHandler = std::function<MemResponse(const MemAccess& access)>;
    using WriteHandler = std::function<MemResponse(const MemAccess& access)>;
    using TickHandler = std::function<void(uint64_t cycles)>;

    Device();
    virtual ~Device() = default;

    MemResponse read(const MemAccess& access);
    MemResponse write(const MemAccess& access);
    void tick(uint64_t cycles);
    virtual void sync(uint64_t currentCycle);
    DeviceType getType() const;

    void setReadHandler(ReadHandler handler);
    void setWriteHandler(WriteHandler handler);
    void setTickHandler(TickHandler handler);
    void setType(DeviceType type);
    void setSyncThreshold(uint64_t threshold);
    
    virtual uint32_t getUpdateFrequency() const { return 0; }

protected:
    uint64_t mLastSyncCycle = 0;
    uint64_t mSyncThreshold = 128;

private:
    ReadHandler mReadHandler;
    WriteHandler mWriteHandler;
    TickHandler mTickHandler;
    DeviceType mType = DeviceType::Other;
};

#endif
