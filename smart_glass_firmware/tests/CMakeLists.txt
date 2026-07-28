cmake_minimum_required(VERSION 3.10)
project(smart_glass_tests C)

set(MOCK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/mocks)
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../src)

include_directories(
    ${MOCK_DIR}
    ${SRC_DIR}/middleware/include
    ${SRC_DIR}/ai/include
    ${SRC_DIR}/decision/include
    ${SRC_DIR}/navigation/include
    ${SRC_DIR}/drivers/include
    ${SRC_DIR}/hal/include
    ${SRC_DIR}/application/include
    ${SRC_DIR}/voice/include
    ${SRC_DIR}/database/include
    ${SRC_DIR}
)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11 -DHOST_BUILD -Wall -Wextra -Wno-unused-parameter -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable -Wno-missing-field-initializers -g -O0")

function(add_module_test name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} m)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_module_test(test_config_manager     test_config_manager.c)
add_module_test(test_object_tracking    test_object_tracking.c)
add_module_test(test_depth_processing   test_depth_processing.c)
add_module_test(test_context_manager    test_context_manager.c)
add_module_test(test_system_manager     test_system_manager.c)
add_module_test(test_logging_manager    test_logging_manager.c)

enable_testing()
