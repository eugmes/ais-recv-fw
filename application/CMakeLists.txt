cmake_minimum_required(VERSION 3.13.1)

set(BOARD_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(BOARD ais_recv)

find_package(Zephyr)
project(ais_recv_fw)

target_sources(app PRIVATE src/main.c src/si4362.c)
