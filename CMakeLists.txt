cmake_minimum_required(VERSION 3.21)

project(app)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Master list of all options to pass to the compiler
set(COMPILER_OPTIONS
    -Wall
    -Wextra
    -Wpedantic
    -Werror
    -Wformat=2
    -Wformat-security
    -Wshadow
    -Wundef
    -Wmissing-prototypes
    -Wmissing-declarations
    -Wstrict-prototypes
    -Wcast-qual
    -Wcast-align
    -Wvla
)

# Pretty print all compiler options that are being set
string(REPLACE ";" "\n\t" COMPILER_OPTIONS_STR "${COMPILER_OPTIONS}")
set(COMPILER_OPTIONS_STR "\t${COMPILER_OPTIONS_STR}")
message(STATUS "Passing compile options:\n${COMPILER_OPTIONS_STR}")

# Add each option in COMPILER_OPTIONS
foreach(opt IN LISTS COMPILER_OPTIONS)
    add_compile_options(${opt})
endforeach()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Per https://sourceware.org/glibc/manual/2.43/html_mono/libc.html#index-_005fPOSIX_005fC_005fSOURCE
add_definitions(-D_POSIX_C_SOURCE=202405L)

if(USE_SYSFS)
    message(STATUS "Compiling for Sysfs-based PWM control")
    add_compile_definitions(USE_SYSFS)
elseif(USE_MMAP)
    message(STATUS "Compiling for MMAP-based PWM control")
    add_compile_definitions(USE_MMAP)
endif()

link_libraries("-static")

add_executable(app
    src/main.c
    src/servo/servo_controller.c
    src/utils/logger.c
    src/utils/mmap_control.c
    src/utils/pwm_io_logic.c
)

target_include_directories(app PRIVATE
    src/servo
    src/utils
)
