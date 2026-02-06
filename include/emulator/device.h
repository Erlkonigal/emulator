#ifndef EMULATOR_DEVICE_H
#define EMULATOR_DEVICE_H

#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include "emulator/bus.h"

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
    using TickHandler = std::function<void(uint32_t cycles)>;

    Device();
    ~Device() = default;

    MemResponse Read(const MemAccess& access);
    MemResponse Write(const MemAccess& access);
    void Tick(uint32_t cycles);
    DeviceType GetType() const;

    void SetReadHandler(ReadHandler handler);
    void SetWriteHandler(WriteHandler handler);
    void SetTickHandler(TickHandler handler);
    void SetType(DeviceType type);

private:
    ReadHandler ReadCallback;
    WriteHandler WriteCallback;
    TickHandler TickCallback;
    DeviceType Type = DeviceType::Other;
};

class MemoryDevice : public Device {
public:
    MemoryDevice(uint64_t size, bool readOnly);

    bool LoadImage(const std::string& path, uint64_t offset = 0);
    uint64_t GetSize() const;
    bool IsReadOnly() const;

private:
    MemResponse HandleRead(const MemAccess& access);
    MemResponse HandleWrite(const MemAccess& access);

    std::vector<uint8_t> Storage;
    bool ReadOnly = false;
};

class TimerDevice : public Device {
public:
    TimerDevice();

private:
    MemResponse HandleRead(const MemAccess& access);
    MemResponse HandleWrite(const MemAccess& access);
    void HandleTick(uint32_t cycles);

    uint64_t GetCounterMicros();
    void AccumulateCycles(uint32_t cycles);

    std::chrono::steady_clock::time_point LastTick;
    uint64_t AccumulatedMicros = 0;
    uint32_t PendingCycles = 0;
};

class UartDevice : public Device {
public:
    UartDevice();
    ~UartDevice();
    void PushRx(uint8_t ch);
    using TxHandler = std::function<void(const std::string&)>;
    void SetTxHandler(TxHandler handler);

private:
    MemResponse HandleRead(const MemAccess& access);
    MemResponse HandleWrite(const MemAccess& access);
    uint32_t GetStatus() const;
    void FlushTxLocked();

    mutable std::mutex Mutex;
    std::deque<uint8_t> RxBuffer;
    std::string TxBuffer;
    TxHandler TxCallback;
};

class SdlDisplayDevice : public Device {
public:
    static constexpr uint64_t kControlRegionSize = 0x1000;
    static constexpr uint64_t kFrameBufferOffset = kControlRegionSize;

    SdlDisplayDevice();
    ~SdlDisplayDevice();

    bool Init(uint32_t width, uint32_t height, const char* title);
    void Shutdown();
    bool IsReady() const;
    void PollEvents(uint32_t timeoutMs = 0);
    bool IsQuitRequested() const;

    uint32_t GetWidth() const;
    uint32_t GetHeight() const;
    uint32_t GetPitch() const;
    uint64_t GetFrameBufferSize() const;
    uint64_t GetMappedSize() const;
    bool IsDirty() const;
    bool IsPresentRequested() const;
    bool ConsumePresentRequest();
    void Present();

private:
    MemResponse HandleRead(const MemAccess& access);
    MemResponse HandleWrite(const MemAccess& access);
    bool ReadRegister(uint64_t offset, uint64_t* value) const;
    bool WriteRegister(uint64_t offset, uint64_t value);

    struct SdlDisplayState;
    SdlDisplayState* State = nullptr;
    std::mutex FrameMutex;
    mutable std::mutex InputMutex;
    mutable std::deque<uint32_t> KeyQueue;
    mutable uint32_t LastKey = 0;
    bool QuitRequested = false;
    std::atomic<bool> Dirty{false};
    std::atomic<bool> PresentRequested{false};
};

#endif
