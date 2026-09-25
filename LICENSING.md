# Licensing

FoBE Studio-owned contributions are Copyright (c) 2026 FoBE Studio and use
[Apache-2.0](LICENSE). This default does not relicense upstream-derived code
or bundled third-party sources.

| Portion | Applicable terms | Evidence |
| --- | --- | --- |
| Project-owned build integration, tools, tests, examples and documentation | Apache-2.0 | [Full text](LICENSES/Apache-2.0.txt) |
| C protocol/runtime adaptation and public interfaces under `src/` and `include/`, excluding bundled Monocypher | Apache-2.0 for FoBE Studio contributions; MIT for upstream-derived portions | [Upstream MIT notice](LICENSES/MIT-MeshCore.txt), [source mapping](UPSTREAM.md) |
| Monocypher 4.0.2 in `src/support/crypto/monocypher*` | BSD-2-Clause OR CC0-1.0, as retained in each original source header | [BSD attribution and terms](LICENSES/BSD-2-Clause-Monocypher.txt) |
| Copied Crypto test inputs in `tests/zephyr/common/lib/Crypto/` | MIT, preserving original source headers and package attribution | [Provenance](docs/test-fixtures.md), [MIT text](LICENSES/MIT.txt) |
| Copied ed25519 test inputs in `tests/zephyr/common/lib/ed25519/` | Zlib, preserving Orson Peters and Tom St Denis attribution | [Original notice](tests/zephyr/common/lib/ed25519/license.txt), [provenance](docs/test-fixtures.md), [Zlib text](LICENSES/Zlib.txt) |

## Upstream MeshCore

The C implementation uses upstream MeshCore revision
`b599bc511751de3681e8b9e1d7f7a31d5d0dad4b` as evidence. Derived or translated
portions retain Copyright (c) 2025 Scott Powell / rippleradios.com and the full
MIT grant. `Apache-2.0 AND MIT` in adaptation files records both obligations;
it does not offer a choice of license for all of a file's contents.
[UPSTREAM.md](UPSTREAM.md) and `upstream.lock` retain the source mapping and
locked reference. The ignored reference checkout is not part of this package.

## Source and binary distributions

Preserve original source headers, this guide, the root license and the
applicable files in `LICENSES/`. Binary distributions select the BSD-2-Clause
option for Monocypher and must carry its attribution, conditions and disclaimer.
The original Monocypher source files retain their dual-license choice. The
copied test inputs are distributed only as source fixtures for the Zephyr
suite; their original notices and the scoped `REUSE.toml` travel with them.

CMake installs `LICENSE`, `LICENSING.md`, `UPSTREAM.md`, `upstream.lock` and
`LICENSES/` under the configured documentation directory. Ship these materials
with installed libraries and downstream binary packages. Hosts, toolchains and
other dependencies retain their own terms.
