cmake_minimum_required(VERSION 3.6)
project(filtrar2016b)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror")

set(SOURCE_FILES filtrar.c)
add_executable(filtrar2016b ${SOURCE_FILES})