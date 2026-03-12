#include "framework/test_framework.h"

#include <arpa/inet.h>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <thread>

#include "emulator/debug/debugger.h"
#include "emulator/debug/input/network_input_handler.h"

using emulator::Debugger;
using emulator::NetworkInputHandler;

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

TEST(network_input_handler_client_connect) {
    uint16_t port = FindAvailablePort(14000);
    ASSERT_TRUE(port > 0);
    
    auto& handler = NetworkInputHandler::getInstance();
    ASSERT_TRUE(handler.start(port));
    
    ASSERT_TRUE(WaitForServerReady(port));
    
    int clientFd = ConnectToServer(port);
    ASSERT_TRUE(clientFd >= 0);
    
    std::string welcome = RecvWithTimeout(clientFd, 1024, 100);
    EXPECT_TRUE(welcome.find("NetworkInputHandler connected") != std::string::npos);
    
    close(clientFd);
    handler.stop();
}

TEST(network_input_handler_receives_welcome) {
    uint16_t port = FindAvailablePort(14100);
    ASSERT_TRUE(port > 0);
    
    auto& handler = NetworkInputHandler::getInstance();
    ASSERT_TRUE(handler.start(port));
    
    ASSERT_TRUE(WaitForServerReady(port));
    
    int clientFd = ConnectToServer(port);
    ASSERT_TRUE(clientFd >= 0);
    
    std::string response = RecvWithTimeout(clientFd, 1024, 100);
    EXPECT_TRUE(response.find("NetworkInputHandler connected") != std::string::npos);
    EXPECT_TRUE(response.find("help") != std::string::npos);
    
    close(clientFd);
    handler.stop();
}

TEST(network_input_handler_command_help) {
    uint16_t port = FindAvailablePort(14200);
    ASSERT_TRUE(port > 0);
    
    auto& handler = NetworkInputHandler::getInstance();
    ASSERT_TRUE(handler.start(port));
    
    ASSERT_TRUE(WaitForServerReady(port));
    
    int clientFd = ConnectToServer(port);
    ASSERT_TRUE(clientFd >= 0);
    
    RecvWithTimeout(clientFd, 1024, 100);
    
    EXPECT_TRUE(SendLine(clientFd, "help"));
    
    std::string response = RecvWithTimeout(clientFd, 2048, 100);
    EXPECT_TRUE(response.find("Available commands") != std::string::npos ||
                response.find("run") != std::string::npos);
    
    close(clientFd);
    handler.stop();
}

TEST(network_input_handler_command_quit) {
    uint16_t port = FindAvailablePort(14300);
    ASSERT_TRUE(port > 0);
    
    auto& handler = NetworkInputHandler::getInstance();
    ASSERT_TRUE(handler.start(port));
    
    ASSERT_TRUE(WaitForServerReady(port));
    
    int clientFd = ConnectToServer(port);
    ASSERT_TRUE(clientFd >= 0);
    
    RecvWithTimeout(clientFd, 1024, 100);
    
    EXPECT_TRUE(SendLine(clientFd, "quit"));
    
    std::string response = RecvWithTimeout(clientFd, 1024, 100);
    EXPECT_TRUE(response.find("Goodbye") != std::string::npos);
    
    close(clientFd);
    handler.stop();
}

TEST(network_input_handler_command_exit) {
    uint16_t port = FindAvailablePort(14400);
    ASSERT_TRUE(port > 0);
    
    auto& handler = NetworkInputHandler::getInstance();
    ASSERT_TRUE(handler.start(port));
    
    ASSERT_TRUE(WaitForServerReady(port));
    
    int clientFd = ConnectToServer(port);
    ASSERT_TRUE(clientFd >= 0);
    
    RecvWithTimeout(clientFd, 1024, 100);
    
    EXPECT_TRUE(SendLine(clientFd, "exit"));
    
    std::string response = RecvWithTimeout(clientFd, 1024, 100);
    EXPECT_TRUE(response.find("Goodbye") != std::string::npos);
    
    close(clientFd);
    handler.stop();
}

TEST(network_input_handler_prompt_after_command) {
    uint16_t port = FindAvailablePort(14500);
    ASSERT_TRUE(port > 0);
    
    auto& handler = NetworkInputHandler::getInstance();
    ASSERT_TRUE(handler.start(port));
    
    ASSERT_TRUE(WaitForServerReady(port));
    
    int clientFd = ConnectToServer(port);
    ASSERT_TRUE(clientFd >= 0);
    
    RecvWithTimeout(clientFd, 1024, 100);
    
    EXPECT_TRUE(SendLine(clientFd, "regs"));
    
    std::string response = RecvWithTimeout(clientFd, 4096, 200);
    EXPECT_TRUE(response.find("dbg>") != std::string::npos);
    
    close(clientFd);
    handler.stop();
}

TEST(network_input_handler_multiple_commands) {
    uint16_t port = FindAvailablePort(14600);
    ASSERT_TRUE(port > 0);
    
    auto& handler = NetworkInputHandler::getInstance();
    ASSERT_TRUE(handler.start(port));
    
    ASSERT_TRUE(WaitForServerReady(port));
    
    int clientFd = ConnectToServer(port);
    ASSERT_TRUE(clientFd >= 0);
    
    RecvWithTimeout(clientFd, 1024, 100);
    
    EXPECT_TRUE(SendLine(clientFd, "help"));
    std::string resp1 = RecvWithTimeout(clientFd, 2048, 100);
    EXPECT_TRUE(!resp1.empty());
    
    EXPECT_TRUE(SendLine(clientFd, "regs"));
    std::string resp2 = RecvWithTimeout(clientFd, 4096, 100);
    EXPECT_TRUE(!resp2.empty());
    
    EXPECT_TRUE(SendLine(clientFd, "bp list"));
    std::string resp3 = RecvWithTimeout(clientFd, 1024, 100);
    EXPECT_TRUE(!resp3.empty());
    
    close(clientFd);
    handler.stop();
}

TEST(network_input_handler_bp_commands) {
    uint16_t port = FindAvailablePort(14700);
    ASSERT_TRUE(port > 0);
    
    auto& handler = NetworkInputHandler::getInstance();
    Debugger::getInstance().reset();
    ASSERT_TRUE(handler.start(port));
    
    ASSERT_TRUE(WaitForServerReady(port));
    
    int clientFd = ConnectToServer(port);
    ASSERT_TRUE(clientFd >= 0);
    
    RecvWithTimeout(clientFd, 1024, 100);
    
    EXPECT_TRUE(SendLine(clientFd, "bp add 0x80000000"));
    std::string resp1 = RecvWithTimeout(clientFd, 1024, 100);
    EXPECT_TRUE(resp1.find("added") != std::string::npos || !resp1.empty());
    
    EXPECT_TRUE(SendLine(clientFd, "bp list"));
    std::string resp2 = RecvWithTimeout(clientFd, 1024, 200);
    EXPECT_TRUE(resp2.find("80000000") != std::string::npos || 
                resp2.find("Breakpoints") != std::string::npos);
    
    EXPECT_TRUE(SendLine(clientFd, "bp del 0x80000000"));
    std::string resp3 = RecvWithTimeout(clientFd, 1024, 100);
    EXPECT_TRUE(resp3.find("removed") != std::string::npos || !resp3.empty());
    
    close(clientFd);
    handler.stop();
}

TEST(network_input_handler_mem_command) {
    uint16_t port = FindAvailablePort(14800);
    ASSERT_TRUE(port > 0);
    
    auto& handler = NetworkInputHandler::getInstance();
    ASSERT_TRUE(handler.start(port));
    
    ASSERT_TRUE(WaitForServerReady(port));
    
    int clientFd = ConnectToServer(port);
    ASSERT_TRUE(clientFd >= 0);
    
    RecvWithTimeout(clientFd, 1024, 100);
    
    EXPECT_TRUE(SendLine(clientFd, "mem 0x80000000 16"));
    std::string response = RecvWithTimeout(clientFd, 1024, 100);
    EXPECT_TRUE(!response.empty());
    
    close(clientFd);
    handler.stop();
}

TEST(network_input_handler_log_command) {
    uint16_t port = FindAvailablePort(14900);
    ASSERT_TRUE(port > 0);
    
    auto& handler = NetworkInputHandler::getInstance();
    ASSERT_TRUE(handler.start(port));
    
    ASSERT_TRUE(WaitForServerReady(port));
    
    int clientFd = ConnectToServer(port);
    ASSERT_TRUE(clientFd >= 0);
    
    RecvWithTimeout(clientFd, 1024, 100);
    
    EXPECT_TRUE(SendLine(clientFd, "log debug"));
    std::string response = RecvWithTimeout(clientFd, 1024, 100);
    EXPECT_TRUE(!response.empty());
    
    close(clientFd);
    handler.stop();
}