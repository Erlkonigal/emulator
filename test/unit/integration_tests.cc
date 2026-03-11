#include "framework/test_framework.h"

#include <arpa/inet.h>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include "emulator/device/uart.h"
#include "emulator/debug/breakpoint.h"
#include "emulator/debug/debugger.h"
#include "emulator/debug/input/network_input_handler.h"
#include "helpers/stdout_capture.h"
#include "helpers/test_helpers.h"
#include "toy/rom_util.h"
#include "toy/toy_cpu_executor.h"
#include "toy/toy_isa.h"

void RegisterIntegrationTests() {}

TEST(integration_ram_rw) {
  std::vector<uint32_t> prog;

  toy::Emit(&prog, toy::Lui(1, 0x8000));
  toy::Emit(&prog, toy::Ori(1, 0x0100));
  toy::Emit(&prog, toy::Lui(2, 0x1122));
  toy::Emit(&prog, toy::Ori(2, 0x3344));
  toy::Emit(&prog, toy::Sw(2, 1, 0));
  toy::Emit(&prog, toy::Lw(3, 1, 0));
  toy::Emit(&prog, toy::Halt());

  std::string err;
  auto romPath = testutil::MakeRomPath("ram_rw");
  ASSERT_TRUE(rom::WriteRomU32LE(romPath, prog, &err));

  int rc = testutil::RunEmuWithRom(romPath, false, &err);
  ASSERT_EQ(rc, 0);

  ToyCpuExecutor *cpu = GetLastToyCpu();
  ASSERT_TRUE(cpu != nullptr);
  EXPECT_EQ(static_cast<uint32_t>(cpu->getRegister(3) & 0xffffffffu),
            0x11223344u);
}

TEST(integration_basic_compute) {
  std::vector<uint32_t> prog;

  toy::Emit(&prog, toy::Lui(1, 0x1234));
  toy::Emit(&prog, toy::Ori(1, 0x5678));
  toy::Emit(&prog, toy::Halt());

  std::string err;
  auto romPath = testutil::MakeRomPath("basic_compute");
  ASSERT_TRUE(rom::WriteRomU32LE(romPath, prog, &err));

  int rc = testutil::RunEmuWithRom(romPath, false, &err);
  ASSERT_EQ(rc, 0);

  ToyCpuExecutor *cpu = GetLastToyCpu();
  ASSERT_TRUE(cpu != nullptr);
  EXPECT_EQ(static_cast<uint32_t>(cpu->getRegister(1) & 0xffffffffu),
            0x12345678u);
}

TEST(integration_beq_taken) {
  std::vector<uint32_t> prog;

  toy::Emit(&prog, toy::Lui(1, 0x0001));
  toy::Emit(&prog, toy::Lui(2, 0x0001));
  toy::Emit(&prog, toy::Beq(1, 2, 1));
  toy::Emit(&prog, toy::Lui(3, 0x0000));
  toy::Emit(&prog, toy::Lui(3, 0x0001));
  toy::Emit(&prog, toy::Halt());

  std::string err;
  auto romPath = testutil::MakeRomPath("beq_taken");
  ASSERT_TRUE(rom::WriteRomU32LE(romPath, prog, &err));

  int rc = testutil::RunEmuWithRom(romPath, false, &err);
  ASSERT_EQ(rc, 0);

  ToyCpuExecutor *cpu = GetLastToyCpu();
  ASSERT_TRUE(cpu != nullptr);
}

TEST(integration_halt) {
  std::vector<uint32_t> prog;

  toy::Emit(&prog, toy::Halt());

  std::string err;
  auto romPath = testutil::MakeRomPath("halt");
  ASSERT_TRUE(rom::WriteRomU32LE(romPath, prog, &err));

  int rc = testutil::RunEmuWithRom(romPath, false, &err);
  ASSERT_EQ(rc, 0);

  ToyCpuExecutor *cpu = GetLastToyCpu();
  ASSERT_TRUE(cpu != nullptr);
  EXPECT_TRUE(testutil::CheckLastError(CpuErrorType::Halt));
}

namespace {

bool WaitForServerReady(uint16_t port, int maxAttempts = 50, int delayMs = 2) {
    for (int i = 0; i < maxAttempts; ++i) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
            continue;
        }
        struct sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = htons(port);
        if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            close(fd);
            return true;
        }
        close(fd);
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
    }
    return false;
}

uint16_t FindAvailablePort(uint16_t start) {
    for (uint16_t port = start; port < start + 100; ++port) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) continue;
        
        struct sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);
        
        if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            close(fd);
            return port;
        }
        close(fd);
    }
    return 0;
}

int ConnectToServer(uint16_t port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    
    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);
    
    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    
    return fd;
}

bool SendLine(int fd, const std::string& line) {
    std::string data = line + "\n";
    ssize_t sent = send(fd, data.c_str(), data.size(), 0);
    return sent == static_cast<ssize_t>(data.size());
}

std::string RecvWithTimeout(int fd, size_t maxLen, int timeoutMs) {
    std::string result;
    char buf[256];
    
    auto start = std::chrono::steady_clock::now();
    
    while (result.size() < maxLen) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
        if (elapsed.count() >= timeoutMs) break;
        
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);
        
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 1000;
        
        int ret = select(fd + 1, &readfds, nullptr, nullptr, &tv);
        if (ret <= 0) continue;
        
        ssize_t n = recv(fd, buf, std::min(sizeof(buf), maxLen - result.size()), 0);
        if (n <= 0) break;
        
        result.append(buf, n);
    }
    
    return result;
}

}

TEST(e2e_network_input_handler_help_command) {
    uint16_t port = FindAvailablePort(16200);
    ASSERT_TRUE(port > 0);
    
    auto& handler = NetworkInputHandler::getInstance();
    ASSERT_TRUE(handler.start(port));
    
    ASSERT_TRUE(WaitForServerReady(port));
    
    int clientFd = ConnectToServer(port);
    ASSERT_TRUE(clientFd >= 0);
    
    RecvWithTimeout(clientFd, 1024, 100);
    
    SendLine(clientFd, "help");
    
    std::string response = RecvWithTimeout(clientFd, 2048, 100);
    EXPECT_TRUE(response.find("Available commands") != std::string::npos ||
                response.find("run") != std::string::npos);
    
    close(clientFd);
    handler.stop();
}

TEST(e2e_network_input_handler_regs_command) {
    uint16_t port = FindAvailablePort(16300);
    ASSERT_TRUE(port > 0);
    
    auto& handler = NetworkInputHandler::getInstance();
    Debugger::getInstance().reset();
    
    ASSERT_TRUE(handler.start(port));
    ASSERT_TRUE(WaitForServerReady(port));
    
    int clientFd = ConnectToServer(port);
    ASSERT_TRUE(clientFd >= 0);
    
    RecvWithTimeout(clientFd, 1024, 100);
    
    SendLine(clientFd, "regs");
    
    std::string response = RecvWithTimeout(clientFd, 4096, 200);
    EXPECT_TRUE(response.find("r0") != std::string::npos);
    EXPECT_TRUE(response.find("r31") != std::string::npos || 
                response.find("dbg>") != std::string::npos);
    
    close(clientFd);
    handler.stop();
}

TEST(e2e_network_input_handler_bp_operations) {
    uint16_t port = FindAvailablePort(16400);
    ASSERT_TRUE(port > 0);
    
    auto& handler = NetworkInputHandler::getInstance();
    auto& dbg = Debugger::getInstance();
    auto& bpCtrl = BreakPointController::getInstance();
    dbg.reset();
    bpCtrl.reset();
    
    ASSERT_TRUE(handler.start(port));
    ASSERT_TRUE(WaitForServerReady(port));
    
    int clientFd = ConnectToServer(port);
    ASSERT_TRUE(clientFd >= 0);
    
    RecvWithTimeout(clientFd, 1024, 100);
    
    SendLine(clientFd, "bp add 0x80000000");
    std::string resp1 = RecvWithTimeout(clientFd, 1024, 100);
    
    SendLine(clientFd, "bp list");
    std::string resp2 = RecvWithTimeout(clientFd, 1024, 200);
    EXPECT_TRUE(resp2.find("80000000") != std::string::npos ||
                resp2.find("Breakpoints") != std::string::npos);
    
    SendLine(clientFd, "bp del 0x80000000");
    std::string resp3 = RecvWithTimeout(clientFd, 1024, 100);
    
    close(clientFd);
    handler.stop();
}

TEST(e2e_network_input_handler_mem_command) {
    uint16_t port = FindAvailablePort(16500);
    ASSERT_TRUE(port > 0);
    
auto& handler = NetworkInputHandler::getInstance();
    ASSERT_TRUE(handler.start(port));
    
    ASSERT_TRUE(WaitForServerReady(port));
    
    int clientFd = ConnectToServer(port);
    ASSERT_TRUE(clientFd >= 0);
    
    RecvWithTimeout(clientFd, 1024, 100);
    
    SendLine(clientFd, "mem 0x80000000 32");
    
    std::string response = RecvWithTimeout(clientFd, 1024, 100);
    EXPECT_TRUE(!response.empty());
    EXPECT_TRUE(response.find("80000000") != std::string::npos ||
                response.find("dbg>") != std::string::npos);
    
    close(clientFd);
    handler.stop();
}

TEST(e2e_network_input_handler_quit_disconnects) {
    uint16_t port = FindAvailablePort(17200);
    ASSERT_TRUE(port > 0);
    
    auto& handler = NetworkInputHandler::getInstance();
    ASSERT_TRUE(handler.start(port));
    
    ASSERT_TRUE(WaitForServerReady(port));
    
    int clientFd = ConnectToServer(port);
    ASSERT_TRUE(clientFd >= 0);
    
    RecvWithTimeout(clientFd, 1024, 100);
    
    SendLine(clientFd, "quit");
    
    std::string response = RecvWithTimeout(clientFd, 1024, 100);
    EXPECT_TRUE(response.find("Goodbye") != std::string::npos);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    close(clientFd);
    handler.stop();
}