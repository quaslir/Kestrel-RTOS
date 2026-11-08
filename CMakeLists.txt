cmake_minimum_required(VERSION 3.20)

if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/arm-none-eabi.cmake)
endif()

project(KestrelRTOS VERSION 0.1.0 LANGUAGES C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()
set(CMAKE_C_FLAGS_DEBUG   "-Og -g3")
set(CMAKE_C_FLAGS_RELEASE "-O2 -g")

include(cmake/dependencies.cmake)

add_library(kestrel_options INTERFACE)
target_compile_options(kestrel_options INTERFACE
    -Wall
    -Wextra
    -Wshadow
    -Wundef
    -Wdouble-promotion
    -Wstrict-prototypes
)
target_link_libraries(kestrel_options INTERFACE cmsis)

file(GLOB KESTREL_SOURCES CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/drivers/src/*.c
    ${PROJECT_SOURCE_DIR}/kernel/src/*.c
    ${PROJECT_SOURCE_DIR}/port/cm4f/*.c
)

add_library(kestrel OBJECT ${KESTREL_SOURCES})

option(KESTREL_PRIORITY_INHERITANCE "Enable mutex priority inheritance" ON)
target_compile_definitions(kestrel PRIVATE
    KESTREL_CFG_PRIORITY_INHERITANCE=$<BOOL:${KESTREL_PRIORITY_INHERITANCE}>)

target_include_directories(kestrel
    PUBLIC
        ${PROJECT_SOURCE_DIR}/drivers/include
        ${PROJECT_SOURCE_DIR}/kernel/include
    PRIVATE
        ${PROJECT_SOURCE_DIR}/kernel/src
        ${PROJECT_SOURCE_DIR}/port/cm4f
)

target_link_libraries(kestrel PUBLIC kestrel_options)

set(KESTREL_LINKER_SCRIPT  ${PROJECT_SOURCE_DIR}/startup/stm32f303re.ld)
set(KESTREL_STARTUP_SOURCE ${PROJECT_SOURCE_DIR}/startup/startup_stm32f303.c)

function(kestrel_add_firmware name)
    foreach(required IN ITEMS ${KESTREL_LINKER_SCRIPT} ${KESTREL_STARTUP_SOURCE})
        if(NOT EXISTS ${required})
            message(FATAL_ERROR "Missing ${required} — write the startup files first (milestone M0).")
        endif()
    endforeach()

    add_executable(${name} ${ARGN} ${KESTREL_STARTUP_SOURCE})
    set_target_properties(${name} PROPERTIES
        SUFFIX       ".elf"
        LINK_DEPENDS ${KESTREL_LINKER_SCRIPT})

    target_link_libraries(${name} PRIVATE kestrel)
    target_link_options(${name} PRIVATE
        -T${KESTREL_LINKER_SCRIPT}
        -nostartfiles
        -Wl,-Map=$<TARGET_FILE_DIR:${name}>/${name}.map
        -Wl,--print-memory-usage)

    add_custom_command(TARGET ${name} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${name}> $<TARGET_FILE_DIR:${name}>/${name}.bin
        COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${name}> $<TARGET_FILE_DIR:${name}>/${name}.hex
        COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${name}>
        VERBATIM)

    add_custom_target(flash_${name}
        COMMAND openocd -f interface/stlink.cfg -f target/stm32f3x.cfg
                -c "program $<TARGET_FILE:${name}> verify reset exit"
        DEPENDS ${name}
        USES_TERMINAL
        VERBATIM)
endfunction()

if(EXISTS ${PROJECT_SOURCE_DIR}/app/demo/CMakeLists.txt)
    add_subdirectory(app/demo)
endif()
