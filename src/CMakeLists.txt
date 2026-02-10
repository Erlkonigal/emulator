file(GLOB_RECURSE SOURCES "*.cpp")

add_library(emulator STATIC ${SOURCES})

target_include_directories(emulator PUBLIC 
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}
)

target_include_directories(emulator PUBLIC 
    ${SDL2_INCLUDE_DIRS}
    ${NCURSES_INCLUDE_DIRS}
    ${VTERM_INCLUDE_DIRS}
)
target_link_libraries(emulator PUBLIC 
    ${SDL2_LIBRARIES}
    ${NCURSES_LIBRARIES}
    ${VTERM_LIBRARIES}
)
