#include "test_framework.h"

extern void RegisterIntegrationTests();
extern void RegisterDeviceTests();
extern void RegisterTraceTests();

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    RegisterIntegrationTests();
    RegisterDeviceTests();
    RegisterTraceTests();
    return testfw::RunAllTests();
}
