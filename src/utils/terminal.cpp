#include "emulator/utils/terminal.h"
#include "emulator/bus/uart.h"
#include "emulator/log/logger.h"

#include <fcntl.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>

#include <atomic>
#include <cstdio>
#include <cstring>

namespace {
std::atomic<bool>* gInterruptFlag = nullptr;
std::atomic<bool> gInterrupted{false};
Terminal* gTerminal = nullptr;

void signalHandler(int) {
    gInterrupted.store(true, std::memory_order_release);
    if (gInterruptFlag) {
        gInterruptFlag->store(true, std::memory_order_release);
    }
}
}

void Terminal::setup() {
    if (mConfigured) {
        return;
    }

    termios* original = new termios;
    mOriginalTermios = original;

    if (tcgetattr(STDIN_FILENO, original) < 0) {
        ERROR("Terminal: tcgetattr failed: %s", strerror(errno));
        delete original;
        mOriginalTermios = nullptr;
        return;
    }

    termios raw = *original;
    raw.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    // raw.c_oflag &= ~OPOST;
    raw.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    raw.c_cflag &= ~(CSIZE | PARENB);
    raw.c_cflag |= CS8;
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) < 0) {
        ERROR("Terminal: tcsetattr failed: %s", strerror(errno));
        delete original;
        mOriginalTermios = nullptr;
        return;
    }

    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    flags = fcntl(STDOUT_FILENO, F_GETFL, 0);
    fcntl(STDOUT_FILENO, F_SETFL, flags | O_NONBLOCK);

    gTerminal = this;
    gInterrupted.store(false, std::memory_order_release);

    struct sigaction sa;
    sa.sa_handler = signalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);

    mConfigured = true;
    INFO("Terminal: configured raw mode");
}

void Terminal::restore() {
    if (!mConfigured || mOriginalTermios == nullptr) {
        return;
    }

    termios* original = static_cast<termios*>(mOriginalTermios);
    tcsetattr(STDIN_FILENO, TCSANOW, original);

    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK);

    flags = fcntl(STDOUT_FILENO, F_GETFL, 0);
    fcntl(STDOUT_FILENO, F_SETFL, flags & ~O_NONBLOCK);

    delete original;
    mOriginalTermios = nullptr;
    mConfigured = false;

    struct sigaction sa;
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);

    gTerminal = nullptr;
    INFO("Terminal: restored original settings");
}

void Terminal::setInterruptFlag(std::atomic<bool>* flag) {
    mInterruptFlag = flag;
    gInterruptFlag = flag;
}

void Terminal::processIo() {
    if (!mConfigured) {
        return;
    }

    if (gInterrupted.load(std::memory_order_acquire)) {
        mInterrupted.store(true, std::memory_order_release);
        return;
    }

    auto& uart = Uart::getInstance();

    char buf[256];
    ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
    if (n > 0) {
        for (ssize_t i = 0; i < n; ++i) {
            uart.pushRx(static_cast<uint8_t>(buf[i]));
        }
    }

    while (uart.hasTxData()) {
        uint8_t byte;
        if (uart.popTx(&byte)) {
            if (byte == '\n') {
                const char* crlf = "\r\n";
                write(STDOUT_FILENO, crlf, 2);
            } else if (byte == '\r') {
                // skip
            } else if (byte == 0x7f || byte == 0x08) {
                const char* backspace = "\b \b";
                write(STDOUT_FILENO, backspace, 3);
            } else if (byte >= 32 && byte < 127) {
                char c = static_cast<char>(byte);
                write(STDOUT_FILENO, &c, 1);
            } else if (byte == '\t') {
                write(STDOUT_FILENO, "\t", 1);
            } else {
                char escaped[8];
                snprintf(escaped, sizeof(escaped), "^%c", byte + 64);
                write(STDOUT_FILENO, escaped, 2);
            }
        }
    }

    fsync(STDOUT_FILENO);
}