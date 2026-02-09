#ifndef EMULATOR_DEVICE_DEVICE_H
#define EMULATOR_DEVICE_DEVICE_H

#include <cstdint>
#include <functional>
#include <string>

#include "emulator/bus/bus.h" // Will be moved later, but let's assume new path for now or keep relative?
// Actually I need to update include paths everywhere.
// For now let's use the old path "emulator/bus/bus.h" and I will update it later.
// Or better, use the NEW path "emulator/bus/bus.h" and I will ensure bus.h is moved there.
#include "emulator/cpu/cpu.h" // For MemResponse/MemAccess (defined in cpu.h currently)

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

    MemResponse Read(const MemAccess& access);
    MemResponse Write(const MemAccess& access);
    void Tick(uint64_t cycles);
    virtual void Sync(uint64_t currentCycle);
    DeviceType GetType() const;

    void SetReadHandler(ReadHandler handler);
    void SetWriteHandler(WriteHandler handler);
    void SetTickHandler(TickHandler handler);
    void SetType(DeviceType type);
    void SetSyncThreshold(uint64_t threshold);
    
    virtual uint32_t GetUpdateFrequency() const { return 0; }

protected:
    uint64_t LastSyncCycle = 0;
    uint64_t SyncThreshold = 128;

private:
    ReadHandler ReadCallback;
    WriteHandler WriteCallback;
    TickHandler TickCallback;
    DeviceType Type = DeviceType::Other;
};

#endif
