#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 FoBE Studio
# SPDX-License-Identifier: Apache-2.0

"""Require every declared MeshCore Zephyr scenario and case to execute."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path


SCENARIO = re.compile(r"^  ([A-Za-z0-9_.-]+):\s*$", re.MULTILINE)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tests-root", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--platform", default="native_sim/native")
    args = parser.parse_args()

    expected: set[str] = set()
    for metadata in args.tests_root.rglob("testcase.yaml"):
        found = SCENARIO.findall(metadata.read_text(encoding="utf-8"))
        if not found:
            print(f"no scenario in {metadata}", file=sys.stderr)
            return 1
        for name in found:
            if name in expected:
                print(f"duplicate scenario: {name}", file=sys.stderr)
                return 1
            expected.add(name)

    suites = json.loads(args.report.read_text(encoding="utf-8"))["testsuites"]
    actual = [suite for suite in suites if suite["platform"] == args.platform]
    names = [suite["name"] for suite in actual]
    errors: list[str] = []
    if set(names) != expected or len(names) != len(expected):
        errors.append(f"scenario mismatch: expected={sorted(expected)}, actual={sorted(names)}")

    case_count = 0
    for suite in actual:
        if suite.get("status") != "passed" or not suite.get("runnable", False):
            errors.append(f"{suite['name']}: status={suite.get('status')}, runnable={suite.get('runnable')}")
        cases = suite.get("testcases", [])
        if not cases:
            errors.append(f"{suite['name']}: no executed test cases")
        for case in cases:
            case_count += 1
            if case.get("status") != "passed":
                errors.append(f"{case.get('identifier')}: {case.get('status')}")

    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1
    print(f"Twister execution: PASS ({len(expected)} scenarios, {case_count} cases, no skips)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
