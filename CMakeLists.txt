cmake_minimum_required(VERSION 3.14)
project(state_machine C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror -pedantic")

# Core library
add_library(state_machine STATIC src/state_machine.c src/state_machine_rt.c)
target_include_directories(state_machine PUBLIC include)

# Tests
enable_testing()
add_executable(state_machine_test tests/test_state_machine.c src/state_machine.c src/state_machine_rt.c)
target_include_directories(state_machine_test PRIVATE include)
add_test(NAME state_machine_test COMMAND state_machine_test)

add_executable(rt_compliance_test tests/test_rt_compliance.c src/state_machine.c src/state_machine_rt.c)
target_include_directories(rt_compliance_test PRIVATE include)
add_test(NAME rt_compliance_test COMMAND rt_compliance_test)

add_executable(misra_verification tests/test_misra_verification.c src/state_machine.c src/state_machine_rt.c)
target_include_directories(misra_verification PRIVATE include)
add_test(NAME misra_verification COMMAND misra_verification)

# Examples
add_executable(posix_app examples/posix_app.c src/state_machine.c src/state_machine_rt.c)
target_include_directories(posix_app PRIVATE include)

add_executable(posix_async_example examples/posix_async_example.c src/state_machine.c src/state_machine_rt.c)
target_include_directories(posix_async_example PRIVATE include)

add_executable(simple_fsm examples/simple_fsm.c src/state_machine.c src/state_machine_rt.c)
target_include_directories(simple_fsm PRIVATE include)

add_executable(traffic_light examples/traffic_light.c src/state_machine.c src/state_machine_rt.c)
target_include_directories(traffic_light PRIVATE include)

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
