#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 FoBE Studio
"""Execute locked upstream CLI/policy excerpts against native C scenarios.

The harness substitutes clocks, RNG, host authorization and packet allocation;
the selected send/receive/reply/policy bodies are compiled from reference source.
It compares plaintext and queued routing/timing, not Arduino or RF execution.
"""

import argparse
import difflib
import re
from pathlib import Path
import shutil
import subprocess
import sys


def block(text, marker):
    """Extract a brace-balanced body from the selected, fixed evidence seam."""
    start = text.index("{", text.index(marker))
    depth = 0
    for end in range(start, len(text)):
        if text[end] == "{":
            depth += 1
        elif text[end] == "}":
            depth -= 1
            if depth == 0:
                return text[start:end + 1]
    raise ValueError(f"Unclosed upstream block: {marker}")


def generate(ref, template):
    chat = (ref / "src/helpers/BaseChatMesh.cpp").read_text()
    companion = (ref / "examples/companion_radio/MyMesh.cpp").read_text()
    repeater = (ref / "examples/simple_repeater/MyMesh.cpp").read_text()
    room = (ref / "examples/simple_room_server/MyMesh.cpp").read_text()
    sensor = (ref / "examples/simple_sensor/SensorMesh.cpp").read_text()
    def define(source, name):
        match = re.search(r"^\s*#define\s+" + name + r"\s+(\d+)\b", source, re.M)
        if not match:
            raise ValueError(f"Missing upstream constant: {name}")
        return match.group(1)
    template = template.replace("@CHAT_DELAY@", define(chat, "CLI_REPLY_DELAY_MILLIS"))
    template = template.replace("@REPEATER_DELAY@", define(repeater, "CLI_REPLY_DELAY_MILLIS"))
    template = template.replace("@SENSOR_DELAY@", define(sensor, "CLI_REPLY_DELAY_MILLIS"))
    template = template.replace("@ROOM_DELAY@", define(
        (ref / "examples/simple_room_server/MyMesh.h").read_text(), "SERVER_RESPONSE_DELAY"))
    # Restrict reply extraction to the CLI branch, not earlier binary replies.
    repeater = repeater[repeater.index("char *command = (char *)&data[5];"):]
    room = room[room.index("uint32_t delay_millis;"):]
    sensor = sensor[sensor.index("char *command = (char *) &data[5];"):]
    replacements = {
        "SEND": block(chat, "int  BaseChatMesh::sendCommandData("),
        "DATA": block(chat, "else if (flags == TXT_TYPE_CLI_DATA)"),
        "COMMAND": block(chat, "else if (flags == TXT_TYPE_CLI_COMMAND)"),
        "REPEATER": block(repeater, "if (text_len > 0)"),
        "ROOM": block(room, "if (text_len > 0)"),
        "SENSOR": block(sensor, "if (text_len > 0)"),
        "FLOOD_POLICY": block(companion, "uint32_t MyMesh::getRetransmitDelay("),
        "DIRECT_POLICY": block(companion, "uint32_t MyMesh::getDirectRetransmitDelay("),
    }
    for name, body in replacements.items():
        template = template.replace("@" + name + "@", body)
    if re.search(r"@[A-Z_]+@", template):
        raise ValueError("Unresolved upstream oracle template marker")
    return template


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", default=".")
    parser.add_argument("--runtime-test", required=True)
    parser.add_argument("--work-dir", required=True)
    parser.add_argument("--cxx", default=None)
    parser.add_argument("--require-reference", action="store_true")
    args = parser.parse_args()
    root = Path(args.repo_root).resolve()
    ref = root / ".reference/meshcore"
    if not ref.is_dir():
        print("missing reference; runtime oracle not executed")
        return 1 if args.require_reference else 0
    subprocess.run([sys.executable, str(root / "tools/upstream_lock_check.py"),
                    "--repo-root", str(root)], check=True)
    cxx = args.cxx or shutil.which("c++")
    if not cxx:
        raise RuntimeError("runtime oracle requires a C++ compiler")
    work = Path(args.work_dir).resolve()
    work.mkdir(parents=True, exist_ok=True)
    source = work / "upstream_runtime.cpp"
    source.write_text(generate(ref, (root / "tests/oracle/upstream_runtime.cpp.in").read_text()))
    exe = work / "upstream_runtime"
    subprocess.run([cxx, "-std=c++11", "-Wall", "-Wextra", "-Werror",
                    "-I", str(ref / "src"), str(source), "-o", str(exe)], check=True)
    expected = subprocess.check_output([str(exe)], text=True)
    actual = subprocess.check_output([str(Path(args.runtime_test).resolve()), "--oracle"], text=True)
    if actual != expected:
        print("".join(difflib.unified_diff(expected.splitlines(True), actual.splitlines(True),
                                         fromfile="upstream", tofile="C runtime")))
        return 1
    print(f"upstream runtime oracle: {len(actual.splitlines())} scenario rows passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
