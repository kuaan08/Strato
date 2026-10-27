#!/usr/bin/env python3
"""Compile and run host tests for Vulkan pipeline cache file bounds."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="strato-pipeline-cache-") as directory:
    executable = str(Path(directory) / "test")
    subprocess.run([
        os.environ.get("CXX", "c++"), "-std=c++17", "-Wall", "-Wextra", "-Werror",
        "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-g",
        "-I", str(ROOT / "app/src/main/cpp"), str(ROOT / "tests/pipeline_cache_file_test.cpp"),
        "-o", executable,
    ], check=True)
    subprocess.run([executable], check=True)
print("PASS: pipeline-cache header, exact payload size and integer/stream bounds")
