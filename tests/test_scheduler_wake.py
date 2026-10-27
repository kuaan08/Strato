#!/usr/bin/env python3
"""Compile and run deterministic host tests for parked-thread wake decisions."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="strato-scheduler-wake-") as directory:
    executable = str(Path(directory) / "test")
    subprocess.run([
        os.environ.get("CXX", "c++"), "-std=c++17", "-Wall", "-Wextra", "-Werror",
        "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-g",
        "-I", str(ROOT / "app/src/main/cpp"), str(ROOT / "tests/scheduler_wake_test.cpp"),
        "-o", executable,
    ], check=True)
    subprocess.run([executable], check=True)
print("PASS: parked-thread wake policy, including no next thread and equal-priority timeslices")
