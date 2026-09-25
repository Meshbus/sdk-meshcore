cmake_minimum_required(VERSION 3.20)

project(meshcore_minimal_host LANGUAGES C)

set(MESHCORE_FIND_VERSION "" CACHE STRING
  "Optional exact upstream numeric baseline for standalone builds"
)

if(NOT TARGET meshcore::meshcore)
  if(MESHCORE_FIND_VERSION)
    find_package(meshcore ${MESHCORE_FIND_VERSION} CONFIG REQUIRED)
  else()
    find_package(meshcore CONFIG REQUIRED)
  endif()
endif()

#
# Copyright (c) 2026 FoBE Studio
# SPDX-License-Identifier: Apache-2.0
#

add_executable(meshcore_minimal_host
  main.c
  example_platform.c
)

target_link_libraries(meshcore_minimal_host PRIVATE meshcore::meshcore)
