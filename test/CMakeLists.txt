cmake_minimum_required(VERSION 3.20)

project(KestrelTests LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
FetchContent_Declare(unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG        v2.6.0
)
FetchContent_MakeAvailable(unity)

set(KESTREL_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

enable_testing()

function(kestrel_add_test name)
    add_executable(${name} ${ARGN})
    target_include_directories(${name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/host
        ${KESTREL_ROOT}/kernel/src
        ${KESTREL_ROOT}/kernel/include
    )
    target_compile_options(${name} PRIVATE
        -Wall
        -Wextra
        -Wshadow
        -Wundef
        -O0
        -g
        -fno-omit-frame-pointer
        -fsanitize=address,undefined
    )
    target_link_options(${name} PRIVATE -fsanitize=address,undefined)
    target_link_libraries(${name} PRIVATE unity)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

kestrel_add_test(test_ready
    test_ready.c
    ${KESTREL_ROOT}/kernel/src/ready.c
)

kestrel_add_test(test_wait_list
    test_wait_list.c
    ${KESTREL_ROOT}/kernel/src/wait_list.c
)
