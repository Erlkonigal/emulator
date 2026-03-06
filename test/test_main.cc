#include "test_framework.h"

extern void RegisterIntegrationTests();
extern void RegisterTraceTests();

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    RegisterIntegrationTests();
    RegisterTraceTests();
    return testfw::RunAllTests();
}
