cmake_minimum_required(VERSION 3.16)
project(qtuxtimings LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Build a position-independent executable (was -fPIE -pie in the Makefile)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(Threads REQUIRED)

find_library(GMP_LIBRARY NAMES gmp REQUIRED)

# C backend (pure C, no Qt). Listed explicitly rather than globbed so a stray
# .c file under src/ can't be silently pulled into the build.
set(BACKEND_SOURCES
    src/backend.c
    src/bench.c
    src/dram.c
    src/pi_bench.c
    src/pm_table.c
    src/util.c
)

add_executable(qtuxtimings
    ${BACKEND_SOURCES}
    src/ui.cpp
    src/main.cpp
)

# Hardening / tuning flags carried over from the original Makefile.
target_compile_options(qtuxtimings PRIVATE
    -Wall -Wextra
    -march=native -mclflushopt
    -fstack-protector-strong
)
target_compile_definitions(qtuxtimings PRIVATE _FORTIFY_SOURCE=2)

target_link_libraries(qtuxtimings PRIVATE
    Qt6::Widgets
    ${GMP_LIBRARY}
    Threads::Threads
)

install(TARGETS qtuxtimings RUNTIME DESTINATION bin)
