cmake_minimum_required(VERSION 3.1)
project(MIMUW-EXECUTOR C)

set(CMAKE_C_STANDARD "11")
set(CMAKE_C_FLAGS "-g -Wall -Wextra -Wno-sign-compare")

function(add_example_program target_name)
  add_executable(${target_name} "${target_name}.c")
  target_link_libraries(${target_name} err)
  target_link_libraries(${target_name} utils)
  target_link_libraries(${target_name} pthread)
endfunction()

add_library(err err.c)
add_library(utils utils.c)
add_example_program(executor)
