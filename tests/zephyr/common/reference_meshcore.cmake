# SPDX-FileCopyrightText: FoBE Studio
# SPDX-License-Identifier: Apache-2.0

get_filename_component(MESHCORE_TEST_COMMON_DIR
  "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)
include("${MESHCORE_TEST_COMMON_DIR}/meshcore_module.cmake")

set(MESHCORE_REFERENCE_DIR "${MESHCORE_REPO_ROOT}/.reference/meshcore/src")
if(NOT EXISTS "${MESHCORE_REFERENCE_DIR}/Dispatcher.cpp")
  message(FATAL_ERROR
    "MeshCore upstream reference not found. Expected "
    "${MESHCORE_REFERENCE_DIR}; prepare the checkout from upstream.lock"
  )
endif()

find_package(Python3 REQUIRED COMPONENTS Interpreter)
execute_process(
  COMMAND ${Python3_EXECUTABLE} ${MESHCORE_REPO_ROOT}/tools/upstream_lock_check.py
          --repo-root ${MESHCORE_REPO_ROOT}
  RESULT_VARIABLE MESHCORE_REFERENCE_LOCK_RESULT
  OUTPUT_VARIABLE MESHCORE_REFERENCE_LOCK_OUTPUT
  ERROR_VARIABLE MESHCORE_REFERENCE_LOCK_ERROR
)
if(NOT MESHCORE_REFERENCE_LOCK_RESULT EQUAL 0)
  message(FATAL_ERROR
    "MeshCore upstream reference does not match upstream.lock:\n"
    "${MESHCORE_REFERENCE_LOCK_OUTPUT}${MESHCORE_REFERENCE_LOCK_ERROR}")
endif()
message(STATUS "MeshCore reference: ${MESHCORE_REFERENCE_DIR}")

set(MESHCORE_REFERENCE_INCLUDE_DIRS
  "${MESHCORE_REFERENCE_DIR}"
  "${MESHCORE_REFERENCE_DIR}/helpers"
  "${MESHCORE_TEST_COMMON_DIR}/include"
)

set(MESHCORE_REFERENCE_PACKET_SOURCES
  "${MESHCORE_REFERENCE_DIR}/Packet.cpp"
)
set(MESHCORE_REFERENCE_UTILS_SOURCES
  "${MESHCORE_REFERENCE_DIR}/Utils.cpp"
)
set(MESHCORE_REFERENCE_IDENTITY_SOURCES
  "${MESHCORE_REFERENCE_DIR}/Identity.cpp"
)
set(MESHCORE_REFERENCE_DISPATCHER_SOURCES
  "${MESHCORE_REFERENCE_DIR}/Dispatcher.cpp"
)
set(MESHCORE_REFERENCE_MESH_SOURCES
  "${MESHCORE_REFERENCE_DIR}/Mesh.cpp"
)
set(MESHCORE_REFERENCE_STATIC_POOL_PACKET_MANAGER_SOURCES
  "${MESHCORE_REFERENCE_DIR}/helpers/StaticPoolPacketManager.cpp"
)
set(MESHCORE_REFERENCE_ADVERT_DATA_SOURCES
  "${MESHCORE_REFERENCE_DIR}/helpers/AdvertDataHelpers.cpp"
)

set(MESHCORE_REFERENCE_CRYPTO_SOURCES
  "${MESHCORE_TEST_COMMON_DIR}/lib/Crypto/Crypto.cpp"
  "${MESHCORE_TEST_COMMON_DIR}/lib/Crypto/Hash.cpp"
  "${MESHCORE_TEST_COMMON_DIR}/lib/Crypto/SHA256.cpp"
)

set(MESHCORE_REFERENCE_CRYPTO_AES_SOURCES
  "${MESHCORE_TEST_COMMON_DIR}/lib/Crypto/BlockCipher.cpp"
  "${MESHCORE_TEST_COMMON_DIR}/lib/Crypto/AESCommon.cpp"
  "${MESHCORE_TEST_COMMON_DIR}/lib/Crypto/AES128.cpp"
)

# Reference identities receive seeds from the injected RNG; the optional
# host-OS seed generator is not part of these deterministic tests.
set(MESHCORE_REFERENCE_CRYPTO_IDENTITY_SOURCES
  "${MESHCORE_TEST_COMMON_DIR}/lib/Crypto/SHA512.cpp"
  "${MESHCORE_TEST_COMMON_DIR}/lib/Crypto/BigNumberUtil.cpp"
  "${MESHCORE_TEST_COMMON_DIR}/lib/Crypto/Curve25519.cpp"
  "${MESHCORE_TEST_COMMON_DIR}/lib/Crypto/Ed25519.cpp"
  "${MESHCORE_TEST_COMMON_DIR}/lib/ed25519/add_scalar.c"
  "${MESHCORE_TEST_COMMON_DIR}/lib/ed25519/fe.c"
  "${MESHCORE_TEST_COMMON_DIR}/lib/ed25519/ge.c"
  "${MESHCORE_TEST_COMMON_DIR}/lib/ed25519/key_exchange.c"
  "${MESHCORE_TEST_COMMON_DIR}/lib/ed25519/keypair.c"
  "${MESHCORE_TEST_COMMON_DIR}/lib/ed25519/sc.c"
  "${MESHCORE_TEST_COMMON_DIR}/lib/ed25519/sha512.c"
  "${MESHCORE_TEST_COMMON_DIR}/lib/ed25519/sign.c"
  "${MESHCORE_TEST_COMMON_DIR}/lib/ed25519/verify.c"
)

# Upstream headers assume Arduino-style transitive includes and share protocol
# macro names with the C port. Keep this scoped to C++ test translation units.
zephyr_library_compile_options(
  $<$<COMPILE_LANGUAGE:CXX>:-include>
  $<$<COMPILE_LANGUAGE:CXX>:${MESHCORE_TEST_COMMON_DIR}/include/meshcore_reference_preinclude.h>
  $<$<COMPILE_LANGUAGE:CXX>:-Wno-error>
)
