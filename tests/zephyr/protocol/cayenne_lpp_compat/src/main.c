/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 FoBE Studio */

#include <errno.h>
#include <string.h>
#include <zephyr/ztest.h>

#include "meshcore_cayenne_lpp_compat.h"

#define BUFFER_CAP 64U
#define GPS_CHANNEL 7U
#define OTHER_CHANNEL 3U

struct scalar_vector {
	const char *name;
	int (*add)(struct cayenne_lpp_writer *writer, uint8_t channel, float value);
	float value;
	size_t size;
	uint8_t bytes[6];
};

struct gps_vector {
	const char *name;
	float latitude;
	float longitude;
	float altitude;
	uint8_t bytes[11];
};

struct location_vector {
	const char *name;
	size_t size;
	uint8_t bytes[BUFFER_CAP];
	bool present;
	int32_t latitude_e6;
	int32_t longitude_e6;
};

#include "vectors.h"

static void expect_buffer(const uint8_t *actual, const uint8_t *bytes, size_t size,
			  const char *name)
{
	uint8_t expected[BUFFER_CAP];

	zassert_true(size <= sizeof(expected), "%s invalid vector size", name);
	memset(expected, 0xa5, sizeof(expected));
	if (size != 0) {
		memcpy(expected, bytes, size);
	}
	/* Check both the encoded payload and untouched buffer tail. */
	zassert_mem_equal(expected, actual, sizeof(expected), "%s payload/tail mismatch", name);
}

ZTEST(meshcore_cayenne_lpp_compat_tdd, test_writer_encode_matches_vectors)
{
	struct cayenne_lpp_writer writer;
	uint8_t buffer[BUFFER_CAP];

	for (size_t i = 0; i < ARRAY_SIZE(scalar_vectors); ++i) {
		const struct scalar_vector *vector = &scalar_vectors[i];

		memset(buffer, 0xa5, sizeof(buffer));
		cayenne_lpp_init(&writer, buffer, sizeof(buffer));
		zassert_equal(0, vector->add(&writer, OTHER_CHANNEL, vector->value),
			      "%s encode failed", vector->name);
		zassert_equal(vector->size, cayenne_lpp_size(&writer),
			      "%s size mismatch", vector->name);
		expect_buffer(buffer, vector->bytes, vector->size, vector->name);

		memset(buffer, 0xa5, sizeof(buffer));
		cayenne_lpp_init(&writer, buffer, vector->size - 1U);
		zassert_equal(-ENOSPC, vector->add(&writer, OTHER_CHANNEL, vector->value),
			      "%s short buffer accepted", vector->name);
		zassert_equal(0, cayenne_lpp_size(&writer), "%s overflow advanced size",
			      vector->name);
		expect_buffer(buffer, NULL, 0, vector->name);
	}
	for (size_t i = 0; i < ARRAY_SIZE(gps_vectors); ++i) {
		const struct gps_vector *vector = &gps_vectors[i];

		memset(buffer, 0xa5, sizeof(buffer));
		cayenne_lpp_init(&writer, buffer, sizeof(buffer));
		zassert_equal(0, cayenne_lpp_add_gps(&writer, GPS_CHANNEL, vector->latitude,
						  vector->longitude, vector->altitude),
			      "%s encode failed", vector->name);
		zassert_equal(sizeof(vector->bytes), cayenne_lpp_size(&writer),
			      "%s size mismatch", vector->name);
		expect_buffer(buffer, vector->bytes, sizeof(vector->bytes), vector->name);

		memset(buffer, 0xa5, sizeof(buffer));
		cayenne_lpp_init(&writer, buffer, sizeof(vector->bytes) - 1U);
		zassert_equal(-ENOSPC,
			      cayenne_lpp_add_gps(&writer, GPS_CHANNEL, vector->latitude,
						 vector->longitude, vector->altitude),
			      "%s short buffer accepted", vector->name);
		zassert_equal(0, cayenne_lpp_size(&writer), "%s overflow advanced size",
			      vector->name);
		expect_buffer(buffer, NULL, 0, vector->name);
	}
}

ZTEST(meshcore_cayenne_lpp_compat_tdd, test_writer_size_overflow_and_invalid_args_match_expected)
{
	struct cayenne_lpp_writer writer;
	uint8_t buffer[BUFFER_CAP];

	memset(buffer, 0xa5, sizeof(buffer));
	cayenne_lpp_init(&writer, buffer, sizeof(combined_bytes));
	zassert_equal(0, cayenne_lpp_size(&writer), "initial size mismatch");
	zassert_equal(0, cayenne_lpp_size(NULL), "NULL size mismatch");
	zassert_equal(0, cayenne_lpp_add_voltage(&writer, OTHER_CHANNEL, 4.2f));
	zassert_equal(0, cayenne_lpp_add_gps(&writer, GPS_CHANNEL, 22.1f, 114.2f, 0.0f));
	zassert_equal(sizeof(combined_bytes), cayenne_lpp_size(&writer), "combined size mismatch");
	expect_buffer(buffer, combined_bytes, sizeof(combined_bytes), "combined fields");

	zassert_equal(-ENOSPC, cayenne_lpp_add_power(&writer, OTHER_CHANNEL, 11.0f));
	zassert_equal(sizeof(combined_bytes), cayenne_lpp_size(&writer), "overflow advanced size");
	expect_buffer(buffer, combined_bytes, sizeof(combined_bytes), "overflow");
	zassert_equal(-EINVAL, cayenne_lpp_add_voltage(NULL, OTHER_CHANNEL, 1.0f));
	writer.buf = NULL;
	zassert_equal(-EINVAL, cayenne_lpp_add_voltage(&writer, OTHER_CHANNEL, 1.0f));
}

ZTEST(meshcore_cayenne_lpp_compat_tdd, test_parse_location_matches_vectors)
{
	for (size_t i = 0; i < ARRAY_SIZE(location_vectors); ++i) {
		const struct location_vector *vector = &location_vectors[i];
		/* A failed parse must clear previous location data. */
		struct cayenne_lpp_location actual = { true, 1, true, 1 };

		cayenne_lpp_parse_location(vector->size ? vector->bytes : NULL,
					  vector->size, &actual);
		zassert_equal(vector->present, actual.has_latitude, "%s latitude flag",
			      vector->name);
		zassert_equal(vector->latitude_e6, actual.latitude_e6, "%s latitude", vector->name);
		zassert_equal(vector->present, actual.has_longitude, "%s longitude flag",
			      vector->name);
		zassert_equal(vector->longitude_e6, actual.longitude_e6, "%s longitude",
			      vector->name);
	}
}

ZTEST_SUITE(meshcore_cayenne_lpp_compat_tdd, NULL, NULL, NULL, NULL, NULL);
