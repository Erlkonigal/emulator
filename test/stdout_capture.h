#ifndef TEST_STDOUT_CAPTURE_H
#define TEST_STDOUT_CAPTURE_H

#include <string>

namespace testutil {

class StdoutCapture {
public:
    StdoutCapture();
    ~StdoutCapture();

    bool Start(std::string* error);
    bool Stop(std::string* out, std::string* error);

private:
    int SavedFd = -1;
    int TempFd = -1;
    std::string TempPath;
    bool Active = false;
};

} // namespace testutil

#endif
