set(CORE_TEST_SRCS
    test_framework.cc
    toy_cpu_executor.cc
)

add_executable(tests 
    ${CORE_TEST_SRCS}
    stdout_capture.cc
    test_helpers.cc
    integration_tests.cc
    trace_tests.cc
    device_tests.cc
    test_main.cc
)

target_link_libraries(tests PRIVATE emulator)
target_include_directories(tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_test(NAME integration_tests COMMAND tests)

add_executable(display_demo 
    ${CORE_TEST_SRCS}
    display_demo.cc
)

target_link_libraries(display_demo PRIVATE emulator)
target_include_directories(display_demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
