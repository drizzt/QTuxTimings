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

# ISA baseline: AVX2 + CLFLUSHOPT, present on every AMD Ryzen (Zen 1+). The
# AVX-512 benchmark paths compile per-function via __attribute__((target)) and
# self-gate at runtime with __builtin_cpu_supports(), so no global -mavx512f is
# needed. Deliberately NOT -march=native: it bakes the build host's CPU into
# the binary, which breaks distro/remote builds (COPR, etc.) and is wrong for
# any redistributable package. Build natively by adding -march=native to your
# CFLAGS (e.g. in makepkg.conf) if you really want a host-pinned binary.
target_compile_options(qtuxtimings PRIVATE
    -Wall -Wextra
    -mavx2 -mclflushopt
    -fstack-protector-strong
)
target_compile_definitions(qtuxtimings PRIVATE _FORTIFY_SOURCE=2)

target_link_libraries(qtuxtimings PRIVATE
    Qt6::Widgets
    ${GMP_LIBRARY}
    Threads::Threads
)

install(TARGETS qtuxtimings RUNTIME DESTINATION bin)
