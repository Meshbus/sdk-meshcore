# MeshCore Testing Guide

`meshcore` has independent native and Zephyr test paths, plus downstream host
integration coverage. Native tests do not require Zephyr, west, or Twister.

Select checks for the affected contract and the requested acceptance criteria.
Use CTest names or labels from `tests/native/CMakeLists.txt` to focus local
validation. Documentation-only edits need content, reference, and command
checks; also run the sync report when architecture, evidence, or boundary
claims change. CI and release coverage do not require replaying the entire
matrix for every local edit.

Run commands from the MeshCore repository root. Replace `TASK` in scratch paths
with a task-specific name. Native tests use local fake hosts and scratch
outputs; within the authorized task, run them, fix in-scope failures, and rerun
affected checks without asking for approval at each step.

## Native CTest

Install the [build prerequisites](../README.md#prerequisites) first. Without
`.reference/meshcore`, CTest marks `meshcore_upstream_oracle` and
`meshcore_upstream_runtime_oracle` as **Skipped**, not executed comparisons.
CTest can still print "100% tests passed" when tests were skipped; inspect
its skipped-test list. Native, package and strict upstream results are
separate evidence. Direct non-strict oracle script calls still return zero
when skipping; `--require-reference` makes a missing reference a failure.


```sh
meshcore_build_dir=build.meshcore-TASK
cmake -S . -B "${meshcore_build_dir}" \
  -DMESHCORE_BUILD_TESTS=ON \
  -DMESHCORE_BUILD_EXAMPLES=ON
cmake --build "${meshcore_build_dir}"
ctest --test-dir "${meshcore_build_dir}" --output-on-failure
```

Native tests use `tests/support/fake_platform.c` to satisfy the public
`meshcore_platform_*` hook contract. The commands above run the full native
suite; for focused validation, build the affected targets and select their
CTest names with `-R` or labels with `-L`. Do not repeat a completed check unless
new changes or failures affect its result.

## Zephyr ztest/Twister

The [Zephyr test guide](../tests/zephyr/TESTING.md) owns the 17
protocol, runtime and boundary scenarios. Use its isolated west layout and
the locked reference from `upstream.lock`; the suite's integration platform is
`native_sim` on Linux, with `qemu_x86` retained as an allowed platform. The
owner-local CI lane prepares exact Zephyr, Mbed TLS, TF-PSA-Crypto and tool
image revisions, checks the reference and both API maps, then runs all scenarios.
The result checker rejects missing or skipped scenarios and cases.

This suite compiles sources from the same MeshCore checkout as the tests. It
uses test-local platform hooks without a production Zephyr adapter. Native CTest,
compiled upstream comparisons and downstream host tests remain separate
evidence; report their results separately.

## Documentation Checks

```sh
python3 tools/check_docs.py --repo-root .
```

This checks local Markdown targets/anchors and shell syntax without network
access or executing examples. For public-header documentation and Doxygen
changes, also [generate the API reference](api.md#generate-locally). The CI
documentation job runs both checks and retains the generated HTML as an
artifact. Building example/install commands remains necessary when those
commands change.

## Package Smoke Test

For package/install changes or release validation, the native suite's
`meshcore_package_smoke` test installs to an isolated prefix, builds and runs
`examples/minimal_host` through `find_package(meshcore CONFIG REQUIRED)`, and
checks exact numeric-baseline acceptance/rejection and the installed full
version label, development/release kind, upstream tag and commit. It also
verifies that the installed
`LICENSE`, `LICENSING.md`, provenance records and `LICENSES/` match the source
files, including the upstream MIT and full Monocypher BSD attribution and terms. It can
also run independently:

```sh
python3 tools/package_smoke.py --source-root . \
  --work-dir build.meshcore-package-TASK
```

The helper recreates its work directory, so use a task-owned scratch path.
A successful full native run already includes this check.

## API And Evidence Inventories

For public API/type changes or evidence-mapping changes, use the corresponding
static report. Both are also registered in the native suite:

```sh
python3 tools/api_surface_report.py --repo-root . \
  --require-runtime-coverage \
  --require-type-coverage \
  --require-fake-platform-hooks
python3 tools/parity_coverage_report.py --repo-root . --require-full-coverage
```

These reports check symbols, coverage markers, and evidence references. They
do not prove that tests executed or that every behavioral case is covered.

## Sync And Boundary Checks

```sh
python3 tools/meshcore_sync_report.py --repo-root .
```

The sync report checks upstream evidence, source manifest coverage, public
header ownership, stale source roots, test-hook boundaries, platform hook
boundaries, and example public-API usage. It includes the upstream lock check.
If `.reference/meshcore` is not checked out, upstream evidence checks are
reported as warnings while the repository-local boundary checks still run.

### Strict Upstream Validation

Upstream sync, strict compatibility validation, and release acceptance require
the ignored reference checkout prepared as described in
[UPSTREAM.md](../UPSTREAM.md#locked-reference). Validate it before relying on
upstream behavior, then run the relevant parity/oracle tests for the changed
surface. Release validation includes the full parity suite and compiled
upstream oracle. After building the native suite, the strict checks are:

```sh
python3 tools/upstream_lock_check.py --repo-root .
ctest --test-dir "${meshcore_build_dir}" -L parity \
  -E '^meshcore_upstream.*oracle$' --output-on-failure
python3 tools/upstream_oracle.py --repo-root . \
  --meshcore-lib "${meshcore_build_dir}/libmeshcore.a" \
  --work-dir "${meshcore_build_dir}/strict-upstream-oracle" \
  --require-reference
python3 tools/upstream_runtime_oracle.py --repo-root . \
  --runtime-test "${meshcore_build_dir}/tests/native/meshcore_native_runtime_cli" \
  --work-dir "${meshcore_build_dir}/strict-upstream-runtime-oracle" \
  --require-reference
```

Set `meshcore_build_dir` to the task's native build directory; adjust the
library artifact path for the generator/platform. This example excludes the
native oracle tests and runs each once with `--require-reference` so missing
evidence cannot silently skip them. Reuse an earlier oracle result only if it
actually executed and passed for the same build and reference. The standalone
lock check also fails on a missing reference; a prior successful sync report
lock result can be reused, but a missing-reference warning cannot satisfy
strict acceptance.

The runtime oracle compiles unchanged locked CLI/policy source excerpts with
small allocation/clock/RNG/authorized-command doubles. It compares send and
reply plaintext, encoded route choice and scheduling against native C cases.
It does not run the entire upstream firmware or validate host ACL/storage.
The native CLI suite additionally exercises the public radio ingress, bounded
callbacks and malformed input. Keep these evidence levels distinct.

## Downstream Coverage

Keep platform, board, transport, and product-service integration tests outside
the generic library. Run the relevant host-adapter tests when a change affects
a concrete host contract or integration configuration, or when the user names
downstream acceptance criteria. A public-header edit or internal library
change alone does not require a product build.

Use the host repository's test metadata, platform selection, and task-specific
output directories. Product builds are relevant when product composition is
affected. For a library-only task, report unverified downstream coverage
separately; if downstream acceptance is explicitly required, keep that
criterion incomplete until verified. Continue independent library work while
any required host scope or authorization is pending.
