#!/usr/bin/env python3
# Copyright (c) 2026 FoBE Studio
# SPDX-License-Identifier: Apache-2.0
"""Check repository-local Markdown links, heading anchors and shell syntax.

No network access or example execution. Fenced code is excluded from link
checks; shell fences are parsed by bash -n. This is a check for the repository's
inline Markdown link style, not a general Markdown or GitHub renderer.
"""

from __future__ import annotations

import argparse
from collections import Counter
from pathlib import Path
import re
import subprocess
from urllib.parse import unquote, urlsplit


FENCE = re.compile(r"^```([^\n]*)\n(.*?)^```[^\n]*$", re.M | re.S)
LINK = re.compile(r"!?\[[^\]]*\]\(([^\s)]+)\)")


def markdown_files(root: Path) -> list[Path]:
    # Also works in a release source archive with no Git metadata. Restrict the
    # scan to documentation roots so build/reference trees cannot enter it.
    files = list(root.glob("*.md"))
    for name in ("docs", "include", "src", "examples", ".github"):
        files.extend((root / name).rglob("*.md"))
    return sorted(files)


def anchors(text: str) -> set[str]:
    result: set[str] = set()
    seen: Counter[str] = Counter()
    for heading in re.findall(r"^#{1,6}\s+(.+?)\s*#*\s*$", text, re.M):
        slug = re.sub(r"[^\w\- ]", "", heading.lower()).replace(" ", "-")
        count = seen[slug]
        result.add(slug + (f"-{count}" if count else ""))
        seen[slug] += 1
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", type=Path, default=Path("."))
    args = parser.parse_args()
    root = args.repo_root.resolve()
    files = markdown_files(root)
    errors: list[str] = []
    link_count = shell_count = 0
    for path in files:
        text = path.read_text(encoding="utf-8")
        name = path.relative_to(root)
        for fence in FENCE.finditer(text):
            if fence.group(1).strip() not in {"sh", "bash"}:
                continue
            shell_count += 1
            result = subprocess.run(
                ["bash", "-n"], input=fence.group(2), text=True,
                capture_output=True, check=False,
            )
            if result.returncode:
                errors.append(f"{name}: shell syntax: {result.stderr.strip()}")
        prose = FENCE.sub("", text)
        for match in LINK.finditer(prose):
            url = urlsplit(match.group(1).strip("<>"))
            if url.scheme or url.netloc:
                continue
            link_count += 1
            target = (path.parent / unquote(url.path)).resolve() if url.path else path
            if not target.is_relative_to(root):
                errors.append(f"{name}: link escapes repository: {match.group(1)}")
            elif not target.exists():
                errors.append(f"{name}: missing target: {match.group(1)}")
            elif url.fragment and target.suffix == ".md":
                target_text = FENCE.sub("", target.read_text(encoding="utf-8"))
                if unquote(url.fragment) not in anchors(target_text):
                    errors.append(f"{name}: missing anchor: {match.group(1)}")
    print(f"docs: {len(files)} Markdown files, {link_count} local links, "
          f"{shell_count} shell blocks, {len(errors)} errors")
    for error in errors:
        print(error)
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
