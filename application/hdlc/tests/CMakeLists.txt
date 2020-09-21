# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)

set(ZEPHYR_EXTRA_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/../../hdlc")

find_package(Zephyr REQUIRED HINTS
  "${CMAKE_CURRENT_SOURCE_DIR}/../../../zephyr")

project(NONE)

target_sources(app PRIVATE src/main.c)

set(gen_dir ${ZEPHYR_BINARY_DIR}/include/generated/)
set(source_file src/gnuais-stereo-2rx-ch1-bitstream.bin)

generate_inc_file_for_target(app ${source_file} ${gen_dir}/bitstream.inc)
