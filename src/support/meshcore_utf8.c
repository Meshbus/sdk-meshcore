// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2026 FoBE Studio
 */

#include "meshcore_utf8.h"

#include <stdbool.h>
#include <stdint.h>

static bool meshcore_utf8_is_continuation(uint8_t byte)
{
  return (byte & 0xC0U) == 0x80U;
}

size_t meshcore_utf8_valid_prefix_length(const char *text, size_t max_bytes)
{
  size_t offset = 0U;

  if (text == NULL) {
    return 0U;
  }

  while (text[offset] != '\0') {
    uint8_t first = (uint8_t)text[offset];
    size_t sequence_length;
    size_t i;

    if (first <= 0x7FU) {
      sequence_length = 1U;
    } else if (first >= 0xC2U && first <= 0xDFU) {
      sequence_length = 2U;
    } else if (first >= 0xE0U && first <= 0xEFU) {
      sequence_length = 3U;
    } else if (first >= 0xF0U && first <= 0xF4U) {
      sequence_length = 4U;
    } else {
      break;
    }

    if (offset + sequence_length > max_bytes) {
      break;
    }

    for (i = 1U; i < sequence_length; i++) {
      if (text[offset + i] == '\0' ||
          !meshcore_utf8_is_continuation((uint8_t)text[offset + i])) {
        return offset;
      }
    }

    if (sequence_length == 3U) {
      uint8_t second = (uint8_t)text[offset + 1U];

      if ((first == 0xE0U && second < 0xA0U) ||
          (first == 0xEDU && second > 0x9FU)) {
        break;
      }
    } else if (sequence_length == 4U) {
      uint8_t second = (uint8_t)text[offset + 1U];

      if ((first == 0xF0U && second < 0x90U) ||
          (first == 0xF4U && second > 0x8FU)) {
        break;
      }
    }

    offset += sequence_length;
  }

  return offset;
}
