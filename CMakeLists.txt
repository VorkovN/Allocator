cmake_minimum_required(VERSION 3.17)
project(Allocator C)

set(CMAKE_C_STANDARD 11)
set(SRC main.c src/mem.c src/mem_debug.c src/util.c)

include_directories(${CMAKE_CURRENT_LIST_DIR}/inc/)

add_executable(${PROJECT_NAME} ${SRC})