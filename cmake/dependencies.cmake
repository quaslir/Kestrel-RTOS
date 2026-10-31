include(FetchContent)

set(KESTREL_CMSIS_CORE_TAG   "v5.4.0_cm4" CACHE STRING "STMicroelectronics/cmsis_core tag")
set(KESTREL_CMSIS_DEVICE_TAG "v2.3.8"     CACHE STRING "STMicroelectronics/cmsis_device_f3 tag")

FetchContent_Declare(cmsis_core
    GIT_REPOSITORY https://github.com/STMicroelectronics/cmsis_core.git
    GIT_TAG        ${KESTREL_CMSIS_CORE_TAG}
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
    SOURCE_SUBDIR  do-not-build
)

FetchContent_Declare(cmsis_device_f3
    GIT_REPOSITORY https://github.com/STMicroelectronics/cmsis_device_f3.git
    GIT_TAG        ${KESTREL_CMSIS_DEVICE_TAG}
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
    SOURCE_SUBDIR  do-not-build
)

FetchContent_MakeAvailable(cmsis_core cmsis_device_f3)

set(KESTREL_CMSIS_CORE_INCLUDE   "${cmsis_core_SOURCE_DIR}/Include")
set(KESTREL_CMSIS_DEVICE_INCLUDE "${cmsis_device_f3_SOURCE_DIR}/Include")

foreach(header IN ITEMS
        "${KESTREL_CMSIS_CORE_INCLUDE}/core_cm4.h"
        "${KESTREL_CMSIS_DEVICE_INCLUDE}/stm32f3xx.h"
        "${KESTREL_CMSIS_DEVICE_INCLUDE}/stm32f303xe.h")
    if(NOT EXISTS "${header}")
        message(FATAL_ERROR
            "CMSIS header not found: ${header}\n"
            "Check KESTREL_CMSIS_CORE_TAG / KESTREL_CMSIS_DEVICE_TAG "
            "and the include layout of the fetched repositories.")
    endif()
endforeach()

add_library(cmsis INTERFACE)

target_include_directories(cmsis SYSTEM INTERFACE
    ${KESTREL_CMSIS_CORE_INCLUDE}
    ${KESTREL_CMSIS_DEVICE_INCLUDE}
)

target_compile_definitions(cmsis INTERFACE STM32F303xE)
