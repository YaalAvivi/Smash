cmake_minimum_required(VERSION 3.0.0)
project(Test1 VERSION 0.1.0)

include(CTest)
enable_testing()

set(OP_FLAGS "--std=c++11 -Wall")

set(CMAKE_CXX_FLAGS ${OP_FLAGS})

add_executable(Test1 smash.cpp signals.cpp Commands.cpp)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
