cmake_minimum_required(VERSION 3.20)
project(smart_glass_firmware C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(TARGET_FLAGS "-mcpu=cortex-m7 -mthumb -mfloat-abi=hard -mfpu=fpv5-d16")
set(CMAKE_C_FLAGS "${TARGET_FLAGS} -ffunction-sections -fdata-sections -Wall -Wextra -Wno-unused-parameter" CACHE STRING "" FORCE)
set(CMAKE_ASM_FLAGS "${TARGET_FLAGS} -x assembler-with-cpp" CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS "${TARGET_FLAGS} -Wl,-Map=output.map -Wl,--gc-sections -specs=nosys.specs -specs=nano.specs -u _printf_float -lm" CACHE STRING "" FORCE)

set(FREERTOS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/freertos-kernel")
set(FREERTOS_CONFIG_FILE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src" CACHE STRING "")
set(FREERTOS_PORT "GCC_ARM_CM7" CACHE STRING "")
set(FREERTOS_HEAP "5" CACHE STRING "")

add_subdirectory(${FREERTOS_DIR} freertos_build)

file(GLOB_RECURSE FIRMWARE_SOURCES
    src/main.c
    src/freertos_hooks.c
    src/drivers/src/*.c
    src/hal/src/*.c
    src/middleware/src/*.c
    src/ai/src/*.c
    src/decision/src/*.c
    src/navigation/src/*.c
    src/voice/src/*.c
    src/application/src/*.c
    src/database/src/*.c
)

add_executable(smart_glass_firmware
    ${FIRMWARE_SOURCES}
    startup_stm32h743xx.s
)

target_include_directories(smart_glass_firmware PRIVATE
    src
    src/drivers/include
    src/hal/include
    src/middleware/include
    src/ai/include
    src/decision/include
    src/navigation/include
    src/voice/include
    src/application/include
    src/database/include
)

target_link_libraries(smart_glass_firmware PRIVATE freertos_kernel m)

set_target_properties(smart_glass_firmware PROPERTIES
    LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/STM32H743ZITx_FLASH.ld
)
target_link_options(smart_glass_firmware PRIVATE
    -T ${CMAKE_CURRENT_SOURCE_DIR}/STM32H743ZITx_FLASH.ld
)
