cmake_minimum_required(VERSION 3.5)

project(fibers)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(tests async.cpp epoll.cpp fibers.cpp tests.cpp)
