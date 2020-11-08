cmake_minimum_required(VERSION 3.13.1)

set(BOARD_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(BOARD ais_recv)

set(ZEPHYR_EXTRA_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/hdlc")

find_package(Zephyr REQUIRED HINTS "${CMAKE_CURRENT_SOURCE_DIR}/../zephyr")
project(ais_recv_fw)

target_sources(app PRIVATE
  src/main.c
  src/si4362.c
  src/radio_config_ch1.c
  src/radio_config_ch2.c
  src/radio_patch.c)

set(gen_dir ${ZEPHYR_BINARY_DIR}/include/generated/)
set(source_file hdlc/tests/src/gnuais-stereo-2rx-ch1-bitstream.bin)

generate_inc_file_for_target(app ${source_file} ${gen_dir}/bitstream.inc)
