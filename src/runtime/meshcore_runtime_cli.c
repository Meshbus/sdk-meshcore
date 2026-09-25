/* FoBE Studio contributions: Apache-2.0; upstream-derived portions: MIT.
 * See LICENSING.md and LICENSES/MIT-MeshCore.txt. */
// SPDX-License-Identifier: Apache-2.0 AND MIT
/* Copyright (c) 2026 FoBE Studio */

#include "meshcore_runtime_internal.h"

#include <errno.h>
#include <string.h>

#include "meshcore_platform_bridge.h"

/* Protocol framing and scheduling only; command execution belongs to hosts. */
static int cli_send(const uint8_t *public_key, const uint8_t *secret,
                    meshcore_common_cli_type_t type, uint8_t attempt,
                    uint32_t timestamp, const char *text, size_t text_len,
                    uint32_t delay_ms, uint8_t flood_hash_size)
{
  struct meshcore_runtime *ctx = meshcore_runtime_context_get();
  meshcore_common_peer_path_t path;
  struct meshcore_identity recipient;
  struct meshcore_packet *packet;
  uint8_t data[5U + MESHCORE_MAX_MESSAGE_TX_LEN];
  uint8_t path_len = 0U;
  int rc = meshcore_runtime_peer_path_get(public_key, &path, &path_len);

  if (rc != 0 && rc != -ENOENT) {
    return rc;
  }
  memcpy(data, &timestamp, sizeof(timestamp));
  data[4] = (uint8_t)(((uint8_t)type << 2) | attempt);
  memcpy(&data[5], text, text_len);
  meshcore_identity_init_from_pub_key(&recipient, public_key);
  packet = meshcore_mesh_create_datagram(&ctx->mesh, PAYLOAD_TYPE_TXT_MSG,
      &recipient, secret, data, 5U + text_len);
  if (packet == NULL) {
    return -ENOBUFS;
  }
  if (rc == 0) {
    return meshcore_mesh_send_direct(&ctx->mesh, packet, path.out_path,
                                      path_len, delay_ms);
  }
  return meshcore_mesh_send_flood(&ctx->mesh, packet, delay_ms,
                                   flood_hash_size);
}

int meshcore_cli_send_to_node(const uint8_t *public_key,
                              meshcore_common_cli_type_t type, uint8_t attempt,
                              const char *text, size_t text_len)
{
  struct meshcore_runtime *ctx = meshcore_runtime_context_get();
  uint8_t secret[MESHCORE_PUBLIC_KEY_SIZE];
  uint32_t timestamp;
  int rc = meshcore_runtime_require_initialized();

  if (rc != 0) {
    return rc;
  }
  if (public_key == NULL || text == NULL || text_len == 0U ||
      text_len > MESHCORE_MAX_MESSAGE_TX_LEN || attempt > 3U ||
      (type != MESHCORE_COMMON_CLI_DATA && type != MESHCORE_COMMON_CLI_COMMAND) ||
      memchr(text, 0, text_len) != NULL) {
    return -EINVAL;
  }
  rc = meshcore_runtime_sync_local_identity();
  if (rc != 0) {
    return rc;
  }
  timestamp = meshcore_clock_rtc_get_current_time_unique(&ctx->rtc_clock_state);
  meshcore_local_identity_calc_shared_secret(&ctx->mesh.self_id, secret,
                                              public_key);
  rc = cli_send(public_key, secret, type, attempt, timestamp, text, text_len,
                  0U, meshcore_runtime_local_path_hash_size_get());
  memset(secret, 0, sizeof(secret));
  if (rc != 0) {
    return rc;
  }
  rc = meshcore_runtime_timer_sync((uint32_t)meshcore_clock_millis_get());
  if (rc < 0) {
    meshcore_platform_bridge_request_error(MESHCORE_RUNTIME_REQUEST_CLI, rc);
  }
  return 0;
}

void meshcore_runtime_cli_receive(struct meshcore_packet *packet,
    const meshcore_common_peer_identity_t *sender, const uint8_t *secret,
    const uint8_t *data, size_t len)
{
  struct meshcore_runtime *ctx = meshcore_runtime_context_get();
  meshcore_common_node_identity_t local;
  meshcore_common_cli_event_t event = {0};
  char reply[MESHCORE_MAX_MESSAGE_TX_LEN + 1U] = {0};
  const uint8_t *end;
  size_t text_len;
  size_t capacity = MESHCORE_MAX_MESSAGE_TX_LEN;
  uint32_t delay_ms = 600U;
  uint32_t timestamp;
  uint8_t hash_size;
  int rc;

  if (packet == NULL || sender == NULL || secret == NULL || data == NULL ||
      len <= 5U || !meshcore_runtime_local_identity_get(&local)) {
    return;
  }
  event.type = (meshcore_common_cli_type_t)(data[4] >> 2);
  if (event.type != MESHCORE_COMMON_CLI_DATA &&
      event.type != MESHCORE_COMMON_CLI_COMMAND) {
    return;
  }
  switch (local.role) {
    case MESHCORE_COMMON_NODE_ROLE_CHAT:
      if (event.type == MESHCORE_COMMON_CLI_DATA) {
        capacity = 0U;
      }
      hash_size = meshcore_runtime_local_path_hash_size_get();
      break;
    case MESHCORE_COMMON_NODE_ROLE_REPEATER:
      hash_size = meshcore_packet_get_path_hash_size(packet);
      break;
    case MESHCORE_COMMON_NODE_ROLE_ROOM:
      delay_ms = 300U;
      hash_size = meshcore_packet_get_path_hash_size(packet);
      break;
    case MESHCORE_COMMON_NODE_ROLE_SENSOR:
      delay_ms = 1000U;
      hash_size = meshcore_packet_get_path_hash_size(packet);
      break;
    default:
      return;
  }
  end = memchr(&data[5], 0, len - 5U);
  text_len = end != NULL ? (size_t)(end - &data[5]) : len - 5U;
  if (text_len > MESHCORE_MAX_MESSAGE_TX_LEN) {
    return;
  }
  memcpy(event.public_key, sender->public_key, sizeof(event.public_key));
  memcpy(&event.sender_timestamp, data, sizeof(event.sender_timestamp));
  event.attempt = data[4] & 3U;
  event.route = meshcore_packet_is_route_flood(packet) ?
      MESHCORE_COMMON_MESSAGE_ROUTE_FLOOD : MESHCORE_COMMON_MESSAGE_ROUTE_DIRECT;
  event.rx_snr_q4 = packet->snr_q4;
  event.text_len = (uint16_t)text_len;
  memcpy(event.text, &data[5], text_len);
  rc = meshcore_platform_bridge_cli_receive(&event,
                                            capacity == 0U ? NULL : reply,
                                            capacity);
  if (rc == 0 || rc == -EACCES || rc == -ENOTSUP) {
    return;
  }
  if (rc > 0 && ((size_t)rc > capacity || memchr(reply, 0, (size_t)rc))) {
    rc = -EINVAL;
  }
  if (rc > 0) {
    timestamp = meshcore_clock_rtc_get_current_time_unique(&ctx->rtc_clock_state);
    if (timestamp == event.sender_timestamp) {
      timestamp++;
    }
    rc = cli_send(sender->public_key, secret, MESHCORE_COMMON_CLI_DATA, 0U,
                    timestamp, reply, (size_t)rc, delay_ms, hash_size);
  }
  if (rc < 0) {
    meshcore_platform_bridge_request_error(MESHCORE_RUNTIME_REQUEST_CLI, rc);
  }
}
