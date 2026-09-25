// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2026 FoBE Studio
 */

#ifndef MESHCORE_TESTS_SUPPORT_FAKE_PLATFORM_H_
#define MESHCORE_TESTS_SUPPORT_FAKE_PLATFORM_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "meshcore/types.h"

void meshcore_native_platform_reset(void);
void meshcore_native_platform_role_set(meshcore_common_node_role_t role);
void meshcore_native_platform_policy_set(
    const meshcore_common_node_runtime_policy_t *policy);
void meshcore_native_platform_cli_result_set(int result, const char *reply);
unsigned int meshcore_native_platform_cli_count_get(void);
size_t meshcore_native_platform_cli_capacity_get(void);
const meshcore_common_cli_event_t *meshcore_native_platform_cli_event_get(void);
void meshcore_native_platform_identity_get_fail_set(bool fail);
void meshcore_native_platform_timer_arm_fail_set(bool fail);
void meshcore_native_platform_radio_send_fail_set(bool fail);
void meshcore_native_platform_radio_receiving_set(bool receiving);
void meshcore_native_platform_event_fail_set(bool fail);
void meshcore_native_platform_sha256_two_fragments_zero_set(bool zero);
void meshcore_native_platform_peer_path_set(bool exists,
                                            bool has_out_path,
                                            const uint8_t *out_path,
                                            uint8_t out_path_byte_len,
                                            uint8_t path_hash_size);
void meshcore_native_platform_peer_secret_set(bool exists,
                                              const uint8_t *secret,
                                              const uint8_t *public_key);
void meshcore_native_platform_peer_path_error_set(int err_code);
void meshcore_native_platform_channel_secret_match_set(bool exists);
void meshcore_native_platform_node_name_set(const char *name);
void meshcore_native_platform_time_set(unsigned long now_ms, uint32_t now_s);
unsigned int meshcore_native_platform_timer_arm_count_get(void);
unsigned int meshcore_native_platform_timer_cancel_count_get(void);
uint32_t meshcore_native_platform_last_timer_deadline_get(void);
unsigned int meshcore_native_platform_radio_begin_count_get(void);
unsigned int meshcore_native_platform_radio_send_count_get(void);
size_t meshcore_native_platform_last_radio_send_len_get(void);
const uint8_t *meshcore_native_platform_last_radio_send_get(void);
unsigned int meshcore_native_platform_message_count_get(void);
const meshcore_common_message_t *meshcore_native_platform_last_message_get(void);
unsigned int meshcore_native_platform_message_ack_count_get(void);
unsigned int meshcore_native_platform_advert_count_get(void);
const meshcore_common_advert_event_t *meshcore_native_platform_last_advert_get(void);
unsigned int meshcore_native_platform_binary_request_count_get(void);
const meshcore_common_binary_request_event_t *
meshcore_native_platform_last_binary_request_get(void);
unsigned int meshcore_native_platform_binary_response_count_get(void);
uint32_t meshcore_native_platform_last_binary_response_tag_get(void);
size_t meshcore_native_platform_last_binary_response_payload_len_get(void);
const uint8_t *meshcore_native_platform_last_binary_response_payload_get(void);
unsigned int meshcore_native_platform_raw_data_count_get(void);
const meshcore_common_raw_data_event_t *
meshcore_native_platform_last_raw_data_get(void);
unsigned int meshcore_native_platform_control_data_count_get(void);
const meshcore_common_control_data_event_t *
meshcore_native_platform_last_control_data_get(void);
unsigned int meshcore_native_platform_telemetry_count_get(void);
uint32_t meshcore_native_platform_last_telemetry_tag_get(void);
size_t meshcore_native_platform_last_telemetry_payload_len_get(void);
const uint8_t *meshcore_native_platform_last_telemetry_payload_get(void);
unsigned int meshcore_native_platform_peer_path_publish_count_get(void);
const meshcore_common_peer_path_event_t *
meshcore_native_platform_last_peer_path_publish_get(void);
unsigned int meshcore_native_platform_trace_path_count_get(void);
uint32_t meshcore_native_platform_last_trace_path_tag_get(void);
uint8_t meshcore_native_platform_last_trace_path_state_get(void);
uint8_t meshcore_native_platform_last_trace_path_out_count_get(void);
uint8_t meshcore_native_platform_last_trace_path_return_count_get(void);
unsigned int meshcore_native_platform_node_discover_count_get(void);
const meshcore_common_node_discover_event_t *
meshcore_native_platform_last_node_discover_get(void);
unsigned int meshcore_native_platform_channel_data_count_get(void);
const meshcore_common_channel_data_event_t *
meshcore_native_platform_last_channel_data_get(void);
unsigned int meshcore_native_platform_request_error_count_get(void);
int meshcore_native_platform_last_request_error_get(void);

#endif /* MESHCORE_TESTS_SUPPORT_FAKE_PLATFORM_H_ */
