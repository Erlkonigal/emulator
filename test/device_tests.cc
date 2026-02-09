#include "test_framework.h"

#include <string>

#include "emulator/device/device.h"
#include "emulator/device/memory.h"
#include "emulator/device/timer.h"
#include "emulator/device/uart.h"
#include "emulator/device/display.h"

namespace {

MemAccess MakeAccess(uint64_t address, uint32_t size, MemAccessType type, uint64_t data = 0) {
    MemAccess access;
    access.Address = address;
    access.Size = size;
    access.Type = type;
    access.Data = data;
    return access;
}

} // namespace

void RegisterDeviceTests() {
}

TEST(device_memory_rw) {
    MemoryDevice ram(16, false);
    MemAccess w = MakeAccess(4, 4, MemAccessType::Write, 0x11223344u);
    MemResponse wr = ram.Write(w);
    ASSERT_TRUE(wr.Success);

    MemAccess r = MakeAccess(4, 4, MemAccessType::Read);
    MemResponse rr = ram.Read(r);
    ASSERT_TRUE(rr.Success);
    EXPECT_EQ(static_cast<uint32_t>(rr.Data & 0xffffffffu), 0x11223344u);
}

TEST(device_memory_oob) {
    MemoryDevice ram(8, false);
    MemAccess r = MakeAccess(6, 4, MemAccessType::Read);
    MemResponse rr = ram.Read(r);
    EXPECT_EQ(rr.Success, false);
    EXPECT_EQ(rr.Error.Type, CpuErrorType::AccessFault);
}

TEST(device_memory_rom_write_fault) {
    MemoryDevice rom(8, true);
    MemAccess w = MakeAccess(0, 4, MemAccessType::Write, 0xdeadbeefu);
    MemResponse wr = rom.Write(w);
    EXPECT_EQ(wr.Success, false);
    EXPECT_EQ(wr.Error.Type, CpuErrorType::AccessFault);
}

TEST(device_uart_status_rx) {
    UartDevice uart;
    MemAccess status = MakeAccess(0x4, 4, MemAccessType::Read);
    MemResponse r0 = uart.Read(status);
    ASSERT_TRUE(r0.Success);
    EXPECT_TRUE((r0.Data & 0x1u) == 0);

    uart.PushRx('A');
    MemResponse r1 = uart.Read(status);
    ASSERT_TRUE(r1.Success);
    EXPECT_TRUE((r1.Data & 0x1u) != 0);
}

TEST(device_uart_rx_data) {
    UartDevice uart;
    uart.PushRx('H');
    uart.PushRx('i');

    MemAccess data = MakeAccess(0x0, 4, MemAccessType::Read);
    MemResponse r0 = uart.Read(data);
    MemResponse r1 = uart.Read(data);
    ASSERT_TRUE(r0.Success);
    ASSERT_TRUE(r1.Success);
    EXPECT_EQ(static_cast<uint32_t>(r0.Data & 0xffu), static_cast<uint32_t>('H'));
    EXPECT_EQ(static_cast<uint32_t>(r1.Data & 0xffu), static_cast<uint32_t>('i'));
}

TEST(device_uart_tx_callback) {
    UartDevice uart;
    std::string captured;
    uart.SetTxHandler([&](const std::string& text) { captured += text; });

    MemAccess data = MakeAccess(0x0, 4, MemAccessType::Write, 'Z');
    MemResponse w = uart.Write(data);
    ASSERT_TRUE(w.Success);
    uart.Flush();
    uart.SetTxHandler(nullptr);

    EXPECT_TRUE(captured.find("Z") != std::string::npos);
}

TEST(device_uart_invalid_access) {
    UartDevice uart;
    MemAccess bad = MakeAccess(0x2, 2, MemAccessType::Read);
    MemResponse r = uart.Read(bad);
    EXPECT_EQ(r.Success, false);
    EXPECT_EQ(r.Error.Type, CpuErrorType::AccessFault);
}

TEST(device_display_headless_regs) {
    SdlDisplayDevice display;
    ASSERT_TRUE(display.InitHeadless(8, 4));

    MemAccess width = MakeAccess(0x04, 4, MemAccessType::Read);
    MemResponse w = display.Read(width);
    ASSERT_TRUE(w.Success);
    EXPECT_EQ(static_cast<uint32_t>(w.Data & 0xffffffffu), 8u);

    MemAccess height = MakeAccess(0x08, 4, MemAccessType::Read);
    MemResponse h = display.Read(height);
    ASSERT_TRUE(h.Success);
    EXPECT_EQ(static_cast<uint32_t>(h.Data & 0xffffffffu), 4u);
}

TEST(device_display_dirty_and_present) {
    SdlDisplayDevice display;
    ASSERT_TRUE(display.InitHeadless(4, 4));

    MemAccess status = MakeAccess(0x10, 4, MemAccessType::Read);
    MemResponse s0 = display.Read(status);
    ASSERT_TRUE(s0.Success);
    EXPECT_TRUE((s0.Data & 0x1u) != 0);

    MemAccess fb = MakeAccess(SdlDisplayDevice::kFrameBufferOffset, 4, MemAccessType::Write,
        0x11223344u);
    MemResponse w = display.Write(fb);
    ASSERT_TRUE(w.Success);

    MemResponse s1 = display.Read(status);
    ASSERT_TRUE(s1.Success);
    EXPECT_TRUE((s1.Data & 0x2u) != 0);

    MemAccess ctrl = MakeAccess(0x00, 4, MemAccessType::Write, 1u);
    MemResponse c = display.Write(ctrl);
    ASSERT_TRUE(c.Success);
    EXPECT_TRUE(display.ConsumePresentRequest());
}

TEST(device_display_keyboard_queue) {
    SdlDisplayDevice display;
    ASSERT_TRUE(display.InitHeadless(4, 4));

    display.PushKey('A');
    display.PushKey('B');

    MemAccess status = MakeAccess(0x24, 4, MemAccessType::Read);
    MemResponse s0 = display.Read(status);
    ASSERT_TRUE(s0.Success);
    EXPECT_TRUE((s0.Data & 0x1u) != 0);

    MemAccess keyData = MakeAccess(0x20, 4, MemAccessType::Read);
    MemResponse k0 = display.Read(keyData);
    MemResponse k1 = display.Read(keyData);
    ASSERT_TRUE(k0.Success);
    ASSERT_TRUE(k1.Success);
    EXPECT_EQ(static_cast<uint32_t>(k0.Data & 0xffffffffu), static_cast<uint32_t>('A'));
    EXPECT_EQ(static_cast<uint32_t>(k1.Data & 0xffffffffu), static_cast<uint32_t>('B'));

    MemAccess keyLast = MakeAccess(0x28, 4, MemAccessType::Read);
    MemResponse kl = display.Read(keyLast);
    ASSERT_TRUE(kl.Success);
    EXPECT_EQ(static_cast<uint32_t>(kl.Data & 0xffffffffu), static_cast<uint32_t>('B'));

    MemAccess clear = MakeAccess(0x24, 4, MemAccessType::Write, 0u);
    MemResponse cl = display.Write(clear);
    ASSERT_TRUE(cl.Success);

    MemResponse s1 = display.Read(status);
    ASSERT_TRUE(s1.Success);
    EXPECT_TRUE((s1.Data & 0x1u) == 0);
}

TEST(device_display_oob) {
    SdlDisplayDevice display;
    ASSERT_TRUE(display.InitHeadless(2, 2));

    uint64_t invalid = display.GetMappedSize() + 4;
    MemAccess bad = MakeAccess(invalid, 4, MemAccessType::Read);
    MemResponse r = display.Read(bad);
    EXPECT_EQ(r.Success, false);
    EXPECT_EQ(r.Error.Type, CpuErrorType::AccessFault);
}

TEST(device_timer_large_tick) {
    TimerDevice timer;
    // Tick with a very large value that would overflow uint32_t (4B + 1000)
    // 4294967296 + 1000 = 4294968296
    timer.Tick(4294968296ULL);
    
    // We can't easily verify internal state without friend access or exposing PendingCycles
    // But we can verify it doesn't crash and GetCounterMicros returns something reasonable
    // if we mock the clock, but TimerDevice uses steady_clock directly.
    // At least ensure it compiles and runs.
}

TEST(device_sync_threshold) {
    class MockDevice : public Device {
    public:
        uint64_t TickedCycles = 0;
        MockDevice() {
            SetTickHandler([this](uint64_t cycles) {
                TickedCycles += cycles;
            });
            SetSyncThreshold(100);
        }
    };

    MockDevice dev;
    dev.Sync(50); // Delta 50 < 100
    EXPECT_EQ(dev.TickedCycles, 0u);

    dev.Sync(150); // Delta 150 >= 100 (since last sync at 0)
    EXPECT_EQ(dev.TickedCycles, 150u);
    
    dev.Sync(200); // Delta 50 < 100 (since last sync at 150)
    EXPECT_EQ(dev.TickedCycles, 150u);

    dev.Sync(300); // Delta 100 >= 100 (since last sync at 150)
    EXPECT_EQ(dev.TickedCycles, 150u + 150u);
}
