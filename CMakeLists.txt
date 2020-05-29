cmake_minimum_required(VERSION 3.13)
project(pisense)

add_executable(main main.c)
target_link_libraries(main PUBLIC microhttpd)

