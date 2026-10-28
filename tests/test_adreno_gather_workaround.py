#!/usr/bin/env python3
"""Keep the Adreno/Turnip gather-rounding workaround narrowly driver-gated."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
profile = (ROOT / "app/libraries/shader-compiler/profile.h").read_text(encoding="utf-8")
shader_emitter = (
    ROOT / "app/libraries/shader-compiler/backend/spirv/emit_spirv_image.cpp"
).read_text(encoding="utf-8")
trait_header = (ROOT / "app/src/main/cpp/skyline/gpu/trait_manager.h").read_text(
    encoding="utf-8"
)
trait_source = (ROOT / "app/src/main/cpp/skyline/gpu/trait_manager.cpp").read_text(
    encoding="utf-8"
)
shader_manager = (ROOT / "app/src/main/cpp/skyline/gpu/shader_manager.cpp").read_text(
    encoding="utf-8"
)

assert "bool need_gather_subpixel_offset{};" in profile
assert "bool needsGatherSubpixelOffset{};" in trait_header
for driver_id in ("eQualcommProprietary", "eMesaTurnip"):
    marker = f"case vk::DriverId::{driver_id}:"
    start = trait_source.index(marker)
    end = trait_source.find("break;", start)
    assert end != -1 and "needsGatherSubpixelOffset = true;" in trait_source[start:end]
assert ".need_gather_subpixel_offset = traits.quirks.needsGatherSubpixelOffset," in shader_manager

assert "Id ImageGatherSubpixelOffset(" in shader_emitter
for operation in ("OpImageQuerySizeLod", "OpConvertUToF", "OpFDiv", "OpFAdd"):
    assert operation in shader_emitter
assert "ctx.Const(0x1p-9f)" in shader_emitter
for texture_type in ("Color2D", "Color2DRect", "ColorArray2D", "ColorCube"):
    assert f"case TextureType::{texture_type}:" in shader_emitter

assert shader_emitter.count("ImageGatherSubpixelOffset(") == 3
for function_name in ("EmitImageGather", "EmitImageGatherDref"):
    start = shader_emitter.index(f"Id {function_name}(")
    end = shader_emitter.find("\nId ", start + 1)
    assert end != -1 and "ImageGatherSubpixelOffset(" in shader_emitter[start:end]

print("Adreno/Turnip texture-gather workaround is driver-gated and scoped to gather operations")
