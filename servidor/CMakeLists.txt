cmake_minimum_required(VERSION 3.6)
project(servidor2016b)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES servidor.c)
add_executable(servidor2016b ${SOURCE_FILES})