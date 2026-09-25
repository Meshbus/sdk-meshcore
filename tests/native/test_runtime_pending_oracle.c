// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2026 FoBE Studio
 */

#include "native_test.h"
#include "fake_platform.h"

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "meshcore/platform.h"
#include "meshcore/runtime.h"
#include "meshcore/types.h"
#include "meshcore_packet.h"
#include "meshcore_runtime_bridge.h"

/*
 * Layer 2 pending/runtime oracle evidence:
 * - .reference/meshcore/src/helpers/BaseChatMesh.h
 * - .reference/meshcore/src/helpers/BaseChatMesh.cpp
 * - .reference/meshcore/examples/companion_radio/main.cpp
 * - .reference/meshcore/examples/companion_radio/MyMesh.h
 * - .reference/meshcore/examples/companion_radio/MyMesh.cpp
 */

bool meshcore_test_runtime_pending_discovery_is_valid(void);
bool meshcore_test_runtime_pending_discovery_get(uint32_t *tag,
                                                 uint8_t *key_prefix,
                                                 unsigned long *expires_at_ms);
bool meshcore_test_runtime_pending_trace_is_valid(void);
bool meshcore_test_runtime_pending_trace_get(uint32_t *tag,
                                             unsigned long *expires_at_ms);
bool meshcore_test_runtime_pending_telemetry_is_valid(void);
bool meshcore_test_runtime_pending_telemetry_get(uint32_t *tag,
                                                 uint8_t *key_prefix,
                                                 uint8_t *permission_mask,
                                                 unsigned long *expires_at_ms);
bool meshcore_test_runtime_pending_binary_is_valid(void);
bool meshcore_test_runtime_pending_binary_get(uint32_t *tag,
                                              uint8_t *key_prefix,
                                              unsigned long *expires_at_ms);
int meshcore_test_runtime_expected_ack_used_count_get(void);
bool meshcore_test_runtime_expected_ack_peek(uint32_t *ack_crc,
                                             uint8_t *target,
                                             uint8_t *attempt,
                                             unsigned long *expires_at_ms);
int meshcore_test_runtime_dispatcher_outbound_total_get(void);
bool meshcore_test_runtime_simulate_ack_recv(uint32_t ack_crc);
bool meshcore_test_runtime_simulate_peer_path_recv(
    const uint8_t *key_prefix, const uint8_t *out_path, uint8_t out_path_len,
    uint8_t path_hash_size, uint8_t extra_type, const int8_t *out_path_snr,
    uint8_t out_path_snr_count, const int8_t *return_path_snr,
    uint8_t return_path_snr_count, int8_t response_snr);
bool meshcore_test_runtime_simulate_trace_recv(uint32_t tag, uint8_t flags,
                                               const int8_t *path_snrs,
                                               uint8_t path_snr_count,
                                               int8_t response_snr);
bool meshcore_test_runtime_simulate_telemetry_response_recv(
    const uint8_t *key_prefix, uint32_t tag, const uint8_t *payload,
    size_t payload_len);
bool meshcore_test_runtime_simulate_binary_response_recv(
    const uint8_t *key_prefix, uint32_t tag, const uint8_t *payload,
    size_t payload_len);

static uint8_t s_public_key[MESHCORE_PUBLIC_KEY_SIZE];
static uint8_t s_payload[MESHCORE_MAX_SERVICE_REQUEST_PAYLOAD_LEN];

static void fill_buffers(void)
{
  size_t i;

  for (i = 0U; i < sizeof(s_public_key); i++) {
    s_public_key[i] = (uint8_t)(0x20U + i);
  }
  for (i = 0U; i < sizeof(s_payload); i++) {
    s_payload[i] = (uint8_t)(0x60U + i);
  }
}

static int init_runtime(void)
{
  meshcore_native_platform_reset();
  meshcore_deinit();
  return meshcore_init();
}

static int test_expected_ack_registers_publishes_and_clears(void)
{
  uint32_t ack_crc = 0U;
  uint8_t target[MESHCORE_NODE_KEY_PREFIX_BYTES] = {0};
  uint8_t attempt = 0U;

  NATIVE_TEST_ASSERT_EQ(0, init_runtime());
  NATIVE_TEST_ASSERT_EQ(0, meshcore_test_runtime_expected_ack_used_count_get());

  NATIVE_TEST_ASSERT_EQ(0, meshcore_message_send_to_node(s_public_key, true,
                                                         2U, s_payload, 1U));
  NATIVE_TEST_ASSERT_EQ(1, meshcore_test_runtime_expected_ack_used_count_get());
  NATIVE_TEST_ASSERT(meshcore_test_runtime_expected_ack_peek(&ack_crc, target,
                                                             &attempt, NULL));
  NATIVE_TEST_ASSERT(ack_crc != 0U);
  NATIVE_TEST_ASSERT_EQ(2U, attempt);
  NATIVE_TEST_ASSERT(memcmp(target, s_public_key, sizeof(target)) == 0);
  NATIVE_TEST_ASSERT(!meshcore_test_runtime_simulate_ack_recv(ack_crc + 1U));
  NATIVE_TEST_ASSERT_EQ(1, meshcore_test_runtime_expected_ack_used_count_get());
  NATIVE_TEST_ASSERT_EQ(0U, meshcore_native_platform_message_ack_count_get());

  NATIVE_TEST_ASSERT(meshcore_test_runtime_simulate_ack_recv(ack_crc));
  NATIVE_TEST_ASSERT_EQ(0, meshcore_test_runtime_expected_ack_used_count_get());
  NATIVE_TEST_ASSERT_EQ(1U, meshcore_native_platform_message_ack_count_get());

  meshcore_deinit();
  return 0;
}

static int test_zero_ack_hash_sends_without_registration(void)
{
  NATIVE_TEST_ASSERT_EQ(0, init_runtime());
  meshcore_native_platform_sha256_two_fragments_zero_set(true);

  NATIVE_TEST_ASSERT_EQ(0, meshcore_message_send_to_node(
                               s_public_key, true, 0U, s_payload, 1U));
  NATIVE_TEST_ASSERT_EQ(0, meshcore_test_runtime_expected_ack_used_count_get());
  NATIVE_TEST_ASSERT_EQ(
      1, meshcore_test_runtime_dispatcher_outbound_total_get());

  meshcore_deinit();
  return 0;
}

static int test_telemetry_pending_response_and_timeout(void)
{
  uint32_t tag = 0x11223344U;
  uint32_t got_tag = 0U;
  uint8_t key_prefix[MESHCORE_NODE_KEY_PREFIX_BYTES] = {0};
  uint8_t permission = 0U;
  unsigned long expires_at = 0UL;
  const uint8_t response[] = {0xA0U, 0xA1U, 0xA2U};

  NATIVE_TEST_ASSERT_EQ(0, init_runtime());
  NATIVE_TEST_ASSERT_EQ(0, meshcore_node_telemetry_request(
      s_public_key, MESHCORE_TELEM_PERM_BASE | MESHCORE_TELEM_PERM_LOCATION,
      &tag));
  NATIVE_TEST_ASSERT(meshcore_test_runtime_pending_telemetry_get(
      &got_tag, key_prefix, &permission, &expires_at));
  NATIVE_TEST_ASSERT_EQ(tag, got_tag);
  NATIVE_TEST_ASSERT_EQ((MESHCORE_TELEM_PERM_BASE | MESHCORE_TELEM_PERM_LOCATION),
                        permission);
  NATIVE_TEST_ASSERT(memcmp(key_prefix, s_public_key, sizeof(key_prefix)) == 0);
  NATIVE_TEST_ASSERT(expires_at > 0UL);

  NATIVE_TEST_ASSERT(!meshcore_test_runtime_simulate_telemetry_response_recv(
      s_public_key, tag + 1U, response, sizeof(response)));
  NATIVE_TEST_ASSERT_EQ(0U, meshcore_native_platform_telemetry_count_get());
  NATIVE_TEST_ASSERT(meshcore_test_runtime_simulate_telemetry_response_recv(
      s_public_key, tag, response, sizeof(response)));
  NATIVE_TEST_ASSERT(!meshcore_test_runtime_pending_telemetry_is_valid());
  NATIVE_TEST_ASSERT_EQ(1U, meshcore_native_platform_telemetry_count_get());
  NATIVE_TEST_ASSERT_EQ(tag, meshcore_native_platform_last_telemetry_tag_get());
  NATIVE_TEST_ASSERT_EQ(sizeof(response),
                        meshcore_native_platform_last_telemetry_payload_len_get());
  NATIVE_TEST_ASSERT(memcmp(meshcore_native_platform_last_telemetry_payload_get(),
                            response, sizeof(response)) == 0);

  meshcore_deinit();

  NATIVE_TEST_ASSERT_EQ(0, init_runtime());
  tag = 0x55667788U;
  NATIVE_TEST_ASSERT_EQ(0, meshcore_node_telemetry_request(
      s_public_key, MESHCORE_TELEM_PERM_BASE, &tag));
  NATIVE_TEST_ASSERT(meshcore_test_runtime_pending_telemetry_get(
      NULL, NULL, NULL, &expires_at));
  NATIVE_TEST_ASSERT_EQ(0, meshcore_timer_fired((uint32_t)expires_at));
  NATIVE_TEST_ASSERT(!meshcore_test_runtime_pending_telemetry_is_valid());

  meshcore_deinit();
  return 0;
}

static int test_binary_pending_response_publishes_and_clears(void)
{
  uint32_t tag = 0xABCDEF01U;
  uint32_t got_tag = 0U;
  uint8_t key_prefix[MESHCORE_NODE_KEY_PREFIX_BYTES] = {0};
  const uint8_t response[] = {0xB0U, 0xB1U};

  NATIVE_TEST_ASSERT_EQ(0, init_runtime());
  NATIVE_TEST_ASSERT_EQ(0, meshcore_node_binary_request_with_tag(
      s_public_key, s_payload, 1U, tag));
  NATIVE_TEST_ASSERT(meshcore_test_runtime_pending_binary_get(
      &got_tag, key_prefix, NULL));
  NATIVE_TEST_ASSERT_EQ(tag, got_tag);
  NATIVE_TEST_ASSERT(memcmp(key_prefix, s_public_key, sizeof(key_prefix)) == 0);

  NATIVE_TEST_ASSERT(!meshcore_test_runtime_simulate_binary_response_recv(
      s_public_key, tag + 1U, response, sizeof(response)));
  NATIVE_TEST_ASSERT_EQ(0U, meshcore_native_platform_binary_response_count_get());
  NATIVE_TEST_ASSERT(meshcore_test_runtime_simulate_binary_response_recv(
      s_public_key, tag, response, sizeof(response)));
  NATIVE_TEST_ASSERT(!meshcore_test_runtime_pending_binary_is_valid());
  NATIVE_TEST_ASSERT_EQ(1U, meshcore_native_platform_binary_response_count_get());
  NATIVE_TEST_ASSERT_EQ(tag,
                        meshcore_native_platform_last_binary_response_tag_get());
  NATIVE_TEST_ASSERT_EQ(
      sizeof(response),
      meshcore_native_platform_last_binary_response_payload_len_get());
  NATIVE_TEST_ASSERT(memcmp(
      meshcore_native_platform_last_binary_response_payload_get(), response,
      sizeof(response)) == 0);

  meshcore_deinit();
  return 0;
}

static int test_discover_path_and_trace_pending_events(void)
{
  uint32_t tag = 0x01020304U;
  uint32_t got_tag = 0U;
  uint8_t key_prefix[MESHCORE_NODE_KEY_PREFIX_BYTES] = {0};
  uint8_t path[] = {0x44U};
  int8_t tag_extra[sizeof(tag)];
  meshcore_common_peer_path_event_t const *peer_path;
  uint8_t trace_path[] = {0x51U};
  int8_t path_snrs[] = {1, 2, 3};

  memcpy(tag_extra, &tag, sizeof(tag));

  NATIVE_TEST_ASSERT_EQ(0, init_runtime());
  NATIVE_TEST_ASSERT_EQ(0, meshcore_node_discover_path_request(s_public_key,
                                                               &tag));
  NATIVE_TEST_ASSERT(meshcore_test_runtime_pending_discovery_get(
      &got_tag, key_prefix, NULL));
  NATIVE_TEST_ASSERT_EQ(tag, got_tag);
  NATIVE_TEST_ASSERT(memcmp(key_prefix, s_public_key, sizeof(key_prefix)) == 0);
  NATIVE_TEST_ASSERT(meshcore_test_runtime_simulate_peer_path_recv(
      s_public_key, path, sizeof(path), 1U, PAYLOAD_TYPE_RESPONSE, tag_extra,
      sizeof(tag_extra), NULL, 0U, 8));
  NATIVE_TEST_ASSERT(!meshcore_test_runtime_pending_discovery_is_valid());
  NATIVE_TEST_ASSERT_EQ(1U,
                        meshcore_native_platform_peer_path_publish_count_get());
  peer_path = meshcore_native_platform_last_peer_path_publish_get();
  NATIVE_TEST_ASSERT_EQ(tag, peer_path->tag);
  NATIVE_TEST_ASSERT_EQ(1U, peer_path->out_path_len);
  NATIVE_TEST_ASSERT_EQ(path[0], peer_path->out_path[0]);

  meshcore_deinit();

  NATIVE_TEST_ASSERT_EQ(0, init_runtime());
  meshcore_native_platform_peer_path_set(true, true, trace_path,
                                         sizeof(trace_path), 1U);
  tag = 0xAABBCCDDU;
  NATIVE_TEST_ASSERT_EQ(0, meshcore_node_trace_request(
                               trace_path, sizeof(trace_path), 1U, &tag));
  NATIVE_TEST_ASSERT(meshcore_test_runtime_pending_trace_get(&got_tag, NULL));
  NATIVE_TEST_ASSERT_EQ(tag, got_tag);
  NATIVE_TEST_ASSERT(meshcore_test_runtime_simulate_trace_recv(
      tag, 0U, path_snrs, sizeof(path_snrs), 4));
  NATIVE_TEST_ASSERT(!meshcore_test_runtime_pending_trace_is_valid());
  NATIVE_TEST_ASSERT_EQ(1U, meshcore_native_platform_trace_path_count_get());
  NATIVE_TEST_ASSERT_EQ(tag, meshcore_native_platform_last_trace_path_tag_get());
  NATIVE_TEST_ASSERT_EQ(1U, meshcore_native_platform_last_trace_path_state_get());
  NATIVE_TEST_ASSERT_EQ(2U,
                        meshcore_native_platform_last_trace_path_out_count_get());
  NATIVE_TEST_ASSERT_EQ(2U,
                        meshcore_native_platform_last_trace_path_return_count_get());

  meshcore_deinit();
  return 0;
}

static int test_ack_delivery_failure_and_capacity(void)
{
  struct meshcore_packet packet;
  struct meshcore_packet duplicate;
  uint32_t ack_crc = 0U;
  uint8_t payload = 0U;
  unsigned int i;

  NATIVE_TEST_ASSERT_EQ(0, init_runtime());
  NATIVE_TEST_ASSERT_EQ(0, meshcore_message_send_to_node(
                               s_public_key, true, 1U, s_payload, 1U));
  NATIVE_TEST_ASSERT(meshcore_test_runtime_expected_ack_peek(
      &ack_crc, NULL, NULL, NULL));

  meshcore_packet_init(&packet);
  packet.header = (uint8_t)((PAYLOAD_TYPE_ACK << PH_TYPE_SHIFT) |
                            ROUTE_TYPE_FLOOD);
  meshcore_native_platform_event_fail_set(true);
  meshcore_runtime_on_ack_recv(&packet, ack_crc);
  NATIVE_TEST_ASSERT(meshcore_packet_is_marked_do_not_retransmit(&packet));
  NATIVE_TEST_ASSERT_EQ(0, meshcore_test_runtime_expected_ack_used_count_get());
  NATIVE_TEST_ASSERT_EQ(1U, meshcore_native_platform_message_ack_count_get());
  NATIVE_TEST_ASSERT_EQ(1U,
                        meshcore_native_platform_request_error_count_get());

  meshcore_native_platform_event_fail_set(false);
  meshcore_packet_init(&duplicate);
  duplicate.header = (uint8_t)((PAYLOAD_TYPE_ACK << PH_TYPE_SHIFT) |
                               ROUTE_TYPE_FLOOD);
  meshcore_runtime_on_ack_recv(&duplicate, ack_crc);
  NATIVE_TEST_ASSERT(!meshcore_packet_is_marked_do_not_retransmit(&duplicate));
  NATIVE_TEST_ASSERT_EQ(0, meshcore_test_runtime_expected_ack_used_count_get());
  NATIVE_TEST_ASSERT_EQ(1U, meshcore_native_platform_message_ack_count_get());
  meshcore_deinit();

  NATIVE_TEST_ASSERT_EQ(0, init_runtime());
  for (i = 0U; i < 8U; i++) {
    payload = (uint8_t)i;
    NATIVE_TEST_ASSERT_EQ(0, meshcore_message_send_to_node(
                                 s_public_key, true, 0U, &payload, 1U));
  }
  NATIVE_TEST_ASSERT_EQ(8, meshcore_test_runtime_expected_ack_used_count_get());
  payload = 0xA5U;
  NATIVE_TEST_ASSERT_EQ(-ENOBUFS, meshcore_message_send_to_node(
                                        s_public_key, true, 0U, &payload, 1U));
  NATIVE_TEST_ASSERT_EQ(8, meshcore_test_runtime_expected_ack_used_count_get());

  meshcore_deinit();
  return 0;
}

static int test_same_surface_overwrite_and_generated_tag_semantics(void)
{
  meshcore_common_peer_identity_t peer = {0};
  struct meshcore_packet packet;
  uint8_t request_data[sizeof(uint32_t) + 1U] = {0U, 0U, 0U, 0U, 0xA5U};
  uint8_t secret[MESHCORE_PUBLIC_KEY_SIZE] = {0};
  const meshcore_common_binary_request_event_t *request_event;
  uint32_t zero = 0U;
  uint32_t maximum = UINT32_MAX;
  uint32_t tag = 1U;
  uint8_t path[] = {0x31U};

  NATIVE_TEST_ASSERT_EQ(0, init_runtime());
  NATIVE_TEST_ASSERT_EQ(
      0, meshcore_node_discover_path_request(s_public_key, &zero));
  NATIVE_TEST_ASSERT(zero != 0U);
  NATIVE_TEST_ASSERT(meshcore_test_runtime_pending_discovery_get(
      &tag, NULL, NULL));
  NATIVE_TEST_ASSERT_EQ(zero, tag);
  NATIVE_TEST_ASSERT_EQ(
      0, meshcore_node_discover_path_request(s_public_key, &maximum));
  NATIVE_TEST_ASSERT(meshcore_test_runtime_pending_discovery_get(
      &tag, NULL, NULL));
  NATIVE_TEST_ASSERT_EQ(UINT32_MAX, tag);

  zero = 0U;
  NATIVE_TEST_ASSERT_EQ(
      0, meshcore_node_trace_request(path, sizeof(path), 1U, &zero));
  NATIVE_TEST_ASSERT(zero != 0U);
  NATIVE_TEST_ASSERT(meshcore_test_runtime_pending_trace_get(&tag, NULL));
  NATIVE_TEST_ASSERT_EQ(zero, tag);
  NATIVE_TEST_ASSERT_EQ(
      0,
      meshcore_node_trace_request(path, sizeof(path), 1U, &maximum));
  NATIVE_TEST_ASSERT(meshcore_test_runtime_pending_trace_get(&tag, NULL));
  NATIVE_TEST_ASSERT_EQ(UINT32_MAX, tag);

  zero = 0U;
  NATIVE_TEST_ASSERT_EQ(
      0, meshcore_node_telemetry_request(
             s_public_key, MESHCORE_TELEM_PERM_BASE, &zero));
  NATIVE_TEST_ASSERT(zero != 0U);
  NATIVE_TEST_ASSERT(meshcore_test_runtime_pending_telemetry_get(
      &tag, NULL, NULL, NULL));
  NATIVE_TEST_ASSERT_EQ(zero, tag);
  NATIVE_TEST_ASSERT_EQ(
      0, meshcore_node_telemetry_request(
             s_public_key, MESHCORE_TELEM_PERM_BASE, &maximum));
  NATIVE_TEST_ASSERT(meshcore_test_runtime_pending_telemetry_get(
      &tag, NULL, NULL, NULL));
  NATIVE_TEST_ASSERT_EQ(UINT32_MAX, tag);

  NATIVE_TEST_ASSERT_EQ(0, meshcore_node_binary_request_with_tag(
                               s_public_key, s_payload, 1U, 0U));
  NATIVE_TEST_ASSERT(meshcore_test_runtime_pending_binary_get(
      &tag, NULL, NULL));
  NATIVE_TEST_ASSERT(tag != 0U);
  NATIVE_TEST_ASSERT_EQ(0, meshcore_node_binary_request_with_tag(
                               s_public_key, s_payload, 1U, maximum));
  NATIVE_TEST_ASSERT_EQ(UINT32_MAX, maximum);
  NATIVE_TEST_ASSERT(meshcore_test_runtime_pending_binary_get(
      &tag, NULL, NULL));
  NATIVE_TEST_ASSERT_EQ(UINT32_MAX, tag);
  meshcore_deinit();

  NATIVE_TEST_ASSERT_EQ(0, init_runtime());
  NATIVE_TEST_ASSERT_EQ(
      0, meshcore_node_binary_request(s_public_key, s_payload, 1U));
  NATIVE_TEST_ASSERT(meshcore_test_runtime_pending_binary_get(
      &tag, NULL, NULL));
  NATIVE_TEST_ASSERT(tag != 0U);
  meshcore_deinit();

  NATIVE_TEST_ASSERT_EQ(0, init_runtime());
  memcpy(peer.public_key, s_public_key, sizeof(peer.public_key));
  meshcore_packet_init(&packet);
  packet.header = (uint8_t)((PAYLOAD_TYPE_REQ << PH_TYPE_SHIFT) |
                            ROUTE_TYPE_DIRECT);
  meshcore_runtime_on_peer_data_recv(&packet, PAYLOAD_TYPE_REQ, &peer, secret,
                                     request_data, sizeof(request_data));
  NATIVE_TEST_ASSERT_EQ(
      1U, meshcore_native_platform_binary_request_count_get());
  request_event = meshcore_native_platform_last_binary_request_get();
  NATIVE_TEST_ASSERT_EQ(0U, request_event->tag);
  meshcore_deinit();
  return 0;
}

static int test_delivery_failures_consume_correlations(void)
{
  uint32_t tag = 0x10203040U;
  uint8_t path[] = {0x42U};
  int8_t tag_extra[sizeof(tag)];
  int8_t path_snrs[] = {1, 2, 3};
  const uint8_t response[] = {0xB1U};

  memcpy(tag_extra, &tag, sizeof(tag));
  NATIVE_TEST_ASSERT_EQ(0, init_runtime());
  NATIVE_TEST_ASSERT_EQ(0, meshcore_node_discover_path_request(
                               s_public_key, &tag));
  NATIVE_TEST_ASSERT_EQ(0, meshcore_node_trace_request(
                               path, sizeof(path), 1U, &tag));
  NATIVE_TEST_ASSERT_EQ(0, meshcore_node_telemetry_request(
                               s_public_key, MESHCORE_TELEM_PERM_BASE, &tag));
  NATIVE_TEST_ASSERT_EQ(0, meshcore_node_binary_request_with_tag(
                               s_public_key, s_payload, 1U, tag));

  meshcore_native_platform_event_fail_set(true);
  NATIVE_TEST_ASSERT(meshcore_test_runtime_simulate_peer_path_recv(
      s_public_key, path, sizeof(path), 1U, PAYLOAD_TYPE_RESPONSE, tag_extra,
      sizeof(tag_extra), NULL, 0U, 8));
  NATIVE_TEST_ASSERT(meshcore_test_runtime_simulate_trace_recv(
      tag, 0U, path_snrs, sizeof(path_snrs), 4));
  NATIVE_TEST_ASSERT(meshcore_test_runtime_simulate_telemetry_response_recv(
      s_public_key, tag, response, sizeof(response)));
  NATIVE_TEST_ASSERT(meshcore_test_runtime_simulate_binary_response_recv(
      s_public_key, tag, response, sizeof(response)));
  NATIVE_TEST_ASSERT(!meshcore_test_runtime_pending_discovery_is_valid());
  NATIVE_TEST_ASSERT(!meshcore_test_runtime_pending_trace_is_valid());
  NATIVE_TEST_ASSERT(!meshcore_test_runtime_pending_telemetry_is_valid());
  NATIVE_TEST_ASSERT(!meshcore_test_runtime_pending_binary_is_valid());
  NATIVE_TEST_ASSERT_EQ(4U,
                        meshcore_native_platform_request_error_count_get());
  NATIVE_TEST_ASSERT_EQ(
      1U, meshcore_native_platform_peer_path_publish_count_get());
  NATIVE_TEST_ASSERT_EQ(1U, meshcore_native_platform_trace_path_count_get());
  NATIVE_TEST_ASSERT_EQ(1U, meshcore_native_platform_telemetry_count_get());
  NATIVE_TEST_ASSERT_EQ(
      1U, meshcore_native_platform_binary_response_count_get());

  meshcore_native_platform_event_fail_set(false);
  NATIVE_TEST_ASSERT(!meshcore_test_runtime_simulate_peer_path_recv(
      s_public_key, path, sizeof(path), 1U, PAYLOAD_TYPE_RESPONSE, tag_extra,
      sizeof(tag_extra), NULL, 0U, 8));
  NATIVE_TEST_ASSERT(!meshcore_test_runtime_simulate_trace_recv(
      tag, 0U, path_snrs, sizeof(path_snrs), 4));
  NATIVE_TEST_ASSERT(!meshcore_test_runtime_simulate_telemetry_response_recv(
      s_public_key, tag, response, sizeof(response)));
  NATIVE_TEST_ASSERT(!meshcore_test_runtime_simulate_binary_response_recv(
      s_public_key, tag, response, sizeof(response)));
  NATIVE_TEST_ASSERT_EQ(
      1U, meshcore_native_platform_peer_path_publish_count_get());
  NATIVE_TEST_ASSERT_EQ(1U, meshcore_native_platform_trace_path_count_get());
  NATIVE_TEST_ASSERT_EQ(1U, meshcore_native_platform_telemetry_count_get());
  NATIVE_TEST_ASSERT_EQ(
      1U, meshcore_native_platform_binary_response_count_get());

  meshcore_deinit();
  return 0;
}

static int test_host_error_and_pool_failure_roll_back(void)
{
  meshcore_common_node_identity_t identity = {0};
  meshcore_common_peer_path_t peer_path = {0};
  const uint8_t control[] = {0x80U};
  uint32_t previous_tag = 0x01020304U;
  uint32_t replacement_tag = 0xCAFEBABEU;
  uint32_t pending_tag = 0U;
  unsigned int i;

  NATIVE_TEST_ASSERT_EQ(0, init_runtime());
  meshcore_native_platform_peer_path_error_set(-EIO);
  NATIVE_TEST_ASSERT_EQ(0, meshcore_platform_node_identity_get(&identity));
  NATIVE_TEST_ASSERT_EQ(
      -EIO, meshcore_platform_peer_path_get_by_key(s_public_key, &peer_path));
  NATIVE_TEST_ASSERT_EQ(-EIO, meshcore_node_binary_request_with_tag(
                                  s_public_key, s_payload, 1U,
                                  replacement_tag));
  NATIVE_TEST_ASSERT(!meshcore_test_runtime_pending_binary_is_valid());
  NATIVE_TEST_ASSERT_EQ(
      0, meshcore_test_runtime_dispatcher_outbound_total_get());
  NATIVE_TEST_ASSERT_EQ(-EIO, meshcore_message_send_to_node(
                                  s_public_key, false, 0U, s_payload, 1U));
  NATIVE_TEST_ASSERT_EQ(0, meshcore_test_runtime_expected_ack_used_count_get());
  NATIVE_TEST_ASSERT_EQ(
      0, meshcore_test_runtime_dispatcher_outbound_total_get());
  meshcore_native_platform_peer_path_error_set(0);
  NATIVE_TEST_ASSERT_EQ(0, meshcore_message_send_to_node(
                               s_public_key, false, 0U, s_payload, 1U));
  NATIVE_TEST_ASSERT_EQ(1, meshcore_test_runtime_expected_ack_used_count_get());
  meshcore_deinit();

  NATIVE_TEST_ASSERT_EQ(0, init_runtime());
  NATIVE_TEST_ASSERT_EQ(0, meshcore_node_binary_request_with_tag(
                               s_public_key, s_payload, 1U, previous_tag));
  NATIVE_TEST_ASSERT(meshcore_test_runtime_pending_binary_get(
      &pending_tag, NULL, NULL));
  NATIVE_TEST_ASSERT_EQ(previous_tag, pending_tag);
  meshcore_native_platform_peer_path_error_set(-EIO);
  NATIVE_TEST_ASSERT_EQ(-EIO, meshcore_node_binary_request_with_tag(
                                  s_public_key, s_payload, 1U,
                                  replacement_tag));
  NATIVE_TEST_ASSERT(meshcore_test_runtime_pending_binary_get(
      &pending_tag, NULL, NULL));
  NATIVE_TEST_ASSERT_EQ(previous_tag, pending_tag);
  NATIVE_TEST_ASSERT_EQ(
      1, meshcore_test_runtime_dispatcher_outbound_total_get());
  meshcore_deinit();

  NATIVE_TEST_ASSERT_EQ(0, init_runtime());
  NATIVE_TEST_ASSERT_EQ(0, meshcore_node_binary_request_with_tag(
                               s_public_key, s_payload, 1U, previous_tag));
  for (i = 0U; i < 9U; i++) {
    NATIVE_TEST_ASSERT_EQ(
        0, meshcore_control_data_send(control, sizeof(control)));
  }
  NATIVE_TEST_ASSERT_EQ(-ENOBUFS, meshcore_control_data_send(
                                        control, sizeof(control)));
  NATIVE_TEST_ASSERT_EQ(-ENOBUFS, meshcore_node_binary_request_with_tag(
                                        s_public_key, s_payload, 1U,
                                        replacement_tag));
  NATIVE_TEST_ASSERT(meshcore_test_runtime_pending_binary_get(
      &pending_tag, NULL, NULL));
  NATIVE_TEST_ASSERT_EQ(previous_tag, pending_tag);

  meshcore_deinit();
  return 0;
}

int main(void)
{
  fill_buffers();

  NATIVE_TEST_ASSERT_EQ(0, test_expected_ack_registers_publishes_and_clears());
  NATIVE_TEST_ASSERT_EQ(0, test_zero_ack_hash_sends_without_registration());
  NATIVE_TEST_ASSERT_EQ(0, test_telemetry_pending_response_and_timeout());
  NATIVE_TEST_ASSERT_EQ(0, test_binary_pending_response_publishes_and_clears());
  NATIVE_TEST_ASSERT_EQ(0, test_discover_path_and_trace_pending_events());
  NATIVE_TEST_ASSERT_EQ(0, test_ack_delivery_failure_and_capacity());
  NATIVE_TEST_ASSERT_EQ(
      0, test_same_surface_overwrite_and_generated_tag_semantics());
  NATIVE_TEST_ASSERT_EQ(0, test_delivery_failures_consume_correlations());
  NATIVE_TEST_ASSERT_EQ(0, test_host_error_and_pool_failure_roll_back());

  return 0;
}
