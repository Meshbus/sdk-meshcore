# Contributing

## Scope

Contributions should improve the platform-neutral C library, its public host
contract, upstream compatibility evidence, tests or documentation. Radio
drivers, RTOS services, persistence implementations, UI and companion transport
adapters belong in host repositories. Read the relevant section of
[ARCHITECTURE.md](ARCHITECTURE.md) when a change crosses a responsibility boundary.

Discuss substantial API or ownership changes in an issue before implementing
them. Include the use case, proposed host contract, compatibility impact and
how behavior will be verified. Small focused fixes can go directly to a PR.

Original contributions are accepted under Apache-2.0. Preserve upstream MIT
and bundled third-party terms; see [LICENSING.md](LICENSING.md).

## Development Loop

1. Start from a source checkout with the [prerequisites](README.md#prerequisites).
2. Create a focused branch and a fresh task-specific build directory.
3. Implement the change in its owning layer. Add new implementation files to
   `cmake/meshcore_sources.cmake`; keep private headers out of the public interface.
4. Run the affected checks from [docs/testing.md](docs/testing.md). Public
   boundary changes need their API/type/hook inventory checks. Protocol/runtime
   behavior changes need relevant contract and parity cases.
5. Run `git diff --check`, describe the change and report actual validation.

Generic code is C99. Match the surrounding file's style; this repository does
not currently provide a formatter configuration. Keep patches focused, include
SPDX/copyright notices on new source files, and preserve third-party notices.
Use regression tests for changed behavior; documentation-only work uses link,
command and API-reference checks. See [the test strategy](PLAN.md#required-checks-by-change-type)
for CI and release expectations.

Use `area: imperative summary` for commit subjects. Keep the
subject below 72 characters and explain the reason and validation in the body
when they are not obvious. Do not include credentials or private device data.
This guide adds no DCO or CLA requirement.

## Upstream Compatibility Changes

Use [UPSTREAM.md](UPSTREAM.md) to prepare and verify the locked reference.
Classify changes as core, support, runtime, host evidence, excluded or deferred.
Explain any intentional difference; a passing evidence-reference inventory
alone does not establish behavioral compatibility. Updating the lock requires
reviewing affected mappings and running strict upstream checks.

Public API changes must follow [versioning](docs/versioning.md), update the
host integration guidance and add an entry to [CHANGELOG.md](CHANGELOG.md).
Keep an unavailable reference visible as skipped evidence instead of claiming
strict parity. Do not modify the reference tree to make comparisons pass.

## Reporting A Bug Or Asking For Help

Use this repository's issue tracker. Include:

- library commit, upstream-based version label and locked upstream commit;
- OS/compiler or Zephyr/board/host-adapter revision, build options and macros;
- a minimal reproducer, expected behavior and observed behavior;
- exact commands and relevant sanitized output;
- whether the failure is native, upstream-oracle, host integration or hardware.

Integration questions should identify which platform hooks are implemented.
Feature requests should explain a host use case and which layer should own it.

## Pull Requests

Describe the problem, resulting behavior, compatibility impact and evidence.
List unexecuted checks explicitly. Update user-facing docs with the behavior
rather than relying on PR discussion as the only explanation. Maintainers
review scope, upstream evidence, public contracts and test results; remote
branch protection and release approval are repository settings, not guarantees
made by this document.
