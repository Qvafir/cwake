cmake_minimum_required(VERSION 3.5)

project(cwake LANGUAGES C)

set(CMAKE_C_STANDARD 99)           # Request C11 standard
set(CMAKE_C_STANDARD_REQUIRED ON)  # Enforce the specific version
set(CMAKE_C_EXTENSIONS OFF)        # Disable compiler extensions (optional, for strict adherence)

add_executable(cwake main.c
    cwake.h
    cwake.c
    mock.c mock.h tests.c tests.h
    common.c common.h)

target_compile_definitions(cwake PRIVATE CWAKE_TEST)
target_compile_definitions(cwake PRIVATE CWAKE_DEBUG_OUTPUT)
include(GNUInstallDirs)

install(TARGETS cwake
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
