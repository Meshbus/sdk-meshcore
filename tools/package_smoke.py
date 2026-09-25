#!/usr/bin/env python3
# Copyright (c) 2026 FoBE Studio
# SPDX-License-Identifier: Apache-2.0

"""Install meshcore and verify a downstream CMake consumer can run."""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path


def parse_args() -> argparse.Namespace:
  parser = argparse.ArgumentParser()
  parser.add_argument("--source-root", default=".", help="MeshCore source root")
  parser.add_argument(
      "--work-dir",
      default="build.meshcore-package-smoke",
      help="Scratch directory for package smoke build outputs",
  )
  parser.add_argument("--cmake", default="cmake", help="CMake executable")
  return parser.parse_args()


def run(command: list[str], cwd: Path) -> None:
  print("+ " + " ".join(command), flush=True)
  subprocess.run(command, cwd=cwd, check=True)


def run_expect_version_rejection(command: list[str], cwd: Path, version: str) -> None:
  print("+ " + " ".join(command) + " (expected failure)", flush=True)
  result = subprocess.run(
      command,
      cwd=cwd,
      check=False,
      text=True,
      stdout=subprocess.PIPE,
      stderr=subprocess.STDOUT,
  )
  if result.returncode == 0:
    print(result.stdout, end="", file=sys.stderr)
    raise RuntimeError("command unexpectedly succeeded")
  expected = f'compatible with requested version "{version}"'
  if expected not in " ".join(result.stdout.split()):
    print(result.stdout, end="", file=sys.stderr)
    raise RuntimeError("consumer failed for a reason other than version rejection")


def find_minimal_host(consumer_build: Path) -> Path | None:
  names = ("meshcore_minimal_host", "meshcore_minimal_host.exe")
  for name in names:
    direct = consumer_build / name
    if direct.is_file():
      return direct
  for path in consumer_build.rglob("*"):
    if path.name in names and path.is_file():
      return path
  return None


def check_installed_licenses(source_root: Path, install_prefix: Path) -> None:
  doc_dir = install_prefix / "share/doc/meshcore"
  for name in ("LICENSE", "LICENSING.md", "UPSTREAM.md", "upstream.lock",
               "LICENSES/Apache-2.0.txt", "LICENSES/MIT-MeshCore.txt",
               "LICENSES/BSD-2-Clause-Monocypher.txt"):
    installed = doc_dir / name
    if not installed.is_file():
      raise RuntimeError(f"missing installed license material: {installed}")
    if installed.read_bytes() != (source_root / name).read_bytes():
      raise RuntimeError(f"installed license material differs from source: {name}")

  apache = (doc_dir / "LICENSES/Apache-2.0.txt").read_bytes()
  if (doc_dir / "LICENSE").read_bytes() != b"Copyright (c) 2026 FoBE Studio\n\n" + apache:
    raise RuntimeError("installed root license must contain FoBE Studio and full Apache-2.0 text")

  notice = " ".join((doc_dir / "LICENSES/BSD-2-Clause-Monocypher.txt").read_text().split())
  crypto_dir = source_root / "src/support/crypto"
  for name in ("monocypher.c", "monocypher.h",
               "monocypher-ed25519.c", "monocypher-ed25519.h"):
    text = "\n".join(line.removeprefix("//").lstrip() for line in
                     (crypto_dir / name).read_text().splitlines())
    copyright_line = next(line for line in text.splitlines()
                          if line.startswith("Copyright (c)"))
    start = text.index("Redistribution and use in source and binary forms")
    end_marker = "OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
    end = text.index(end_marker, start) + len(end_marker)
    terms = " ".join(text[start:end].split())
    if copyright_line not in notice or terms not in notice:
      raise RuntimeError(f"installed Monocypher license omits Monocypher BSD notice: {name}")
  print("installed license materials: PASS (Apache-2.0, upstream MIT and Monocypher BSD notices)")


def main() -> int:
  args = parse_args()
  source_root = Path(args.source_root).resolve()
  work_dir = Path(args.work_dir).resolve()
  install_build = work_dir / "install-build"
  install_prefix = work_dir / "prefix"
  consumer_build = work_dir / "consumer-build"
  if not (source_root / "CMakeLists.txt").is_file():
    print(f"missing MeshCore source root: {source_root}", file=sys.stderr)
    return 1

  lock = dict(line.split("=", 1) for line in
              (source_root / "upstream.lock").read_text().splitlines()
              if line and not line.startswith("#"))
  version = lock["base_tag"].removeprefix("companion-v")
  expected_label = lock["base_tag"]
  if lock["version_kind"] == "dev":
    expected_label += "-dev." + lock["commit"][:12]

  if work_dir.exists():
    shutil.rmtree(work_dir)
  work_dir.mkdir(parents=True)

  run(
      [
          args.cmake,
          "-S",
          str(source_root),
          "-B",
          str(install_build),
          "-DMESHCORE_INSTALL=ON",
          "-DMESHCORE_BUILD_TESTS=OFF",
          "-DMESHCORE_BUILD_EXAMPLES=OFF",
          "-DCMAKE_INSTALL_DOCDIR=share/doc/meshcore",
      ],
      source_root,
  )
  run([args.cmake, "--build", str(install_build), "--parallel"], source_root)
  run(
      [
          args.cmake,
          "--install",
          str(install_build),
          "--prefix",
          str(install_prefix),
      ],
      source_root,
  )
  check_installed_licenses(source_root, install_prefix)
  run(
      [
          args.cmake,
          "-S",
          str(source_root / "examples/minimal_host"),
          "-B",
          str(consumer_build),
          f"-DCMAKE_PREFIX_PATH={install_prefix}",
          f"-DMESHCORE_FIND_VERSION={version}",
      ],
      source_root,
  )
  major, minor, patch = map(int, version.split("."))
  rejected_versions = {"0.4.0", f"{major}.{minor}.{patch + 1}",
                       f"{major}.{minor + 1}.0"}
  if patch:
    rejected_versions.add(f"{major}.{minor}.{patch - 1}")
  rejected_versions.discard(version)
  for rejected in sorted(rejected_versions):
    run_expect_version_rejection(
        [args.cmake, "-S", str(source_root / "examples/minimal_host"),
         "-B", str(work_dir / f"reject-{rejected}"),
         f"-DCMAKE_PREFIX_PATH={install_prefix}",
         f"-DMESHCORE_FIND_VERSION={rejected}"],
        source_root, rejected,
    )

  # Check installed metadata independently of the numeric version guard.
  metadata_source = work_dir / "metadata-source"
  metadata_source.mkdir()
  checks = {"VERSION_STRING": expected_label,
            "VERSION_KIND": lock["version_kind"],
            "UPSTREAM_TAG": lock["base_tag"],
            "UPSTREAM_COMMIT": lock["commit"]}
  content = ("cmake_minimum_required(VERSION 3.20)\n"
             "project(meshcore_metadata_consumer LANGUAGES C)\n"
             f"find_package(meshcore {version} EXACT CONFIG REQUIRED)\n")
  for key, expected in checks.items():
    content += (f'if(NOT meshcore_{key} STREQUAL "{expected}")\n'
                f'  message(FATAL_ERROR "Incorrect meshcore_{key}")\n'
                'endif()\n')
  (metadata_source / "CMakeLists.txt").write_text(content)
  run([args.cmake, "-S", str(metadata_source),
       "-B", str(work_dir / "metadata-build"),
       f"-DCMAKE_PREFIX_PATH={install_prefix}"], source_root)
  run([args.cmake, "--build", str(consumer_build), "--parallel"], source_root)

  exe = find_minimal_host(consumer_build)
  if exe is None:
    print("minimal host executable was not produced", file=sys.stderr)
    return 1

  result = subprocess.run(
      [str(exe)],
      cwd=consumer_build,
      check=False,
      text=True,
      stdout=subprocess.PIPE,
      stderr=subprocess.PIPE,
  )
  if result.returncode != 0:
    print(result.stdout, end="")
    print(result.stderr, end="", file=sys.stderr)
    return result.returncode
  if "meshcore minimal host initialized" not in result.stdout:
    print("unexpected minimal host output:", file=sys.stderr)
    print(result.stdout, file=sys.stderr)
    return 1

  print(result.stdout, end="")
  print(f"package version compatibility: PASS ({version} accepted; "
        f"{', '.join(sorted(rejected_versions))} rejected)")
  print(f"installed version metadata: PASS ({expected_label})")
  print(f"package smoke: PASS ({install_prefix})")
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
