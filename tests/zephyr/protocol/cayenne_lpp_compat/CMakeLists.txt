cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(meshcore_cayenne_lpp_compat_tdd_test)

include(${CMAKE_CURRENT_LIST_DIR}/../../common/meshcore_module.cmake)

zephyr_include_directories(
  ${MESHCORE_INTERNAL_INCLUDE_DIRS}
)

target_sources(app PRIVATE
  src/main.c
  ${MESHCORE_SUPPORT_CAYENNE_LPP_SOURCES}
)
