set(CORE_TEST_SRCS
    framework/test_framework.cc
    toy/toy_cpu_executor.cc
)

add_executable(tests
    ${CORE_TEST_SRCS}
    helpers/stdout_capture.cc
    helpers/test_helpers.cc
    integration/integration_tests.cc
    unit/trace_tests.cc
    test_main.cc
)

target_link_libraries(tests PRIVATE emulator)
target_include_directories(tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_test(NAME integration_tests COMMAND tests)

add_executable(demo
    tools/demo.cc
    toy/toy_cpu_executor.cc
)
target_link_libraries(demo PRIVATE emulator)
target_include_directories(demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(asm_rom tools/asm_rom.cc)
set_target_properties(asm_rom PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)