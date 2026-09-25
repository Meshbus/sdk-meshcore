# MeshCore C Library Agent Rules

This independent repository owns the platform-neutral C MeshCore protocol and
runtime library. Hosts own scheduling, storage, transport, and hardware
integration. Preserve upstream wire/runtime compatibility and the affected
public C contracts.

## Read By Task

- For architecture or compatibility changes, read the relevant sections of
  `ARCHITECTURE.md` and `UPSTREAM.md`. They own the layer model, source manifest,
  evidence mapping, and upstream synchronization procedure.
- For host integration or public interface work, consult the affected headers
  and `docs/porting.md`.
- For interface compatibility decisions, version changes, or release preparation,
  consult `docs/versioning.md`.
- For validation, select applicable checks from `docs/testing.md`.
- For gap analysis, use the behavior classification in `ARCHITECTURE.md`;
  identify expected behavior, upstream evidence, affected C surface, risk,
  validation, and ownership boundaries.

Read only the sections needed by the task and reuse unchanged material already
read. Current source and tests describe implementation; locked upstream evidence
is the arbiter for compatibility disagreements. Keep `.reference/meshcore`
read-only unless an upstream update is explicitly authorized.

## Engineering Boundaries

- Keep generic code plain C. Product services, boards, storage implementations,
  Bluetooth, UI, and concrete transports belong in the host.
- Keep protocol core, promoted support helpers, runtime behavior, and platform
  hooks in their documented layers. Example behavior enters runtime or an
  explicitly promoted support helper, rather than protocol core by default.
- Define affected public surfaces and compatibility evidence for architecture
  changes. Move files only when the responsibility change warrants it.
- Preserve observable behavior and test coverage. Before removing or relocating
  a test, identify where its protected behavior will be verified or why removal
  is justified. Extend the public interface only for an intended host contract.
- Preserve direct/flood contact semantics: `has_out_path=true` with zero path
  bytes is a known direct zero-hop route; `has_out_path=false` means unknown and
  falls back to flood. Companion's `out_path_len == 0xff` wire/storage sentinel
  is separate from the host data shape.

Preserve unrelated changes and keep independent repositories scoped separately.
Use existing authorization; dependency/reference changes, hardware actions,
signing, and publication require explicit authorization. Stage, commit, or push
only when requested. Continue independent authorized work while a required
scope or authorization decision is pending.

## Completion

Run the smallest relevant checks and use task-specific output directories.
Fix in-scope failures and rerun affected checks. Expand validation only for
changed behavior, failures, or a named acceptance gap. Report actual results
and unmet acceptance criteria; distinguish protocol parity, runtime oracle,
public-contract/boundary, and downstream host or hardware evidence.
