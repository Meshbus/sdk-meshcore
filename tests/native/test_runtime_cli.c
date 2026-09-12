// SPDX-License-Identifier: MIT
/* Copyright (c) 2026 FoBE Studio */

#include "native_test.h"
#include "fake_platform.h"
#include "meshcore/platform.h"
#include "meshcore/runtime.h"
#include "meshcore_runtime_internal.h"
#include "meshcore_runtime_bridge.h"
#include <errno.h>
#include <string.h>

/* MESHCORE_API_COVERAGE: meshcore_cli_send_to_node */
/* MESHCORE_TYPE_COVERAGE: meshcore_common_cli_type meshcore_common_cli_event */
/* Runtime evidence:
 * .reference/meshcore/src/helpers/BaseChatMesh.cpp
 * .reference/meshcore/examples/companion_radio/MyMesh.cpp
 * .reference/meshcore/examples/simple_repeater/MyMesh.cpp
 * .reference/meshcore/examples/simple_room_server/MyMesh.cpp
 * .reference/meshcore/examples/simple_sensor/SensorMesh.cpp
 */

static const uint8_t peer_key[32] = {0x21U};
static const uint8_t secret[32] = {0};
static const uint8_t path[6] = {1U, 2U, 3U, 4U, 5U, 6U};
static bool oracle;
static bool flood_input;

static int begin(meshcore_common_node_role_t role, uint8_t path_len)
{
  meshcore_deinit();
  meshcore_native_platform_reset();
  flood_input = false;
  meshcore_native_platform_role_set(role);
  meshcore_native_platform_time_set(1U, 100U);
  meshcore_native_platform_peer_secret_set(true, secret, peer_key);
  meshcore_native_platform_peer_path_set(true, path_len != 0xFFU, path,
      path_len == 0xFFU ? 0U : (uint8_t)(((path_len >> 6) + 1U) * (path_len & 63U)),
      path_len == 0xFFU ? 1U : (uint8_t)((path_len >> 6) + 1U));
  return meshcore_init();
}

static int check_output(const char *label, unsigned role, unsigned type,
                        uint8_t path_len, uint32_t delay, uint32_t timestamp,
                        uint8_t flags, const char *text, size_t text_len)
{
  struct meshcore_runtime *ctx = meshcore_runtime_context_get();
  struct meshcore_packet *packet;
  uint8_t data[MESHCORE_MAX_RAW_DATA_PAYLOAD_LEN];
  uint32_t due = 0U, got_timestamp;
  size_t i;
  int len;

  NATIVE_TEST_ASSERT(meshcore_packet_queue_manager_next_outbound_scheduled_get(
      &ctx->packet_manager, 1U, &due));
  NATIVE_TEST_ASSERT_EQ(1U + delay, due);
  packet = meshcore_packet_queue_manager_get_next_outbound(&ctx->packet_manager, due);
  NATIVE_TEST_ASSERT(packet != NULL);
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_TXT_MSG, meshcore_packet_get_payload_type(packet));
  NATIVE_TEST_ASSERT_EQ(path_len == 0xFFU ? ROUTE_TYPE_FLOOD : ROUTE_TYPE_DIRECT,
                        meshcore_packet_get_route_type(packet));
  NATIVE_TEST_ASSERT_EQ(path_len == 0xFFU ? 0U : path_len, packet->path_len);
  if (path_len != 0xFFU) {
    NATIVE_TEST_ASSERT(memcmp(packet->path, path,
        meshcore_packet_get_path_byte_len(packet)) == 0);
  }
  len = meshcore_utils_mac_then_decrypt(secret, data, &packet->payload[2],
                                       packet->payload_len - 2);
  NATIVE_TEST_ASSERT(len >= (int)(5U + text_len));
  memcpy(&got_timestamp, data, sizeof(got_timestamp));
  NATIVE_TEST_ASSERT_EQ(timestamp, got_timestamp);
  NATIVE_TEST_ASSERT_EQ(flags, data[4]);
  NATIVE_TEST_ASSERT(memcmp(&data[5], text, text_len) == 0);
  for (i = 5U + text_len; i < (size_t)len; i++) {
    NATIVE_TEST_ASSERT_EQ(0U, data[i]);
  }
  if (oracle) {
    printf("%s %u %u %u %u %u %u ", label, role, type, path_len, delay,
           timestamp, flags);
    for (i = 0U; i < text_len; i++) printf("%02x", data[5U + i]);
    printf("\n");
  }
  meshcore_packet_queue_manager_free(&ctx->packet_manager, packet);
  for (i = 0U; i < MESHCORE_RUNTIME_EXPECTED_ACK_TABLE_SIZE; i++) {
    NATIVE_TEST_ASSERT(!ctx->expected_acks[i].valid);
  }
  return 0;
}

static int receive(uint8_t type, const char *text, size_t len, bool radio)
{
  meshcore_common_peer_identity_t peer = {0};
  struct meshcore_packet packet;
  uint8_t data[5U + MESHCORE_MAX_MESSAGE_TX_LEN + 16U] = {0};
  uint8_t raw[MESHCORE_MAX_TRANS_UNIT_LEN];
  uint32_t timestamp = 100U;
  int encrypted_len;
  memcpy(peer.public_key, peer_key, sizeof(peer_key));
  meshcore_packet_init(&packet);
  packet.header = (PAYLOAD_TYPE_TXT_MSG << PH_TYPE_SHIFT) |
      (flood_input ? ROUTE_TYPE_FLOOD : ROUTE_TYPE_DIRECT);
  packet.snr_q4 = 12;
  memcpy(data, &timestamp, 4U);
  data[4] = (uint8_t)((type << 2) | 2U);
  memcpy(&data[5], text, len);
  if (!radio) {
    meshcore_runtime_on_peer_data_recv(&packet, PAYLOAD_TYPE_TXT_MSG, &peer,
                                       secret, data, 5U + len);
    return 0;
  }
  packet.payload[0] = 0x10U; /* Fake local public-key hash. */
  packet.payload[1] = peer_key[0];
  encrypted_len = meshcore_utils_encrypt_then_mac(secret, &packet.payload[2],
                                                 data, (int)(5U + len));
  NATIVE_TEST_ASSERT(encrypted_len > 0);
  packet.payload_len = (uint16_t)(2 + encrypted_len);
  return meshcore_radio_rx_inject(raw, meshcore_packet_write_to(&packet, raw),
                                  -80, 12, 1U);
}

static int test_send_and_receive(void)
{
  const uint8_t paths[] = {0xFFU, 0x00U, 0x42U, 0x82U};
  uint8_t type;
  unsigned role;
  size_t i;
  for (type = 1U; type <= 3U; type += 2U) {
    for (i = 0U; i < sizeof(paths); i++) {
      NATIVE_TEST_ASSERT_EQ(0, begin(MESHCORE_COMMON_NODE_ROLE_CHAT, paths[i]));
      NATIVE_TEST_ASSERT_EQ(0, meshcore_cli_send_to_node(peer_key,
          (meshcore_common_cli_type_t)type, 2U, "ver", 3U));
      NATIVE_TEST_ASSERT_EQ(0, check_output("send", 1U, type, paths[i], 0U,
                                           100U, (uint8_t)((type << 2) | 2U), "ver", 3U));
      for (role = 1U; role <= 4U; role++) {
        const meshcore_common_cli_event_t *event;
        NATIVE_TEST_ASSERT_EQ(0, begin((meshcore_common_node_role_t)role, paths[i]));
        meshcore_native_platform_cli_result_set(type == 1U && role == 1U ? 0 : 2, "OK");
        NATIVE_TEST_ASSERT_EQ(0, receive(type, "ver", 3U, false));
        NATIVE_TEST_ASSERT_EQ(1U, meshcore_native_platform_cli_count_get());
        event = meshcore_native_platform_cli_event_get();
        NATIVE_TEST_ASSERT_EQ(type, event->type);
        NATIVE_TEST_ASSERT_EQ(100U, event->sender_timestamp);
        NATIVE_TEST_ASSERT_EQ(2U, event->attempt);
        NATIVE_TEST_ASSERT_EQ(12, event->rx_snr_q4);
        NATIVE_TEST_ASSERT_EQ(3U, event->text_len);
        NATIVE_TEST_ASSERT(strcmp(event->text, "ver") == 0);
        NATIVE_TEST_ASSERT(memcmp(event->public_key, peer_key, 32U) == 0);
        NATIVE_TEST_ASSERT_EQ(0U, meshcore_native_platform_message_count_get());
        if (type == 1U && role == 1U) {
          NATIVE_TEST_ASSERT_EQ(0U, meshcore_native_platform_cli_capacity_get());
          NATIVE_TEST_ASSERT_EQ(0, meshcore_packet_queue_manager_get_outbound_total(
              &meshcore_runtime_context_get()->packet_manager));
          if (oracle) printf("data %u %u %u\n", role, type, paths[i]);
        } else {
          uint32_t delay = role == 3U ? 300U : role == 4U ? 1000U : 600U;
          NATIVE_TEST_ASSERT_EQ(0, check_output("reply", role, type, paths[i],
                                               delay, 101U, 4U, "OK", 2U));
        }
      }
    }
  }
  return 0;
}

static int test_errors_and_boundaries(void)
{
  const int errors[] = {0, -EACCES, -ENOTSUP, -EIO, 161};
  char text[161];
  size_t i;
  memset(text, 'x', sizeof(text));
  meshcore_deinit();
  NATIVE_TEST_ASSERT(meshcore_cli_send_to_node(peer_key, MESHCORE_COMMON_CLI_COMMAND,
                                             0U, "x", 1U) < 0);
  NATIVE_TEST_ASSERT_EQ(0, begin(MESHCORE_COMMON_NODE_ROLE_CHAT, 0U));
  NATIVE_TEST_ASSERT_EQ(-EINVAL, meshcore_cli_send_to_node(NULL, 3, 0, "x", 1));
  NATIVE_TEST_ASSERT_EQ(-EINVAL, meshcore_cli_send_to_node(peer_key, 2, 0, "x", 1));
  NATIVE_TEST_ASSERT_EQ(-EINVAL, meshcore_cli_send_to_node(peer_key, 3, 4, "x", 1));
  NATIVE_TEST_ASSERT_EQ(-EINVAL, meshcore_cli_send_to_node(peer_key, 3, 0, NULL, 1));
  NATIVE_TEST_ASSERT_EQ(-EINVAL, meshcore_cli_send_to_node(peer_key, 3, 0, "", 0));
  NATIVE_TEST_ASSERT_EQ(-EINVAL, meshcore_cli_send_to_node(peer_key, 3, 0, "x\0y", 3));
  NATIVE_TEST_ASSERT_EQ(-EINVAL, meshcore_cli_send_to_node(peer_key, 3, 0, text, 161));
  meshcore_native_platform_peer_path_error_set(-EIO);
  NATIVE_TEST_ASSERT_EQ(-EIO, meshcore_cli_send_to_node(peer_key, 3, 0, "x", 1));
  meshcore_native_platform_peer_path_error_set(0);
  meshcore_native_platform_peer_path_set(true, true, path, 1U, 3U);
  NATIVE_TEST_ASSERT_EQ(-EINVAL, meshcore_cli_send_to_node(peer_key, 3, 0, "x", 1));
  for (i = 0U; i < sizeof(errors) / sizeof(errors[0]); i++) {
    NATIVE_TEST_ASSERT_EQ(0, begin(MESHCORE_COMMON_NODE_ROLE_CHAT, 0U));
    meshcore_native_platform_cli_result_set(errors[i], NULL);
    NATIVE_TEST_ASSERT_EQ(0, receive(3, "ver", 3, false));
    NATIVE_TEST_ASSERT_EQ(0, meshcore_packet_queue_manager_get_outbound_total(
        &meshcore_runtime_context_get()->packet_manager));
    NATIVE_TEST_ASSERT_EQ(errors[i] == -EIO || errors[i] == 161 ? 1U : 0U,
                          meshcore_native_platform_request_error_count_get());
  }
  NATIVE_TEST_ASSERT_EQ(0, begin(MESHCORE_COMMON_NODE_ROLE_CHAT, 0U));
  meshcore_native_platform_cli_result_set(2, "x\0");
  NATIVE_TEST_ASSERT_EQ(0, receive(3, "ver", 3, false));
  NATIVE_TEST_ASSERT_EQ(-EINVAL, meshcore_native_platform_last_request_error_get());
  NATIVE_TEST_ASSERT_EQ(0, begin(MESHCORE_COMMON_NODE_ROLE_CHAT, 0U));
  flood_input = true;
  meshcore_native_platform_cli_result_set(0, NULL);
  NATIVE_TEST_ASSERT_EQ(0, receive(1, "data", 4, true));
  NATIVE_TEST_ASSERT_EQ(1U, meshcore_native_platform_cli_count_get());
  NATIVE_TEST_ASSERT_EQ(MESHCORE_COMMON_MESSAGE_ROUTE_FLOOD,
                        meshcore_native_platform_cli_event_get()->route);
  NATIVE_TEST_ASSERT_EQ(0, meshcore_packet_queue_manager_get_outbound_total(
      &meshcore_runtime_context_get()->packet_manager));
  NATIVE_TEST_ASSERT_EQ(0, begin(MESHCORE_COMMON_NODE_ROLE_CHAT, 0U));
  meshcore_native_platform_cli_result_set(2, "OK");
  NATIVE_TEST_ASSERT_EQ(0, receive(1, "data", 4, false));
  NATIVE_TEST_ASSERT_EQ(-EINVAL, meshcore_native_platform_last_request_error_get());
  NATIVE_TEST_ASSERT_EQ(0, begin(MESHCORE_COMMON_NODE_ROLE_CHAT, 0U));
  meshcore_native_platform_cli_result_set(2, "OK");
  meshcore_native_platform_peer_path_error_set(-EIO);
  NATIVE_TEST_ASSERT_EQ(0, receive(3, "ver", 3, false));
  NATIVE_TEST_ASSERT_EQ(-EIO, meshcore_native_platform_last_request_error_get());
  NATIVE_TEST_ASSERT_EQ(0, meshcore_packet_queue_manager_get_outbound_total(
      &meshcore_runtime_context_get()->packet_manager));
  NATIVE_TEST_ASSERT_EQ(0, begin(MESHCORE_COMMON_NODE_ROLE_CHAT, 0U));
  NATIVE_TEST_ASSERT_EQ(0, meshcore_cli_send_to_node(peer_key, 3, 0, text, 160));
  NATIVE_TEST_ASSERT_EQ(0, check_output("max", 1U, 3U, 0U, 0U, 100U, 12U, text, 160U));
  NATIVE_TEST_ASSERT_EQ(0, meshcore_cli_send_to_node(peer_key, 3, 0, "next", 4));
  NATIVE_TEST_ASSERT_EQ(0, check_output("unique", 1U, 3U, 0U, 0U, 101U, 12U, "next", 4U));
  NATIVE_TEST_ASSERT_EQ(0, begin(MESHCORE_COMMON_NODE_ROLE_CHAT, 0U));
  for (i = 0U; i < MESHCORE_RUNTIME_PACKET_POOL_SIZE; i++) {
    NATIVE_TEST_ASSERT_EQ(0, meshcore_cli_send_to_node(peer_key, 3, 0, "x", 1));
  }
  NATIVE_TEST_ASSERT_EQ(-ENOBUFS, meshcore_cli_send_to_node(peer_key, 3, 0, "x", 1));
  NATIVE_TEST_ASSERT_EQ(0, begin(MESHCORE_COMMON_NODE_ROLE_CHAT, 0U));
  NATIVE_TEST_ASSERT_EQ(0, receive(3, text, 161, false));
  NATIVE_TEST_ASSERT_EQ(0U, meshcore_native_platform_cli_count_get());
  NATIVE_TEST_ASSERT_EQ(0, begin(MESHCORE_COMMON_NODE_ROLE_CHAT, 0U));
  meshcore_native_platform_cli_result_set(160, text);
  NATIVE_TEST_ASSERT_EQ(0, receive(3, text, 160, true));
  NATIVE_TEST_ASSERT_EQ(1U, meshcore_native_platform_cli_count_get());
  NATIVE_TEST_ASSERT_EQ(601U, meshcore_native_platform_last_timer_deadline_get());
  NATIVE_TEST_ASSERT_EQ(0U, meshcore_native_platform_radio_send_count_get());
  NATIVE_TEST_ASSERT_EQ(0, meshcore_timer_fired(600U));
  NATIVE_TEST_ASSERT_EQ(0U, meshcore_native_platform_radio_send_count_get());
  NATIVE_TEST_ASSERT_EQ(0, meshcore_timer_fired(601U));
  NATIVE_TEST_ASSERT_EQ(1U, meshcore_native_platform_radio_send_count_get());
  return 0;
}

int main(int argc, char **argv)
{
  oracle = argc == 2 && strcmp(argv[1], "--oracle") == 0;
  NATIVE_TEST_ASSERT_EQ(0, test_send_and_receive());
  if (oracle) {
    const float factors[][2] = {{0.5f, 0.2f}, {1.25f, 0.75f}, {0.0f, 0.0f}};
    const uint8_t policy_paths[] = {0U, 0x42U, 0x82U};
    unsigned i, p, transport;
    for (i = 0U; i < 3U; i++) {
      struct meshcore_packet packet;
      meshcore_common_node_runtime_policy_t policy = {0};
      NATIVE_TEST_ASSERT_EQ(0, begin(MESHCORE_COMMON_NODE_ROLE_CHAT, 0U));
      meshcore_packet_init(&packet);
      packet.payload_len = 10U;
      policy.path_hash_size = 1U;
      policy.client_repeat = true;
      policy.tx_delay_factor = factors[i][0];
      policy.direct_tx_delay_factor = factors[i][1];
      meshcore_native_platform_policy_set(&policy);
      for (p = 0U; p < 3U; p++) {
        for (transport = 0U; transport < 2U; transport++) {
          packet.path_len = policy_paths[p];
          packet.header = transport ? ROUTE_TYPE_TRANSPORT_FLOOD : ROUTE_TYPE_DIRECT;
          printf("policy %u %u %u %u %u\n", i, p, transport,
              meshcore_runtime_protocol_get_retransmit_delay(NULL, NULL, &packet),
              meshcore_runtime_protocol_get_direct_retransmit_delay(NULL, NULL, &packet));
        }
      }
    }
  }
  if (!oracle) NATIVE_TEST_ASSERT_EQ(0, test_errors_and_boundaries());
  meshcore_deinit();
  return 0;
}
