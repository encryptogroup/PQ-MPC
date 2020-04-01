cmake_minimum_required(VERSION 3.10)

project(pq-yao LANGUAGES C CXX)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

option(BUILD_TESTS "Build tests" OFF)
message(STATUS "Option: BUILD_TESTS = ${BUILD_TESTS}")

add_compile_options("-pthread;-Wall;-march=native;-maes;-mrdseed;-std=c++14;-O3")

find_package(GMP REQUIRED)

find_package(SEAL 3.1.0 EXACT QUIET)
if (NOT SEAL_FOUND)
    message(STATUS "SEAL 3.1.0 was not found: clone and install SEAL locally")
    if (NOT EXISTS "${PROJECT_SOURCE_DIR}/extern/SEAL/src/CMakeLists.txt")
        find_package(Git REQUIRED)
        message(STATUS "initialize Git submodule: extern/SEAL")
        execute_process(COMMAND git submodule update --init --recursive extern/SEAL
                WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
    endif ()
    execute_process(COMMAND ${CMAKE_COMMAND} -DCMAKE_INSTALL_PREFIX=${PROJECT_SOURCE_DIR}/build .
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/extern/SEAL/src")
    execute_process(COMMAND ${CMAKE_COMMAND} --build . --target install
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/extern/SEAL/src")
    find_package(SEAL 3.1.0 EXACT REQUIRED PATHS "${PROJECT_SOURCE_DIR}/build/")
endif ()

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(emp-tool)
add_subdirectory(pq-ot)
add_subdirectory(pq-yao)

if (BUILD_TESTS)
    add_subdirectory(test)
endif (BUILD_TESTS)
