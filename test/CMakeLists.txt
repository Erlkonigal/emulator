set(CORE_TEST_SRCS
    framework/test_framework.cc
    toy/toy_cpu_executor.cc
)

add_executable(tests
    ${CORE_TEST_SRCS}
    helpers/stdout_capture.cc
    helpers/test_helpers.cc
    unit/cli_tests.cc
    unit/debug_server_tests.cc
    unit/debugger_tests.cc
    unit/integration_tests.cc
    unit/server_tests.cc
    unit/trace_tests.cc
    unit/uart_tests.cc
    test_main.cc
)

target_link_libraries(tests PRIVATE emulator)
target_include_directories(tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_test(NAME integration_tests COMMAND tests)

add_executable(demo
    unit/demo.cc
    toy/toy_cpu_executor.cc
)
target_link_libraries(demo PRIVATE emulator)
target_include_directories(demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(asm_rom tools/asm_rom.cc)
set_target_properties(asm_rom PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

set(ASM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tools/asm")
set(ROM_DIR "${CMAKE_BINARY_DIR}/rom")

file(GLOB ASM_SOURCES "${ASM_DIR}/*.asm")

foreach(ASM_FILE ${ASM_SOURCES})
    get_filename_component(ASM_NAME ${ASM_FILE} NAME_WE)
    set(ROM_FILE "${ROM_DIR}/${ASM_NAME}.rom")
    
    add_custom_command(
        OUTPUT ${ROM_FILE}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${ROM_DIR}
        COMMAND $<TARGET_FILE:asm_rom> ${ASM_FILE} ${ROM_FILE}
        DEPENDS asm_rom ${ASM_FILE}
        COMMENT "Assembling ${ASM_NAME}.asm -> ${ASM_NAME}.rom"
    )
    list(APPEND ROM_FILES ${ROM_FILE})
endforeach()

add_custom_target(all_roms DEPENDS ${ROM_FILES})
add_dependencies(tests all_roms)