cmake_minimum_required(VERSION 3.20)

# ---------------------------------------------------------------------------
# Toolchain: default to arm-none-eabi when Kestrel is the top-level project.
# A parent project that includes Kestrel via add_subdirectory() sets its own.
# ---------------------------------------------------------------------------
if(NOT CMAKE_TOOLCHAIN_FILE AND CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/arm-none-eabi.cmake)
endif()

project(KestrelRTOS
    VERSION 1.0.0
    DESCRIPTION "Preemptive real-time kernel for ARM Cortex-M4F"
    LANGUAGES C ASM)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(KESTREL_IS_TOP_LEVEL ON)
else()
    set(KESTREL_IS_TOP_LEVEL OFF)
endif()

# ---------------------------------------------------------------------------
# Global settings (only when built standalone)
# ---------------------------------------------------------------------------
if(KESTREL_IS_TOP_LEVEL)
    set(CMAKE_C_STANDARD 11)
    set(CMAKE_C_STANDARD_REQUIRED ON)
    set(CMAKE_C_EXTENSIONS ON)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
    endif()
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release MinSizeRel)

    set(CMAKE_C_FLAGS_DEBUG      "-Og -g3")
    set(CMAKE_C_FLAGS_RELEASE    "-O2 -g")
    set(CMAKE_C_FLAGS_MINSIZEREL "-Os -g")
endif()

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
set(KESTREL_MAX_TASKS        8   CACHE STRING "Maximum number of application tasks")
set(KESTREL_STACK_WORDS      256 CACHE STRING "Stack size of each task, in 32-bit words")
set(KESTREL_IDLE_STACK_WORDS 64  CACHE STRING "Stack size of the idle task, in 32-bit words")
option(KESTREL_PRIORITY_INHERITANCE "Enable mutex priority inheritance"  ON)
option(KESTREL_WARNINGS_AS_ERRORS   "Treat compiler warnings as errors"  OFF)
option(KESTREL_BUILD_EXAMPLES       "Build the example firmware"          ${KESTREL_IS_TOP_LEVEL})
option(KESTREL_BUILD_BENCHMARKS     "Build the benchmark firmware"        ${KESTREL_IS_TOP_LEVEL})

set(KESTREL_OPENOCD_ARGS "-f;board/st_nucleo_f3.cfg"
    CACHE STRING "OpenOCD arguments used by the flash_<firmware> targets")

include(cmake/dependencies.cmake)

# ---------------------------------------------------------------------------
# Warnings: applied to Kestrel's own code and firmware, never forced on users
# ---------------------------------------------------------------------------
add_library(kestrel_warnings INTERFACE)
target_compile_options(kestrel_warnings INTERFACE
    -Wall
    -Wextra
    -Wshadow
    -Wundef
    -Wdouble-promotion
    -Wstrict-prototypes
    $<$<BOOL:${KESTREL_WARNINGS_AS_ERRORS}>:-Werror>)

# ---------------------------------------------------------------------------
# Kernel + Cortex-M4F port
# ---------------------------------------------------------------------------
add_library(kestrel OBJECT
    kernel/src/task.c
    kernel/src/time.c
    kernel/src/ready.c
    kernel/src/wait_list.c
    kernel/src/semaphore.c
    kernel/src/mutex.c
    kernel/src/queue.c
    port/cm4f/port.c)
add_library(Kestrel::kernel ALIAS kestrel)

target_include_directories(kestrel
    PUBLIC
        ${PROJECT_SOURCE_DIR}/kernel/include
    PRIVATE
        ${PROJECT_SOURCE_DIR}/kernel/src
        ${PROJECT_SOURCE_DIR}/port/cm4f)

target_compile_definitions(kestrel PUBLIC
    KESTREL_MAX_TASKS=${KESTREL_MAX_TASKS}
    KESTREL_STACK_WORDS=${KESTREL_STACK_WORDS}
    KESTREL_IDLE_STACK_WORDS=${KESTREL_IDLE_STACK_WORDS}
    KESTREL_CFG_PRIORITY_INHERITANCE=$<BOOL:${KESTREL_PRIORITY_INHERITANCE}>)

target_compile_options(kestrel PRIVATE -ffunction-sections -fdata-sections)
target_link_libraries(kestrel
    PUBLIC  cmsis
    PRIVATE kestrel_warnings)

# ---------------------------------------------------------------------------
# Board drivers (NUCLEO-F303RE): clock, GPIO, SysTick, UART
# ---------------------------------------------------------------------------
add_library(kestrel_drivers OBJECT
    drivers/src/clock.c
    drivers/src/gpio.c
    drivers/src/systick.c
    drivers/src/uart.c)
add_library(Kestrel::drivers ALIAS kestrel_drivers)

target_include_directories(kestrel_drivers PUBLIC ${PROJECT_SOURCE_DIR}/drivers/include)
target_compile_options(kestrel_drivers PRIVATE -ffunction-sections -fdata-sections)
target_link_libraries(kestrel_drivers
    PUBLIC  cmsis
    PRIVATE kestrel_warnings)

# ---------------------------------------------------------------------------
# kestrel_add_firmware(<name> <sources>...)
#   Builds <name>.elf/.bin/.hex with the startup file and linker script,
#   links the kernel and the board drivers, and adds a flash_<name> target.
# ---------------------------------------------------------------------------
set(KESTREL_LINKER_SCRIPT  ${PROJECT_SOURCE_DIR}/startup/stm32f303re.ld)
set(KESTREL_STARTUP_SOURCE ${PROJECT_SOURCE_DIR}/startup/startup_stm32f303.c)

function(kestrel_add_firmware name)
    if(NOT ARGN)
        message(FATAL_ERROR "kestrel_add_firmware(${name}): no sources given")
    endif()

    add_executable(${name} ${ARGN} ${KESTREL_STARTUP_SOURCE})
    set_target_properties(${name} PROPERTIES
        SUFFIX       ".elf"
        LINK_DEPENDS ${KESTREL_LINKER_SCRIPT})

    target_link_libraries(${name} PRIVATE Kestrel::kernel Kestrel::drivers kestrel_warnings)
    target_link_options(${name} PRIVATE
        -T${KESTREL_LINKER_SCRIPT}
        -nostartfiles
        -Wl,--gc-sections
        -Wl,-Map=$<TARGET_FILE_DIR:${name}>/${name}.map
        -Wl,--print-memory-usage)

    add_custom_command(TARGET ${name} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${name}> $<TARGET_FILE_DIR:${name}>/${name}.bin
        COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${name}> $<TARGET_FILE_DIR:${name}>/${name}.hex
        COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${name}>
        VERBATIM)

    add_custom_target(flash_${name}
        COMMAND openocd ${KESTREL_OPENOCD_ARGS}
                -c "program $<TARGET_FILE:${name}> verify reset exit"
        DEPENDS ${name}
        USES_TERMINAL
        VERBATIM)
endfunction()

# ---------------------------------------------------------------------------
# Firmware in this repository
# ---------------------------------------------------------------------------
if(KESTREL_BUILD_EXAMPLES AND EXISTS ${PROJECT_SOURCE_DIR}/examples/CMakeLists.txt)
    add_subdirectory(examples)
endif()

if(KESTREL_BUILD_BENCHMARKS AND EXISTS ${PROJECT_SOURCE_DIR}/bench/CMakeLists.txt)
    add_subdirectory(bench)
endif()

# ---------------------------------------------------------------------------
# Configuration summary
# ---------------------------------------------------------------------------
if(KESTREL_IS_TOP_LEVEL)
    message(STATUS "Kestrel ${PROJECT_VERSION} (${CMAKE_BUILD_TYPE})")
    message(STATUS "  tasks: ${KESTREL_MAX_TASKS}, stack: ${KESTREL_STACK_WORDS} words, "
                   "idle stack: ${KESTREL_IDLE_STACK_WORDS} words")
    message(STATUS "  priority inheritance: ${KESTREL_PRIORITY_INHERITANCE}, "
                   "warnings as errors: ${KESTREL_WARNINGS_AS_ERRORS}")
    message(STATUS "  examples: ${KESTREL_BUILD_EXAMPLES}, benchmarks: ${KESTREL_BUILD_BENCHMARKS}")
endif()
