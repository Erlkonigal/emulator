file(GLOB_RECURSE SOURCES "*.cpp")

add_library(emulator STATIC ${SOURCES})

target_include_directories(emulator PUBLIC 
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}
)

target_include_directories(emulator PUBLIC ${SDL2_INCLUDE_DIRS} ${CURSES_INCLUDE_DIRS})
target_link_libraries(emulator PUBLIC ${SDL2_LIBRARIES} ${CURSES_LIBRARIES})
