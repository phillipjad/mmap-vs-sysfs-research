cmake_minimum_required(VERSION 3.21)

project(app LANGUAGES C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Wpedantic -Werror)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Per https://sourceware.org/glibc/manual/2.43/html_mono/libc.html#index-_005fPOSIX_005fC_005fSOURCE
add_definitions(-D_POSIX_C_SOURCE=202405L)

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
