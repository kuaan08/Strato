#!/usr/bin/env python3
"""Guard safe fallback conditions in the JNI system Vulkan driver query."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "app/src/main/cpp/driver_jni.cpp").read_text(encoding="utf-8")
kotlin = (ROOT / "app/src/main/java/org/stratoemu/strato/utils/GpuDriverHelper.kt").read_text(encoding="utf-8")
gpu_loader = (ROOT / "app/src/main/cpp/skyline/gpu.cpp").read_text(encoding="utf-8")

for guard in (
    "if (!vulkanLibrary)",
    "if (!getInstanceProcAddr)",
    "if (physicalDevices.empty())",
    "catch (const std::exception &error)",
    "catch (...)",
):
    assert guard in source, f"missing JNI Vulkan discovery guard: {guard}"
assert "getSystemDriverInfo() : Array<String>?" in kotlin
assert "systemDriverInfo?.get(0) ?: \"\"" in kotlin
assert "systemDriverInfo?.get(1) ?: \"\"" in kotlin
assert "if (IsKnownSwitchIncompatibleTurnipR8(selectedDriver))" in gpu_loader
assert "using the system Vulkan driver" in gpu_loader
print("Vulkan driver discovery returns the documented nullable fallback on failures")
