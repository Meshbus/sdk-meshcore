<!-- SPDX-FileCopyrightText: 2026 FoBE Studio -->
<!-- SPDX-License-Identifier: Apache-2.0 -->
# Cayenne LPP fixed compatibility vectors

This test compares the MeshCore C telemetry implementation with fixed
bytes and location results captured independently from a pinned third-party
CayenneLpp reference. Ordinary builds compile only `src/main.c` and the module's
`MESHCORE_SUPPORT_CAYENNE_LPP_SOURCES`; they do not fetch, compile or link the
C++ reference or `tools/capture_reference.cpp`.

`src/vectors.h` is generated evidence. Do not regenerate expected results from
the implementation under test or edit the bytes to make a failing test pass.

## Origin and capture

The capture's source record identifies the Meshbus SDK repository at
`afd751f2d9d67fef1e3d8cab04ed0d66e46ab741`, directory
`tests/lib/meshcore/common/lib/CayenneLpp/`. This is the exact retained reference
snapshot; its original upstream import revision is not established. This
provenance entry identifies the captured input, not a dependency of ordinary
test builds or a publicly verified download source.

The reference identifies The Things Network (2017) and Manuel
Weichselbaumer (2021) under MIT, and linked the original
`https://developer.mbed.org/teams/myDevicesIoT/code/Cayenne-LPP/` project.
Its implementation is not distributed in this source tree.
The fixture capture tool contains only calls into that independently obtained
reference; the checked-in vectors record numeric protocol inputs and outputs.

SHA-256 of each reference input:

| File | SHA-256 |
| --- | --- |
| `CayenneLPP.cpp` | `60bae743ffc984990b4515d1fb25621183e7832e013005110b3e5c02c9377754` |
| `CayenneLPP.h` | `1eda64780873da0411b8850891177a03e9bc4423504fc3c446ef052606234257` |
| `CayenneLPPMessage.h` | `17e56d2248468612bec2599801933e2b7a43ede1fdddc9cc47c851bdb4b23d94` |
| `CayenneLPPPolyline.cpp` | `c99f1dd0bd61691e27e7cde20828cd881798d81e29f9d52840c4be731a4b902a` |
| `CayenneLPPPolyline.h` | `9ce4d4189cf68df9f724ef9a9672c0772691d90de681f0eee3102ce0dec23b77` |

The checked-in capture used Apple Clang 21.0.0, C++17, without fast-math or
`NDEBUG`. The full reference Polyline implementation supplies link symbols;
none of the captured reference operations encodes or decodes a polyline.
Capture does not compile or link the MeshCore C implementation.

The generated `src/vectors.h` SHA-256 is
`37a8b4f57f11ba8ac81efc3e5e6bdfbfac7aedad2f364f9c462bdeee3b5bb233`.
GCC 13.3.0 in the common Linux builder reproduced the same bytes and hash.

## Coverage and expectation sources

- Ten scalar encoding inputs cover zero/negative analog values, pressure,
  distance, humidity, negative temperature, voltage, current, power and negative
  altitude. Two GPS inputs cover positive and negative coordinates/altitudes.
  Encoded bytes are captured from the reference. Each input is also tested with
  insufficient capacity, with no length advance or buffer mutation permitted.
- A 15-byte voltage-plus-GPS frame checks exact capacity, rejected subsequent
  power insertion, preserved bytes, and null writer/buffer handling. The capture
  verifies the reference's overflow result and unchanged payload. C API error
  codes (`-ENOSPC`, `-EINVAL`) remain explicit adapter-contract assertions.
- Eleven location inputs cover empty, non-GPS, single GPS,
  mixed fields, repeated GPS (last wins), valid prefix, unknown type after/before
  GPS, truncated field after GPS, known skipped fields, and malformed polyline.
  The first nine outputs come from the reference decoder. Coordinate conversion
  uses float-to-E4 rounding followed by E6 scaling.
- For known skipped fields, the expected location comes from reference decoding
  of the valid GPS suffix. The full mixed frame is not sent to the reference.
  Malformed-polyline input has an explicitly absent-location expectation from
  the existing C adapter contract. These last two cases do not claim upstream
  polyline compatibility. Output is prefilled before every parse to verify that
  stale location values are cleared.

The vectors preserve the selected baseline behavior, not continuous comparison
against all future upstream changes. Review and recapture deliberately when
the compatibility baseline changes. No hardware behavior is established here.

## Reproduce the capture

Recapture requires an independently obtained reference directory containing
the five files with the exact hashes above. That implementation is not included
in this repository, and no independently verified public source for the exact
snapshot is documented here. This checkout alone is therefore insufficient to
recapture the vectors. Ordinary test execution uses the checked-in vectors and
does not need the reference.

After obtaining and verifying those files, run in Bash from this repository's
root with a C++17 compiler. Set `reference_source` to their directory. The
command writes outputs to a fresh temporary directory and compares them with
the checked-in vector file; it does not replace that file.

```sh
set -euo pipefail
reference_source=/absolute/path/to/verified-cayenne-lpp-reference
reference_dir="$(mktemp -d)"
test_dir="$PWD/tests/zephyr/protocol/cayenne_lpp_compat"
# Verify the five source hashes against the table above before compiling.
c++ -std=c++17 -include cstdint -include cstddef -I "$reference_source" \
  "$test_dir/tools/capture_reference.cpp" \
  "$reference_source/CayenneLPP.cpp" "$reference_source/CayenneLPPPolyline.cpp" \
  -o "$reference_dir/capture"
"$reference_dir/capture" > "$reference_dir/vectors.h"
cmp "$reference_dir/vectors.h" "$test_dir/src/vectors.h"
```

## Run the test

From the isolated west workspace described in `tests/zephyr/TESTING.md`:

```sh
west twister -T meshcore/tests/zephyr/protocol/cayenne_lpp_compat \
  -p native_sim -O twister-cayenne --inline-logs -j 2
```

The scenario ID is `lib.meshcore.cayenne_lpp_compat.tdd`. The three ztest
cases exercise encoding, writer boundaries and location parsing.
Record simulator results for the tested checkout separately from
physical-device qualification or hosted CI results.
