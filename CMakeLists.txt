cmake_minimum_required(VERSION 3.17)
project(quitvim)

set(CMAKE_C_STANDARD 23)

add_compile_options(-Wall -Wextra -Werror)
add_compile_options(-Wno-unused-parameter)

add_executable(quitvim quitvim.c)
