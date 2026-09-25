<!-- SPDX-FileCopyrightText: 2026 FoBE Studio -->
<!-- SPDX-License-Identifier: Apache-2.0 -->
# Copied MeshCore test libraries

The 124 retained files under `tests/zephyr/common/lib/` are third-party
test dependencies. Their licensing remains independent of the MeshCore-owned
tests. Test-only use does not remove the notice requirements when these files
are redistributed with sdk-meshcore.

The scoped `tests/zephyr/common/lib/REUSE.toml` lists exact reviewed paths so
future imports do not inherit an unreviewed declaration. Original file bytes,
copyright notices and permission text are retained. The comparisons below
identify correspondence with specific upstream snapshots; they do not establish
an original import revision.

## Crypto: 107 files, MIT

The package records version 0.4.0 and Rhys Weatherley as its author, with
[`rweather/arduinolibs`](https://github.com/rweather/arduinolibs/tree/37a76b8f7516568e1c575b6dc9268da1ccaac6b6)
as the source. Its README explicitly covers the libraries and examples under
MIT. The 103 source/example files with copyright headers contain the same
complete MIT permission text, with Southern Storm Software, Pty Ltd. copyright
years retained individually (2015, 2016, 2018 and 2022 as applicable).

Comparison with that revision's `libraries/Crypto/` found 102 byte-identical
files. `Poly1305.cpp`, `RNG.cpp` and `RNG.h` differ but retain their complete
MIT headers. This is a retained package snapshot, not a claim that those files
match the comparison revision. `library.json` is a transformed package
descriptor that preserves the upstream author, source and version fields.
The package-generated `.piopm` record has no upstream counterpart.

`examples/TestRNG/TestRNG.ino` and `keywords.txt` match upstream exactly but
have no embedded attribution. Their author is recorded as Rhys Weatherley,
as identified by the upstream README/package metadata; the same attribution
covers `library.json`. `.piopm` contains only factual package identity,
version and registry fields, so its copyright field is `NONE`; MIT records
the package's license, without claiming ownership of third-party source.

The complete [MIT text](../LICENSES/MIT.txt) and original source notices
must accompany redistribution.

## Cayenne LPP fixed vectors

The Cayenne LPP test distributes fixed inputs and expected results, together
with a capture tool. The external reference implementation is not included. The
[test record](../tests/zephyr/protocol/cayenne_lpp_compat/README.md)
records attribution, the exact reference snapshot and source hashes,
recapture requirements, and the boundary between captured reference behavior
and explicit C adapter contract expectations. Crypto requires the
retained [MIT text](../LICENSES/MIT.txt).

## ed25519: 17 files, Zlib

All 17 files are byte-identical to the `lib/ed25519/` directory at the locked
[MeshCore reference b599bc511751de3681e8b9e1d7f7a31d5d0dad4b](https://github.com/meshcore-dev/MeshCore/tree/b599bc511751de3681e8b9e1d7f7a31d5d0dad4b/lib/ed25519).
Its `license.txt` matches the retained
[Orson Peters notice](../tests/zephyr/common/lib/ed25519/license.txt).

The original [orlp/ed25519 reference](https://github.com/orlp/ed25519/tree/b1f19fab4aebe607805620d25a5e42566ce46a0e)
states in its README that all code is under Zlib. Against that reference,
10 files match exactly. The inherited MeshCore version changes seven files:
it renames `ed25519.h` to `ed_25519.h`, adjusts includes, adds an upstream
reference comment, and declares/implements `ed25519_derive_pub` in the header
and `keypair.c`. These are modified upstream copies, not pristine or
original works of this project. This record marks those inherited alterations
without rewriting the fixtures.

`sha512.c` also retains its LibTomCrypt/Tom St Denis banner; that attribution
is recorded in addition to Orson Peters. Its bytes match the orlp reference,
whose all-code Zlib statement covers this distribution. Both the complete
[Zlib text](../LICENSES/Zlib.txt) and the component copyright/permission
notice are retained. Neither the source banner nor the original notice is
removed or replaced.

## Review boundary

These declarations cover only the listed fixture files. They do not establish
licensing for future imports, externally fetched reference trees, fonts, or
complete firmware distributions. Keep this record and applicable source
notices with source distributions; do not apply the repository's Apache-2.0
default to these third-party bodies.
