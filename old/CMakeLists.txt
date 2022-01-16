cmake_minimum_required(VERSION 3.13)
project(pisense)

add_executable(main main.c bme280.c)
target_link_libraries(main PUBLIC microhttpd)
target_include_directories(main PUBLIC .)
set_target_properties(main PROPERTIES OUTPUT_NAME "pisense")
install(TARGETS main DESTINATION bin)

