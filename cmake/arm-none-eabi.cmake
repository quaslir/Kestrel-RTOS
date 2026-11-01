# Toolchain file: GCC for bare-metal ARM, target STM32F303 (Cortex-M4F)

set(CMAKE_SYSTEM_NAME      Generic)     # no OS
set(CMAKE_SYSTEM_PROCESSOR arm)

set(TOOLCHAIN_PREFIX arm-none-eabi-)

set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}gcc)

find_program(CMAKE_OBJCOPY ${TOOLCHAIN_PREFIX}objcopy REQUIRED)
find_program(CMAKE_SIZE    ${TOOLCHAIN_PREFIX}size    REQUIRED)

# CMake test-compiles a program to check the compiler; without a linker
# script that would fail, so test with a static library instead.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Cortex-M4 with single-precision FPU, hardware float ABI
set(KESTREL_MCU_FLAGS "-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard")

set(CMAKE_C_FLAGS_INIT   "${KESTREL_MCU_FLAGS} -ffunction-sections -fdata-sections -fno-common")
set(CMAKE_ASM_FLAGS_INIT "${KESTREL_MCU_FLAGS} -x assembler-with-cpp")

# newlib-nano (small libc), stub syscalls, drop unused sections
set(CMAKE_EXE_LINKER_FLAGS_INIT
    "${KESTREL_MCU_FLAGS} -specs=nano.specs -specs=nosys.specs -Wl,--gc-sections")

# Search headers/libraries only in the target environment, programs on the host
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
