# SPDX-FileCopyrightText: FoBE Studio
# SPDX-License-Identifier: Apache-2.0

# The runtime uses POSIX string helpers such as strnlen. Host libc hides
# those declarations in strict C17 unless the consuming test requests them.
zephyr_compile_definitions_ifdef(CONFIG_NATIVE_LIBC _POSIX_C_SOURCE=200809L)

get_filename_component(MESHCORE_REPO_ROOT
  "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)

# These tests compile the source checkout containing them. In a consumer west
# workspace, reject an accidentally discovered second MeshCore checkout.
if(DEFINED ZEPHYR_MESHCORE_MODULE_DIR AND
   NOT ZEPHYR_MESHCORE_MODULE_DIR STREQUAL "")
  get_filename_component(MESHCORE_DISCOVERED_ROOT
    "${ZEPHYR_MESHCORE_MODULE_DIR}" ABSOLUTE)
  if(NOT MESHCORE_DISCOVERED_ROOT STREQUAL MESHCORE_REPO_ROOT)
    message(FATAL_ERROR
      "Discovered MeshCore module differs from the test checkout: "
      "${MESHCORE_DISCOVERED_ROOT} != ${MESHCORE_REPO_ROOT}")
  endif()
endif()

set(MESHCORE_MODULE_SOURCE_MANIFEST
  "${MESHCORE_REPO_ROOT}/cmake/meshcore_sources.cmake"
)

if(NOT EXISTS "${MESHCORE_MODULE_SOURCE_MANIFEST}")
  message(FATAL_ERROR
    "MeshCore source manifest not found: "
    "${MESHCORE_MODULE_SOURCE_MANIFEST}"
  )
endif()

include("${MESHCORE_REPO_ROOT}/cmake/meshcore_version.cmake")
message(STATUS "MeshCore source: ${MESHCORE_REPO_ROOT}")
message(STATUS "MeshCore ${MESHCORE_VERSION_STRING} (upstream ${MESHCORE_UPSTREAM_COMMIT})")
include("${MESHCORE_MODULE_SOURCE_MANIFEST}")
