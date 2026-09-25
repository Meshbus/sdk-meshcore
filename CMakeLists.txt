#
# Copyright (c) 2026 FoBE Studio
# SPDX-License-Identifier: Apache-2.0
#

cmake_minimum_required(VERSION 3.20)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/meshcore_version.cmake)
project(meshcore VERSION ${MESHCORE_VERSION_NUMBER} LANGUAGES C)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  set(MESHCORE_IS_TOP_LEVEL ON)
else()
  set(MESHCORE_IS_TOP_LEVEL OFF)
endif()

option(MESHCORE_BUILD_TESTS "Build MeshCore native CTest tests" ${MESHCORE_IS_TOP_LEVEL})
option(MESHCORE_BUILD_EXAMPLES "Build MeshCore example programs" ${MESHCORE_IS_TOP_LEVEL})
option(MESHCORE_INSTALL "Install MeshCore CMake package files" ${MESHCORE_IS_TOP_LEVEL})
option(MESHCORE_BUILD_DOCS "Enable the Doxygen API documentation target" OFF)

if(MESHCORE_BUILD_DOCS)
  find_package(Doxygen 1.9 REQUIRED)
  configure_file(docs/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
  add_custom_target(meshcore_docs
    COMMAND Doxygen::doxygen ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generating MeshCore public API documentation"
    VERBATIM
  )
endif()

include(GNUInstallDirs)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/meshcore_sources.cmake)

add_library(meshcore STATIC
  ${MESHCORE_RUNTIME_LIBRARY_SOURCES}
)
add_library(meshcore::meshcore ALIAS meshcore)

target_compile_features(meshcore PUBLIC c_std_99)
target_include_directories(meshcore
  PUBLIC
    $<BUILD_INTERFACE:${MESHCORE_PUBLIC_INCLUDE_DIRS}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  PRIVATE
    ${MESHCORE_INTERNAL_INCLUDE_DIRS}
)

if(UNIX)
  target_link_libraries(meshcore PUBLIC m)
endif()

if(MESHCORE_BUILD_TESTS)
  enable_language(CXX)
  target_compile_definitions(meshcore PRIVATE MESHCORE_ENABLE_TEST_HOOKS)
  include(CTest)
  add_subdirectory(tests/native)
endif()

if(MESHCORE_BUILD_EXAMPLES)
  add_subdirectory(examples/minimal_host)
endif()

if(MESHCORE_INSTALL)
  include(CMakePackageConfigHelpers)

  install(TARGETS meshcore
    EXPORT meshcore-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )

  install(FILES
    ${CMAKE_CURRENT_LIST_DIR}/LICENSE
    ${CMAKE_CURRENT_LIST_DIR}/LICENSING.md
    ${CMAKE_CURRENT_LIST_DIR}/UPSTREAM.md
    ${CMAKE_CURRENT_LIST_DIR}/upstream.lock
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
  )

  install(DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/LICENSES
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
  )

  install(DIRECTORY ${MESHCORE_LIB_DIR}/include/meshcore
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
  )

  configure_package_config_file(
    ${CMAKE_CURRENT_LIST_DIR}/cmake/meshcore-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/meshcore-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/meshcore
  )
  write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/meshcore-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ExactVersion
  )

  install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/meshcore-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/meshcore-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/meshcore
  )
  install(EXPORT meshcore-targets
    NAMESPACE meshcore::
    FILE meshcore-targets.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/meshcore
  )
endif()
