/* FoBE Studio contributions: Apache-2.0; upstream-derived portions: MIT.
 * See LICENSING.md and LICENSES/MIT-MeshCore.txt. */
// SPDX-License-Identifier: Apache-2.0 AND MIT
/*
 * Copyright (c) 2026 FoBE Studio
 */

#ifndef MESHCORE_SUPPORT_UTF8_H_
#define MESHCORE_SUPPORT_UTF8_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Return the longest complete, valid UTF-8 prefix within max_bytes. */
size_t meshcore_utf8_valid_prefix_length(const char *text, size_t max_bytes);

#ifdef __cplusplus
}
#endif

#endif /* MESHCORE_SUPPORT_UTF8_H_ */
