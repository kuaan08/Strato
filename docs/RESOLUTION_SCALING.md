# Resolution scaling integration

Status: foundation and renderer audit only. Not exposed in the UI and not functional in games yet.
Base: settings RC2 commit 8820bddeaae06f7007fc19da4e52f35b214d1330.

Requested ratios: 0.50, 0.75, 1.00 (default), 1.25, 1.50, 1.75, 2.00.
`gpu/resolution_scaling.h` computes host extents with integer quarter ratios,
rejects device-limit violations, clamps tiny dimensions to one pixel and
handles invalid settings as native scale. ASan/UBSan tests cover these cases.
No render path consumes this helper yet; passing its tests is not validation
of resolution scaling or improved game performance.

## Confirmed integration requirements

Current `Texture::SynchronizeHostImpl` rejects unequal guest and host dimensions.
`Texture::CopyFrom` similarly requires equal dimensions. Buffer-image copies
use native mip layouts and buffer sizes. Changing these extents alone risks
reading past guest buffers and invalid Vulkan copy commands.

Reference: Eden 746a6d9160c817a173d97f645c7b8abf75020f32,
`renderer_vulkan/vk_texture_cache.cpp` Image::ScaleUp/ScaleDown/BlitScaleHelper.
Eden owns original and scaled images, tracks the active representation and
converts their contents at synchronization boundaries. It has special handling
for depth/stencil and other images unsupported by ordinary Vulkan blits.
Its texture cache and rasterizer also scale copy regions and framebuffer extents.

Before enabling the setting:

1. Give eligible render targets native and scaled Vulkan representations, with
   explicit content ownership, lifetime and fence tracking. Keep all guest sizes,
   allocation pitches and CPU readback buffers native.
2. Support upload/readback conversions, format-specific capability checks,
   depth/stencil, layers/mips and sample counts. Unsupported configurations must
   use a consistent native rendering path rather than mixed framebuffer sizes.
3. Integrate framebuffer, viewport, scissor, clears, 2D copies, presentation and
   texture-coordinate/shader query behavior with the active representation.
4. Apply scale only on the next emulation session. Keep 1x behavior intact.
5. Expose the seven choices globally and per game only once the renderer uses them.
6. Validate Vulkan commands and real readback pixels at all ratios, test restart,
   memory pressure and gameplay/cutscenes on the Mali driver. Higher resolutions
   may legitimately exceed device memory even when dimension limits are valid.

Absolute crash-free guarantees for all games and devices are not possible;
this feature needs runtime validation before release.
