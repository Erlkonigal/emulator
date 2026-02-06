#include "test_framework.h"

extern void RegisterIntegrationTests();

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    RegisterIntegrationTests();
    return testfw::RunAllTests();
}
