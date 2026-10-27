#!/usr/bin/env python3
"""Compile the production scheduler timing helpers and run deterministic checks."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="strato-scheduler-timing-") as directory:
    executable = str(Path(directory) / "test")
    subprocess.run([
        os.environ.get("CXX", "c++"), "-std=c++17", "-Wall", "-Wextra", "-Werror",
        "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-g",
        "-I", str(ROOT), str(ROOT / "tests/scheduler_timing_test.cpp"), "-o", executable,
    ], check=True)
    subprocess.run([executable], check=True)
