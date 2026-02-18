cmake_minimum_required(VERSION 3.14)
project(state_machine C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror -pedantic")

# Core library
add_library(state_machine STATIC src/state_machine.c src/state_machine_rt.c)
target_include_directories(state_machine PUBLIC include)
target_include_directories(state_machine PRIVATE include/state_machine)

# Tests
enable_testing()
add_executable(state_machine_test tests/test_state_machine.c src/state_machine.c src/state_machine_rt.c)
target_include_directories(state_machine_test PRIVATE include include/state_machine)
add_test(NAME state_machine_test COMMAND state_machine_test)

add_executable(rt_compliance_test tests/test_rt_compliance.c src/state_machine.c src/state_machine_rt.c)
target_include_directories(rt_compliance_test PRIVATE include include/state_machine)
add_test(NAME rt_compliance_test COMMAND rt_compliance_test)

add_executable(misra_verification tests/test_misra_verification.c src/state_machine.c src/state_machine_rt.c)
target_include_directories(misra_verification PRIVATE include include/state_machine)
add_test(NAME misra_verification COMMAND misra_verification)

# Examples
add_executable(simple_hsm examples/simple_hsm.c src/state_machine.c src/state_machine_rt.c)
target_include_directories(simple_hsm PRIVATE include include/state_machine)

add_executable(traffic_light examples/traffic_light.c src/state_machine.c src/state_machine_rt.c)
target_include_directories(traffic_light PRIVATE include include/state_machine)

# Code quality
find_program(CLANG_FORMAT clang-format)
if(CLANG_FORMAT)
    add_custom_target(format
        COMMAND ${CLANG_FORMAT} -i include/*.h src/*.c tests/*.c examples/*.c
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif()

find_program(CPPCHECK cppcheck)
if(CPPCHECK)
    add_custom_target(cppcheck
        COMMAND ${CPPCHECK} --enable=all --suppress=missingIncludeSystem
                include/ src/ tests/ examples/
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif()
