#include "framework/test_framework.h"

#include <arpa/inet.h>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstdint>
#include <thread>

#include "emulator/server/debug_server.h"
#include "emulator/generated/hardware_config.h"

namespace {

uint16_t FindAvailablePort(uint16_t start) {
    for (uint16_t port = start; port < start + 100; ++port) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) continue;
        
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
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

}

TEST(server_debug_server_start_stop) {
    uint16_t port = FindAvailablePort(13100);
    ASSERT_TRUE(port > 0);
    
    auto& server = DebugServer::getInstance();
    EXPECT_TRUE(!server.isRunning());
    
    bool started = server.start(port);
    ASSERT_TRUE(started);
    EXPECT_TRUE(server.isRunning());
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    server.stop();
    EXPECT_TRUE(!server.isRunning());
}

TEST(server_debug_server_port_binding) {
    uint16_t port = FindAvailablePort(13300);
    ASSERT_TRUE(port > 0);
    
    auto& server = DebugServer::getInstance();
    ASSERT_TRUE(server.start(port));
    
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_TRUE(fd >= 0);
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);
    
    bool connected = (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == 0);
    EXPECT_TRUE(connected);
    
    close(fd);
    server.stop();
}

TEST(server_debug_server_restart) {
    uint16_t port = FindAvailablePort(13600);
    ASSERT_TRUE(port > 0);
    
    auto& server = DebugServer::getInstance();
    
    ASSERT_TRUE(server.start(port));
    EXPECT_TRUE(server.isRunning());
    server.stop();
    EXPECT_TRUE(!server.isRunning());
    
    ASSERT_TRUE(server.start(port));
    EXPECT_TRUE(server.isRunning());
    server.stop();
    EXPECT_TRUE(!server.isRunning());
}