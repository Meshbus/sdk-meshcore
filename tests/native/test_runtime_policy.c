// SPDX-License-Identifier: Apache-2.0
/* Copyright (c) 2026 FoBE Studio */

#include "native_test.h"
#include "fake_platform.h"
#include "meshcore/runtime.h"
#include "meshcore_runtime_internal.h"
#include <float.h>
#include <math.h>
#include <string.h>

/* Runtime evidence: .reference/meshcore/examples/companion_radio/MyMesh.cpp */

int main(void)
{
  meshcore_common_node_runtime_policy_t policy = {0};
  struct meshcore_packet packet;
  uint32_t random_word;
  const uint8_t random_bytes[] = {0xA5U, 0xA6U, 0xA7U, 0xA8U};
  const float invalid_factors[] = {-1.0f, NAN, INFINITY, FLT_MAX, 100000000.0f};
  meshcore_common_node_role_t role;
  size_t i;

  memcpy(&random_word, random_bytes, sizeof(random_word));
  meshcore_native_platform_reset();
  meshcore_packet_init(&packet);
  packet.header = ROUTE_TYPE_DIRECT;
  packet.payload_len = 10U; /* 12 serialized bytes, fake airtime 13 ms. */
  policy.path_hash_size = 1U;
  policy.client_repeat = true;
  policy.tx_delay_factor = 0.5f;
  policy.direct_tx_delay_factor = 0.2f;
  meshcore_native_platform_policy_set(&policy);
  NATIVE_TEST_ASSERT_EQ(0, meshcore_init());
  NATIVE_TEST_ASSERT_EQ(random_word % 31U,
      meshcore_runtime_protocol_get_retransmit_delay(NULL, NULL, &packet));
  NATIVE_TEST_ASSERT_EQ(random_word % 11U,
      meshcore_runtime_protocol_get_direct_retransmit_delay(NULL, NULL, &packet));

  policy.tx_delay_factor = 1.25f;
  policy.direct_tx_delay_factor = 0.75f;
  for (role = MESHCORE_COMMON_NODE_ROLE_CHAT;
       role <= MESHCORE_COMMON_NODE_ROLE_REPEATER; role++) {
    meshcore_native_platform_role_set(role);
    meshcore_native_platform_policy_set(&policy);
    NATIVE_TEST_ASSERT_EQ(random_word % 81U,
        meshcore_runtime_protocol_get_retransmit_delay(NULL, NULL, &packet));
    NATIVE_TEST_ASSERT_EQ(random_word % 46U,
        meshcore_runtime_protocol_get_direct_retransmit_delay(NULL, NULL, &packet));
  }
  meshcore_native_platform_role_set(MESHCORE_COMMON_NODE_ROLE_CHAT);
  policy.tx_delay_factor = 0.0f;
  policy.direct_tx_delay_factor = 0.0f;
  meshcore_native_platform_policy_set(&policy);
  NATIVE_TEST_ASSERT_EQ(0U,
      meshcore_runtime_protocol_get_retransmit_delay(NULL, NULL, &packet));
  NATIVE_TEST_ASSERT_EQ(0U,
      meshcore_runtime_protocol_get_direct_retransmit_delay(NULL, NULL, &packet));

  /* Transport codes do not participate in the upstream repeat estimate. */
  packet.header = ROUTE_TYPE_TRANSPORT_FLOOD;
  packet.path_len = 0x82U; /* Six path bytes, fake airtime 19 ms. */
  policy.tx_delay_factor = 1.25f;
  policy.direct_tx_delay_factor = 0.75f;
  meshcore_native_platform_policy_set(&policy);
  for (role = MESHCORE_COMMON_NODE_ROLE_CHAT;
       role <= MESHCORE_COMMON_NODE_ROLE_REPEATER; role++) {
    meshcore_native_platform_role_set(role);
    NATIVE_TEST_ASSERT_EQ(random_word % 116U,
        meshcore_runtime_protocol_get_retransmit_delay(NULL, NULL, &packet));
    NATIVE_TEST_ASSERT_EQ(random_word % 71U,
        meshcore_runtime_protocol_get_direct_retransmit_delay(NULL, NULL, &packet));
  }

  for (role = MESHCORE_COMMON_NODE_ROLE_CHAT;
       role <= MESHCORE_COMMON_NODE_ROLE_REPEATER; role++) {
    meshcore_native_platform_role_set(role);
    for (i = 0U; i < sizeof(invalid_factors) / sizeof(invalid_factors[0]); i++) {
      policy.tx_delay_factor = invalid_factors[i];
      policy.direct_tx_delay_factor = invalid_factors[i];
      meshcore_native_platform_policy_set(&policy);
      NATIVE_TEST_ASSERT_EQ(0U,
          meshcore_runtime_protocol_get_retransmit_delay(NULL, NULL, &packet));
      NATIVE_TEST_ASSERT_EQ(0U,
          meshcore_runtime_protocol_get_direct_retransmit_delay(NULL, NULL, &packet));
    }
  }
  meshcore_native_platform_role_set(MESHCORE_COMMON_NODE_ROLE_CHAT);
  policy.client_repeat = false;
  policy.tx_delay_factor = 1.25f;
  policy.direct_tx_delay_factor = 0.75f;
  meshcore_native_platform_policy_set(&policy);
  NATIVE_TEST_ASSERT(!meshcore_runtime_protocol_allow_packet_forward(NULL, NULL, &packet));
  /* Non-repeating base Mesh still estimates the full raw frame (23 ms). */
  NATIVE_TEST_ASSERT_EQ((random_word % 5U) * 11U,
      meshcore_runtime_protocol_get_retransmit_delay(NULL, NULL, &packet));
  NATIVE_TEST_ASSERT_EQ(0U,
      meshcore_runtime_protocol_get_direct_retransmit_delay(NULL, NULL, &packet));
  meshcore_native_platform_role_set(MESHCORE_COMMON_NODE_ROLE_ROOM);
  policy.client_repeat = true;
  meshcore_native_platform_policy_set(&policy);
  NATIVE_TEST_ASSERT(!meshcore_runtime_protocol_allow_packet_forward(NULL, NULL, &packet));
  NATIVE_TEST_ASSERT_EQ(0U,
      meshcore_runtime_protocol_get_direct_retransmit_delay(NULL, NULL, &packet));
  meshcore_deinit();
  return 0;
}
