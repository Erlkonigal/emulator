#include "framework/test_framework.h"

#include <cstdint>
#include <cstring>

#include "emulator/device/uart.h"

using emulator::Uart;
using emulator::kUartFifoDepth;

TEST(uart_read_empty_rx_returns_zero) {
    Uart& uart = Uart::getInstance();
    
    uint8_t data[4] = {0xff, 0xff, 0xff, 0xff};
    bool ok = uart.read(0, data, 1);
    EXPECT_TRUE(ok);
    EXPECT_EQ(data[0], 0);
}

TEST(uart_write_tx_pushes_to_fifo) {
    Uart& uart = Uart::getInstance();
    
    uint8_t data = 0x41;
    bool ok = uart.write(0, &data, 1);
    EXPECT_TRUE(ok);
    
    uint8_t byte = 0;
    EXPECT_TRUE(uart.hasTxData());
    EXPECT_TRUE(uart.popTx(&byte));
    EXPECT_EQ(byte, 0x41);
}

TEST(uart_read_rx_pops_from_fifo) {
    Uart& uart = Uart::getInstance();
    
    uart.pushRx(0x42);
    uart.pushRx(0x43);
    
    uint8_t data = 0;
    bool ok = uart.read(0, &data, 1);
    EXPECT_TRUE(ok);
    EXPECT_EQ(data, 0x42);
    
    ok = uart.read(0, &data, 1);
    EXPECT_TRUE(ok);
    EXPECT_EQ(data, 0x43);
}

TEST(uart_status_tx_ready_when_not_full) {
    Uart& uart = Uart::getInstance();
    
    while (uart.hasTxData()) {
        uint8_t byte;
        uart.popTx(&byte);
    }
    
    uint8_t status = 0;
    bool ok = uart.read(4, &status, 1);
    EXPECT_TRUE(ok);
    EXPECT_TRUE((status & 0x01) != 0);
}

TEST(uart_status_tx_not_ready_when_full) {
    Uart& uart = Uart::getInstance();
    
    while (uart.hasTxData()) {
        uint8_t byte;
        uart.popTx(&byte);
    }
    
    for (size_t i = 0; i < kUartFifoDepth; ++i) {
        uint8_t data = static_cast<uint8_t>(i);
        uart.write(0, &data, 1);
    }
    
    uint8_t status = 0;
    bool ok = uart.read(4, &status, 1);
    EXPECT_TRUE(ok);
    EXPECT_TRUE((status & 0x01) == 0);
    
    while (uart.hasTxData()) {
        uint8_t byte;
        uart.popTx(&byte);
    }
}

TEST(uart_status_rx_ready_when_has_data) {
    Uart& uart = Uart::getInstance();
    
    while (uart.hasRxData()) {
        uint8_t data;
        uart.read(0, &data, 1);
    }
    
    uart.pushRx(0x55);
    
    uint8_t status = 0;
    bool ok = uart.read(4, &status, 1);
    EXPECT_TRUE(ok);
    EXPECT_TRUE((status & 0x02) != 0);
    
    uint8_t data;
    uart.read(0, &data, 1);
}

TEST(uart_status_rx_not_ready_when_empty) {
    Uart& uart = Uart::getInstance();
    
    while (uart.hasRxData()) {
        uint8_t data;
        uart.read(0, &data, 1);
    }
    
    uint8_t status = 0;
    bool ok = uart.read(4, &status, 1);
    EXPECT_TRUE(ok);
    EXPECT_TRUE((status & 0x02) == 0);
}

TEST(uart_rx_fifo_overflow_drops_byte) {
    Uart& uart = Uart::getInstance();
    
    while (uart.hasRxData()) {
        uint8_t data;
        uart.read(0, &data, 1);
    }
    
    for (size_t i = 0; i < kUartFifoDepth + 5; ++i) {
        uart.pushRx(static_cast<uint8_t>(i));
    }
    
    size_t count = 0;
    while (uart.hasRxData()) {
        uint8_t data;
        uart.read(0, &data, 1);
        ++count;
    }
    
    EXPECT_EQ(count, kUartFifoDepth);
}

TEST(uart_tx_fifo_overflow_drops_byte) {
    Uart& uart = Uart::getInstance();
    
    while (uart.hasTxData()) {
        uint8_t byte;
        uart.popTx(&byte);
    }
    
    for (size_t i = 0; i < kUartFifoDepth + 5; ++i) {
        uint8_t data = static_cast<uint8_t>(i);
        uart.write(0, &data, 1);
    }
    
    size_t count = 0;
    while (uart.hasTxData()) {
        uint8_t byte;
        uart.popTx(&byte);
        ++count;
    }
    
    EXPECT_EQ(count, kUartFifoDepth);
}

TEST(uart_push_rx_and_read_consistency) {
    Uart& uart = Uart::getInstance();
    
    while (uart.hasRxData()) {
        uint8_t data;
        uart.read(0, &data, 1);
    }
    
    uint8_t testData[] = {0x00, 0x7f, 0x80, 0xff, 'A', 'B', 'C'};
    for (uint8_t b : testData) {
        uart.pushRx(b);
    }
    
    for (uint8_t expected : testData) {
        uint8_t actual = 0;
        bool ok = uart.read(0, &actual, 1);
        EXPECT_TRUE(ok);
        EXPECT_EQ(actual, expected);
    }
}

TEST(uart_read_other_offsets_return_zero) {
    Uart& uart = Uart::getInstance();
    
    uint8_t data[4] = {0xff, 0xff, 0xff, 0xff};
    
    bool ok = uart.read(8, data, 4);
    EXPECT_TRUE(ok);
    EXPECT_EQ(data[0], 0);
    EXPECT_EQ(data[1], 0);
    EXPECT_EQ(data[2], 0);
    EXPECT_EQ(data[3], 0);
}

TEST(uart_write_other_offsets_ignored) {
    Uart& uart = Uart::getInstance();
    
    while (uart.hasTxData()) {
        uint8_t byte;
        uart.popTx(&byte);
    }
    
    uint8_t data = 0x55;
    bool ok = uart.write(8, &data, 1);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(!uart.hasTxData());
}

TEST(uart_pop_tx_empty_returns_false) {
    Uart& uart = Uart::getInstance();
    
    while (uart.hasTxData()) {
        uint8_t byte;
        uart.popTx(&byte);
    }
    
    uint8_t byte = 0;
    EXPECT_TRUE(!uart.popTx(&byte));
}