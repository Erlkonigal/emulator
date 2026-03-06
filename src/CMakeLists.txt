file(GLOB_RECURSE SOURCES "*.cpp")

add_library(emulator STATIC ${SOURCES})

target_include_directories(emulator PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}
)

target_link_libraries(emulator PUBLIC Threads::Threads)