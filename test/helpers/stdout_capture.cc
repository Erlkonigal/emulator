#include "stdout_capture.h"

#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace testutil {

StdoutCapture::StdoutCapture() = default;

StdoutCapture::~StdoutCapture() {
    if (Active) {
        std::string out;
        std::string err;
        (void)Stop(&out, &err);
    }
}

bool StdoutCapture::Start(std::string* error) {
    if (Active) {
        if (error != nullptr) {
            *error = "capture already active";
        }
        return false;
    }

    std::fflush(stdout);
    SavedFd = ::dup(STDOUT_FILENO);
    if (SavedFd < 0) {
        if (error != nullptr) {
            *error = "dup failed";
        }
        return false;
    }

    char tmpl[] = "/tmp/emulator_test_stdout_XXXXXX";
    TempFd = ::mkstemp(tmpl);
    if (TempFd < 0) {
        if (error != nullptr) {
            *error = "mkstemp failed";
        }
        ::close(SavedFd);
        SavedFd = -1;
        return false;
    }
    TempPath = tmpl;

    if (::dup2(TempFd, STDOUT_FILENO) < 0) {
        if (error != nullptr) {
            *error = "dup2 failed";
        }
        ::close(TempFd);
        ::close(SavedFd);
        TempFd = -1;
        SavedFd = -1;
        return false;
    }
    Active = true;
    return true;
}

bool StdoutCapture::Stop(std::string* out, std::string* error) {
    if (!Active) {
        if (error != nullptr) {
            *error = "capture not active";
        }
        return false;
    }

    std::fflush(stdout);
    if (::dup2(SavedFd, STDOUT_FILENO) < 0) {
        if (error != nullptr) {
            *error = "restore dup2 failed";
        }
        return false;
    }

    ::lseek(TempFd, 0, SEEK_SET);
    std::string data;
    char buf[4096];
    while (true) {
        ssize_t n = ::read(TempFd, buf, sizeof(buf));
        if (n < 0) {
            if (error != nullptr) {
                *error = "read failed";
            }
            break;
        }
        if (n == 0) {
            break;
        }
        data.append(buf, buf + n);
    }

    ::close(TempFd);
    ::close(SavedFd);
    TempFd = -1;
    SavedFd = -1;
    Active = false;

    if (!TempPath.empty()) {
        ::unlink(TempPath.c_str());
        TempPath.clear();
    }

    if (out != nullptr) {
        *out = std::move(data);
    }
    return true;
}

} // namespace testutil
