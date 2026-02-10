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
    access.address = address;
    access.size = size;
    access.type = type;
    access.data = data;
    return access;
}

} // namespace

void RegisterDeviceTests() {
}

TEST(device_memory_rw) {
    MemoryDevice ram(16, false);
    MemAccess w = MakeAccess(4, 4, MemAccessType::Write, 0x11223344u);
    MemResponse wr = ram.write(w);
    ASSERT_TRUE(wr.success);

    MemAccess r = MakeAccess(4, 4, MemAccessType::Read);
    MemResponse rr = ram.read(r);
    ASSERT_TRUE(rr.success);
    EXPECT_EQ(static_cast<uint32_t>(rr.data & 0xffffffffu), 0x11223344u);
}

TEST(device_memory_oob) {
    MemoryDevice ram(8, false);
    MemAccess r = MakeAccess(6, 4, MemAccessType::Read);
    MemResponse rr = ram.read(r);
    EXPECT_EQ(rr.success, false);
    EXPECT_EQ(rr.error.type, CpuErrorType::AccessFault);
}

TEST(device_memory_rom_write_fault) {
    MemoryDevice rom(8, true);
    MemAccess w = MakeAccess(0, 4, MemAccessType::Write, 0xdeadbeefu);
    MemResponse wr = rom.write(w);
    EXPECT_EQ(wr.success, false);
    EXPECT_EQ(wr.error.type, CpuErrorType::AccessFault);
}

TEST(device_uart_status_rx) {
    UartDevice uart;
    MemAccess status = MakeAccess(0x4, 4, MemAccessType::Read);
    MemResponse r0 = uart.read(status);
    ASSERT_TRUE(r0.success);
    EXPECT_TRUE((r0.data & 0x1u) == 0);

    uart.pushRx('A');
    MemResponse r1 = uart.read(status);
    ASSERT_TRUE(r1.success);
    EXPECT_TRUE((r1.data & 0x1u) != 0);
}

TEST(device_uart_rx_data) {
    UartDevice uart;
    uart.pushRx('H');
    uart.pushRx('i');

    MemAccess data = MakeAccess(0x0, 4, MemAccessType::Read);
    MemResponse r0 = uart.read(data);
    MemResponse r1 = uart.read(data);
    ASSERT_TRUE(r0.success);
    ASSERT_TRUE(r1.success);
    EXPECT_EQ(static_cast<uint32_t>(r0.data & 0xffu), static_cast<uint32_t>('H'));
    EXPECT_EQ(static_cast<uint32_t>(r1.data & 0xffu), static_cast<uint32_t>('i'));
}

TEST(device_uart_invalid_access) {
    UartDevice uart;
    MemAccess bad = MakeAccess(0x2, 2, MemAccessType::Read);
    MemResponse r = uart.read(bad);
    EXPECT_EQ(r.success, false);
    EXPECT_EQ(r.error.type, CpuErrorType::AccessFault);
}

TEST(device_display_headless_regs) {
    SdlDisplayDevice display;
    ASSERT_TRUE(display.initHeadless(8, 4));

    MemAccess width = MakeAccess(0x04, 4, MemAccessType::Read);
    MemResponse w = display.read(width);
    ASSERT_TRUE(w.success);
    EXPECT_EQ(static_cast<uint32_t>(w.data & 0xffffffffu), 8u);

    MemAccess height = MakeAccess(0x08, 4, MemAccessType::Read);
    MemResponse h = display.read(height);
    ASSERT_TRUE(h.success);
    EXPECT_EQ(static_cast<uint32_t>(h.data & 0xffffffffu), 4u);
}

TEST(device_display_dirty_and_present) {
    SdlDisplayDevice display;
    ASSERT_TRUE(display.initHeadless(4, 4));

    MemAccess status = MakeAccess(0x10, 4, MemAccessType::Read);
    MemResponse s0 = display.read(status);
    ASSERT_TRUE(s0.success);
    EXPECT_TRUE((s0.data & 0x1u) != 0);

    MemAccess fb = MakeAccess(SdlDisplayDevice::kFrameBufferOffset, 4, MemAccessType::Write,
        0x11223344u);
    MemResponse w = display.write(fb);
    ASSERT_TRUE(w.success);

    MemResponse s1 = display.read(status);
    ASSERT_TRUE(s1.success);
    EXPECT_TRUE((s1.data & 0x2u) != 0);

    MemAccess ctrl = MakeAccess(0x00, 4, MemAccessType::Write, 1u);
    MemResponse c = display.write(ctrl);
    ASSERT_TRUE(c.success);
    EXPECT_TRUE(display.consumePresentRequest());
}

TEST(device_display_keyboard_queue) {
    SdlDisplayDevice display;
    ASSERT_TRUE(display.initHeadless(4, 4));

    display.pushKey('A');
    display.pushKey('B');

    MemAccess status = MakeAccess(0x24, 4, MemAccessType::Read);
    MemResponse s0 = display.read(status);
    ASSERT_TRUE(s0.success);
    EXPECT_TRUE((s0.data & 0x1u) != 0);

    MemAccess keyData = MakeAccess(0x20, 4, MemAccessType::Read);
    MemResponse k0 = display.read(keyData);
    MemResponse k1 = display.read(keyData);
    ASSERT_TRUE(k0.success);
    ASSERT_TRUE(k1.success);
    EXPECT_EQ(static_cast<uint32_t>(k0.data & 0xffffffffu), static_cast<uint32_t>('A'));
    EXPECT_EQ(static_cast<uint32_t>(k1.data & 0xffffffffu), static_cast<uint32_t>('B'));

    MemAccess keyLast = MakeAccess(0x28, 4, MemAccessType::Read);
    MemResponse kl = display.read(keyLast);
    ASSERT_TRUE(kl.success);
    EXPECT_EQ(static_cast<uint32_t>(kl.data & 0xffffffffu), static_cast<uint32_t>('B'));

    MemAccess clear = MakeAccess(0x24, 4, MemAccessType::Write, 0u);
    MemResponse cl = display.write(clear);
    ASSERT_TRUE(cl.success);

    MemResponse s1 = display.read(status);
    ASSERT_TRUE(s1.success);
    EXPECT_TRUE((s1.data & 0x1u) == 0);
}

TEST(device_display_oob) {
    SdlDisplayDevice display;
    ASSERT_TRUE(display.initHeadless(2, 2));

    uint64_t invalid = display.getMappedSize() + 4;
    MemAccess bad = MakeAccess(invalid, 4, MemAccessType::Read);
    MemResponse r = display.read(bad);
    EXPECT_EQ(r.success, false);
    EXPECT_EQ(r.error.type, CpuErrorType::AccessFault);
}

TEST(device_timer_large_tick) {
    TimerDevice timer;
    timer.tick(4294968296ULL);
}

TEST(device_sync_threshold) {
    class MockDevice : public Device {
    public:
        uint64_t mTickedCycles = 0;
        MockDevice() {
            setTickHandler([this](uint64_t cycles) {
                mTickedCycles += cycles;
            });
            setSyncThreshold(100);
        }
    };

    MockDevice dev;
    dev.sync(50);
    EXPECT_EQ(dev.mTickedCycles, 0u);

    dev.sync(150);
    EXPECT_EQ(dev.mTickedCycles, 150u);
    
    dev.sync(200);
    EXPECT_EQ(dev.mTickedCycles, 150u);

    dev.sync(300);
    EXPECT_EQ(dev.mTickedCycles, 150u + 150u);
}
