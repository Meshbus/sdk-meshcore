// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2026 FoBE Studio
 */

#include "native_test.h"

#include <string.h>

#include "meshcore_advert_data.h"
#include "meshcore_mesh.h"
#include "meshcore_packet.h"
#include "meshcore_packet_manager.h"
#include "meshcore_tables.h"
#include "meshcore_utf8.h"

/*
 * Layer 1 parity evidence:
 * - .reference/meshcore/src/Packet.h
 * - .reference/meshcore/src/Packet.cpp
 * - .reference/meshcore/src/helpers/AdvertDataHelpers.h
 * - .reference/meshcore/src/helpers/AdvertDataHelpers.cpp
 * - .reference/meshcore/src/helpers/UTF8Helpers.h
 */

static int test_public_payload_constants_match_packet_core(void)
{
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_REQ,
                        MESHCORE_PACKET_PAYLOAD_TYPE_REQ);
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_RESPONSE,
                        MESHCORE_PACKET_PAYLOAD_TYPE_RESPONSE);
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_TXT_MSG,
                        MESHCORE_PACKET_PAYLOAD_TYPE_TXT_MSG);
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_ACK,
                        MESHCORE_PACKET_PAYLOAD_TYPE_ACK);
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_ADVERT,
                        MESHCORE_PACKET_PAYLOAD_TYPE_ADVERT);
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_GRP_TXT,
                        MESHCORE_PACKET_PAYLOAD_TYPE_GRP_TXT);
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_GRP_DATA,
                        MESHCORE_PACKET_PAYLOAD_TYPE_GRP_DATA);
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_ANON_REQ,
                        MESHCORE_PACKET_PAYLOAD_TYPE_ANON_REQ);
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_PATH,
                        MESHCORE_PACKET_PAYLOAD_TYPE_PATH);
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_TRACE,
                        MESHCORE_PACKET_PAYLOAD_TYPE_TRACE);
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_MULTIPART,
                        MESHCORE_PACKET_PAYLOAD_TYPE_MULTIPART);
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_CONTROL,
                        MESHCORE_PACKET_PAYLOAD_TYPE_CONTROL);
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_RAW_CUSTOM,
                        MESHCORE_PACKET_PAYLOAD_TYPE_RAW_CUSTOM);
  NATIVE_TEST_ASSERT_EQ(PATH_EXTRA_TYPE_SNR,
                        MESHCORE_PACKET_PATH_EXTRA_TYPE_SNR);
  NATIVE_TEST_ASSERT_EQ(0xFFU, MESHCORE_OUT_PATH_UNKNOWN);

  return 0;
}

static int test_packet_path_length_boundaries_match_upstream_encoding(void)
{
  struct meshcore_packet packet;
  uint8_t raw[] = {
    (uint8_t)((PAYLOAD_TYPE_RAW_CUSTOM << PH_TYPE_SHIFT) | ROUTE_TYPE_DIRECT),
    0xC0U,
    0x01U,
  };

  NATIVE_TEST_ASSERT(meshcore_packet_is_valid_path_len(0U));
  NATIVE_TEST_ASSERT(meshcore_packet_is_valid_path_len(63U));
  NATIVE_TEST_ASSERT(meshcore_packet_is_valid_path_len((1U << 6) | 32U));
  NATIVE_TEST_ASSERT(meshcore_packet_is_valid_path_len((2U << 6) | 21U));

  NATIVE_TEST_ASSERT(!meshcore_packet_is_valid_path_len((1U << 6) | 33U));
  NATIVE_TEST_ASSERT(!meshcore_packet_is_valid_path_len((2U << 6) | 22U));
  NATIVE_TEST_ASSERT(!meshcore_packet_is_valid_path_len((3U << 6) | 0U));

  meshcore_packet_init(&packet);
  packet.header = (uint8_t)((PAYLOAD_TYPE_RAW_CUSTOM << PH_TYPE_SHIFT) |
                            ROUTE_TYPE_DIRECT);
  packet.path_len = (1U << 6) | 33U;
  packet.payload[0] = 0xAAU;
  packet.payload_len = 1U;
  NATIVE_TEST_ASSERT_EQ(0U, meshcore_packet_write_to(&packet, raw));

  meshcore_packet_init(&packet);
  NATIVE_TEST_ASSERT(!meshcore_packet_read_from(&packet, raw, sizeof(raw)));

  return 0;
}

static int test_packet_transport_code_serialization_is_little_endian(void)
{
  struct meshcore_packet source;
  struct meshcore_packet parsed;
  uint8_t raw[MESHCORE_MAX_TRANS_UNIT_LEN];
  uint8_t len;
  const uint8_t expected[] = {
    (uint8_t)((PAYLOAD_TYPE_TXT_MSG << PH_TYPE_SHIFT) |
              ROUTE_TYPE_TRANSPORT_DIRECT),
    0x34U,
    0x12U,
    0xCDU,
    0xABU,
    0x00U,
    0x99U,
  };

  meshcore_packet_init(&source);
  source.header = expected[0];
  source.transport_codes[0] = 0x1234U;
  source.transport_codes[1] = 0xABCDU;
  source.payload[0] = 0x99U;
  source.payload_len = 1U;

  len = meshcore_packet_write_to(&source, raw);
  NATIVE_TEST_ASSERT_EQ(sizeof(expected), len);
  NATIVE_TEST_ASSERT(memcmp(raw, expected, sizeof(expected)) == 0);
  NATIVE_TEST_ASSERT(meshcore_packet_read_from(&parsed, raw, len));
  NATIVE_TEST_ASSERT_EQ(ROUTE_TYPE_TRANSPORT_DIRECT,
                        meshcore_packet_get_route_type(&parsed));
  NATIVE_TEST_ASSERT_EQ(0x1234U, parsed.transport_codes[0]);
  NATIVE_TEST_ASSERT_EQ(0xABCDU, parsed.transport_codes[1]);
  NATIVE_TEST_ASSERT_EQ(PAYLOAD_TYPE_TXT_MSG,
                        meshcore_packet_get_payload_type(&parsed));

  return 0;
}

static int test_transport_flood_hash_width_golden_vectors(void)
{
  const uint16_t transport_codes[][2] = {
    {0x1234U, 0xABCDU},
    {MESHCORE_SNR_TRANSPORT_CODE0, MESHCORE_SNR_TRANSPORT_CODE1},
  };
  size_t transport_idx;
  uint8_t hash_size;

  for (transport_idx = 0U;
       transport_idx < sizeof(transport_codes) / sizeof(transport_codes[0]);
       transport_idx++) {
    for (hash_size = 1U; hash_size <= 3U; hash_size++) {
      struct meshcore_packet_queue_manager manager;
      struct meshcore_tables tables;
      struct meshcore_mesh mesh;
      struct meshcore_packet *packet;
      struct meshcore_packet *queued;
      uint8_t raw[MESHCORE_MAX_TRANS_UNIT_LEN];
      uint8_t expected[7U];
      uint8_t raw_len;

      meshcore_packet_queue_manager_prepare(&manager, 2);
      NATIVE_TEST_ASSERT(manager.initialized);
      meshcore_tables_init(&tables);
      meshcore_mesh_init(&mesh, &manager, &tables);

      packet = meshcore_dispatcher_obtain_new_packet(&mesh.dispatcher);
      NATIVE_TEST_ASSERT(packet != NULL);
      packet->header = (uint8_t)(PAYLOAD_TYPE_TXT_MSG << PH_TYPE_SHIFT);
      packet->payload[0] = 0xA5U;
      packet->payload_len = 1U;

      NATIVE_TEST_ASSERT_EQ(
          0, meshcore_mesh_send_flood_by_transport_codes(
                 &mesh, packet, transport_codes[transport_idx], 0U,
                 hash_size));
      queued = meshcore_packet_queue_manager_get_outbound_by_idx(&manager, 0);
      NATIVE_TEST_ASSERT(queued != NULL);
      NATIVE_TEST_ASSERT_EQ(hash_size,
                            meshcore_packet_get_path_hash_size(queued));

      expected[0] = (uint8_t)((PAYLOAD_TYPE_TXT_MSG << PH_TYPE_SHIFT) |
                              ROUTE_TYPE_TRANSPORT_FLOOD);
      expected[1] = (uint8_t)(transport_codes[transport_idx][0] & 0xFFU);
      expected[2] =
          (uint8_t)((transport_codes[transport_idx][0] >> 8) & 0xFFU);
      expected[3] = (uint8_t)(transport_codes[transport_idx][1] & 0xFFU);
      expected[4] =
          (uint8_t)((transport_codes[transport_idx][1] >> 8) & 0xFFU);
      expected[5] = (uint8_t)((hash_size - 1U) << 6);
      expected[6] = 0xA5U;

      raw_len = meshcore_packet_write_to(queued, raw);
      NATIVE_TEST_ASSERT_EQ(sizeof(expected), raw_len);
      NATIVE_TEST_ASSERT(memcmp(raw, expected, sizeof(expected)) == 0);
      meshcore_packet_queue_manager_deinit(&manager);
    }
  }

  return 0;
}

static int test_advert_data_golden_vector_and_parser_boundaries(void)
{
  struct meshcore_advert_data_builder builder;
  struct meshcore_advert_data_parser parser;
  uint8_t encoded[MESHCORE_MAX_ADVERT_DATA_LEN];
  const uint8_t expected[] = {
    0xF1U,
    0x87U, 0xD6U, 0x12U, 0x00U,
    0x60U, 0xDAU, 0xD9U, 0xFFU,
    0x34U, 0x12U,
    0xCDU, 0xABU,
    'n', 'o', 'd', 'e',
  };
  const uint8_t truncated_lat_lon[] = {0x11U, 0x00U, 0x00U, 0x00U};
  uint8_t too_long_name[MESHCORE_MAX_ADVERT_DATA_LEN + 1U];
  uint8_t len;

  meshcore_advert_data_builder_init_with_name_lat_lon(
      &builder, ADV_TYPE_CHAT, "node", 1.234567, -2.5);
  meshcore_advert_data_builder_set_feat1(&builder, 0x1234U);
  meshcore_advert_data_builder_set_feat2(&builder, 0xABCDU);
  len = meshcore_advert_data_builder_encode_to(&builder, encoded);

  NATIVE_TEST_ASSERT_EQ(sizeof(expected), len);
  NATIVE_TEST_ASSERT(memcmp(encoded, expected, sizeof(expected)) == 0);

  meshcore_advert_data_parser_init(&parser, encoded, len);
  NATIVE_TEST_ASSERT(meshcore_advert_data_parser_is_valid(&parser));
  NATIVE_TEST_ASSERT_EQ(ADV_TYPE_CHAT,
                        meshcore_advert_data_parser_get_type(&parser));
  NATIVE_TEST_ASSERT(meshcore_advert_data_parser_has_lat_lon(&parser));
  NATIVE_TEST_ASSERT_EQ(1234567, meshcore_advert_data_parser_get_int_lat(&parser));
  NATIVE_TEST_ASSERT_EQ((uint32_t)-2500000,
                        (uint32_t)meshcore_advert_data_parser_get_int_lon(&parser));
  NATIVE_TEST_ASSERT_EQ(0x1234U,
                        meshcore_advert_data_parser_get_feat1(&parser));
  NATIVE_TEST_ASSERT_EQ(0xABCDU,
                        meshcore_advert_data_parser_get_feat2(&parser));
  NATIVE_TEST_ASSERT(meshcore_advert_data_parser_has_name(&parser));
  NATIVE_TEST_ASSERT(strcmp(meshcore_advert_data_parser_get_name(&parser),
                            "node") == 0);

  meshcore_advert_data_parser_init(&parser, truncated_lat_lon,
                                   sizeof(truncated_lat_lon));
  NATIVE_TEST_ASSERT(!meshcore_advert_data_parser_is_valid(&parser));

  memset(too_long_name, 'a', sizeof(too_long_name));
  too_long_name[0] = ADV_TYPE_CHAT | ADV_NAME_MASK;
  meshcore_advert_data_parser_init(&parser, too_long_name,
                                   (uint8_t)sizeof(too_long_name));
  NATIVE_TEST_ASSERT(!meshcore_advert_data_parser_is_valid(&parser));

  return 0;
}

static int test_utf8_advert_name_prefix_matches_upstream(void)
{
  static const char utf8_name[] =
      "Example RPT "
      "\xF0\x9F\x94\x8B"
      "\xF0\x9F\x87\xB5\xF0\x9F\x87\xB1";
  static const char expected_20[] =
      "Example RPT "
      "\xF0\x9F\x94\x8B"
      "\xF0\x9F\x87\xB5";
  static const char invalid_lead[] = {'A', (char)0xC0, (char)0x80, '\0'};
  static const char overlong_three[] = {
      'A', (char)0xE0, (char)0x80, (char)0x80, '\0'};
  static const char surrogate[] = {
      'A', (char)0xED, (char)0xA0, (char)0x80, '\0'};
  static const char out_of_range[] = {
      'A', (char)0xF4, (char)0x90, (char)0x80, (char)0x80, '\0'};
  static const char incomplete[] = {
      'A', (char)0xF0, (char)0x9F, (char)0x94, '\0'};
  struct meshcore_advert_data_builder builder;
  uint8_t encoded[MESHCORE_MAX_ADVERT_DATA_LEN];
  uint8_t len;

  NATIVE_TEST_ASSERT_EQ(24U,
                        meshcore_utf8_valid_prefix_length(utf8_name, 24U));
  NATIVE_TEST_ASSERT_EQ(20U,
                        meshcore_utf8_valid_prefix_length(utf8_name, 23U));
  NATIVE_TEST_ASSERT_EQ(1U,
                        meshcore_utf8_valid_prefix_length(invalid_lead, 8U));
  NATIVE_TEST_ASSERT_EQ(
      1U, meshcore_utf8_valid_prefix_length(overlong_three, 8U));
  NATIVE_TEST_ASSERT_EQ(1U,
                        meshcore_utf8_valid_prefix_length(surrogate, 8U));
  NATIVE_TEST_ASSERT_EQ(
      1U, meshcore_utf8_valid_prefix_length(out_of_range, 8U));
  NATIVE_TEST_ASSERT_EQ(1U,
                        meshcore_utf8_valid_prefix_length(incomplete, 8U));

  meshcore_advert_data_builder_init_with_name_lat_lon(
      &builder, ADV_TYPE_CHAT, utf8_name, 1.0, 2.0);
  len = meshcore_advert_data_builder_encode_to(&builder, encoded);
  NATIVE_TEST_ASSERT_EQ(9U + sizeof(expected_20) - 1U, len);
  NATIVE_TEST_ASSERT((encoded[0] & ADV_NAME_MASK) != 0U);
  NATIVE_TEST_ASSERT(memcmp(&encoded[9], expected_20,
                            sizeof(expected_20) - 1U) == 0);

  meshcore_advert_data_builder_init_with_name(&builder, ADV_TYPE_CHAT,
                                              &invalid_lead[1]);
  len = meshcore_advert_data_builder_encode_to(&builder, encoded);
  NATIVE_TEST_ASSERT_EQ(1U, len);
  NATIVE_TEST_ASSERT((encoded[0] & ADV_NAME_MASK) == 0U);

  return 0;
}

int main(void)
{
  NATIVE_TEST_ASSERT_EQ(0, test_public_payload_constants_match_packet_core());
  NATIVE_TEST_ASSERT_EQ(0, test_packet_path_length_boundaries_match_upstream_encoding());
  NATIVE_TEST_ASSERT_EQ(0, test_packet_transport_code_serialization_is_little_endian());
  NATIVE_TEST_ASSERT_EQ(0, test_transport_flood_hash_width_golden_vectors());
  NATIVE_TEST_ASSERT_EQ(0, test_advert_data_golden_vector_and_parser_boundaries());
  NATIVE_TEST_ASSERT_EQ(0, test_utf8_advert_name_prefix_matches_upstream());

  return 0;
}
