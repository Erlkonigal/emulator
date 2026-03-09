#include "emulator/debug/input/network_input_handler.h"
#include "emulator/commit/commit_thread.h"
#include "emulator/debug/debugger.h"
#include "emulator/log/logger.h"
#include "emulator/log/trace_manager.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>

namespace {
void sendLine(int fd, const std::string& line) {
    std::string data = line + "\n";
    send(fd, data.c_str(), data.size(), MSG_NOSIGNAL);
}
} // namespace

void NetworkInputHandler::broadcast(const std::string& message) {
    std::lock_guard<std::mutex> lock(mClientMutex);
    if (mClientFd >= 0) {
        std::string data = message;
        if (!data.empty() && data.back() == '\n') {
            data.back() = '\r';
            data += '\n';
        }
        send(mClientFd, data.c_str(), data.size(), MSG_NOSIGNAL);
    }
}

bool NetworkInputHandler::start(uint16_t port) {
    if (isRunning()) {
        return true;
    }

    mPort = port;
    mNotifiedHalt = false;

    mListenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (mListenFd < 0) {
        ERROR("NetworkInputHandler: failed to create socket: %s", strerror(errno));
        return false;
    }

    int opt = 1;
    setsockopt(mListenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(mListenFd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        ERROR("NetworkInputHandler: failed to bind port %u: %s", port, strerror(errno));
        close(mListenFd);
        mListenFd = -1;
        return false;
    }

    if (listen(mListenFd, 1) < 0) {
        ERROR("NetworkInputHandler: failed to listen: %s", strerror(errno));
        close(mListenFd);
        mListenFd = -1;
        return false;
    }

    int flags = fcntl(mListenFd, F_GETFL, 0);
    fcntl(mListenFd, F_SETFL, flags | O_NONBLOCK);

    setStarted(true);
    setRunning(true);
    mThread = std::thread(&NetworkInputHandler::serverThread, this);

    auto& debugger = Debugger::getInstance();
    debugger.setOutputHandler([this](const std::string& msg) { broadcast(msg); });
    Logger::getInstance().setHandler([this](const char* msg) { broadcast(msg); });
    
    Tracer* itrace = TraceManager::getInstance().getTracer("itrace");
    if (itrace) {
        itrace->setHandler([this](const char* msg) { broadcast(msg); });
    }

    INFO("NetworkInputHandler: listening on port %u", port);
    return true;
}

void NetworkInputHandler::stop() {
    setRunning(false);
    Logger::getInstance().setHandler(nullptr);
    
    Tracer* itrace = TraceManager::getInstance().getTracer("itrace");
    if (itrace) {
        itrace->setHandler(nullptr);
    }
    
    Debugger::getInstance().setOutputHandler(nullptr);
    joinThread();
    {
        std::lock_guard<std::mutex> lock(mClientMutex);
        if (mClientFd >= 0) {
            close(mClientFd);
            mClientFd = -1;
        }
    }
    if (mListenFd >= 0) {
        close(mListenFd);
        mListenFd = -1;
    }
    setStarted(false);
}

void NetworkInputHandler::threadLoop() {
}

void NetworkInputHandler::serverThread() {
    while (isRunning()) {
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        int clientFd = accept(mListenFd, (struct sockaddr*)&clientAddr, &clientLen);

        if (clientFd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            if (isRunning()) {
                WARN("NetworkInputHandler: accept failed: %s", strerror(errno));
            }
            continue;
        }

        char clientIp[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, clientIp, sizeof(clientIp));
        INFO("NetworkInputHandler: client connected from %s:%u", clientIp,
             ntohs(clientAddr.sin_port));

        mNotifiedHalt = false;
        handleClient(clientFd);

        {
            std::lock_guard<std::mutex> lock(mClientMutex);
            if (mClientFd >= 0) {
                close(mClientFd);
                mClientFd = -1;
            }
        }
        INFO("NetworkInputHandler: client disconnected");
    }
}

void NetworkInputHandler::handleClient(int clientFd) {
    auto& debugger = Debugger::getInstance();
    debugger.reset();

    {
        std::lock_guard<std::mutex> lock(mClientMutex);
        mClientFd = clientFd;
    }

    int flags = fcntl(clientFd, F_GETFL, 0);
    fcntl(clientFd, F_SETFL, flags | O_NONBLOCK);

    sendLine(clientFd, "NetworkInputHandler connected. Type 'help' for commands.");
    send(clientFd, "dbg> ", 5, MSG_NOSIGNAL);

    std::string line;
    char ch;
    while (isRunning()) {
        auto state = CommitThread::getInstance().getState();
        if (state == CommitThreadState::Halted && !mNotifiedHalt) {
            sendLine(clientFd, "[CPU halted - program finished]");
            mNotifiedHalt = true;
            send(clientFd, "dbg> ", 5, MSG_NOSIGNAL);
        }

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(clientFd, &readfds);
        struct timeval tv = {0, 10000};
        int ret = select(clientFd + 1, &readfds, nullptr, nullptr, &tv);

        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }

        if (ret == 0) {
            continue;
        }

        ssize_t n = recv(clientFd, &ch, 1, 0);
        if (n <= 0) {
            if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                continue;
            }
            break;
        }

        if (ch == '\n' || ch == '\r') {
            if (!line.empty()) {
                debugger.processCommand(line);
                line.clear();

                if (debugger.wasQuitRequested()) {
                    sendLine(clientFd, "Goodbye.");
                    break;
                }
            }

            send(clientFd, "dbg> ", 5, MSG_NOSIGNAL);
        } else if (ch >= 32 && ch < 127) {
            line += ch;
        }
    }

    {
        std::lock_guard<std::mutex> lock(mClientMutex);
        mClientFd = -1;
    }
}