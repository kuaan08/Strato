# Experimental internal resolution scaling

The selector is in global Settings > Graphics and in per-title display settings.
Available values: 0.50x, 0.75x, 1x (default), 1.25x, 1.50x, 1.75x and 2x.
Restart the game to apply a change. Invalid preference values use native resolution.

## Implementation and limits

Eligible graphics passes render to real scaled Vulkan color/depth/stencil attachments.
Native contents are loaded before the pass; results are resolved to the original images
before any subsequent guest texture sampling, image copies, CPU readback or presentation.
Guest addresses, tiling, pitches, allocations and sampled image dimensions remain native.
Viewports and scissors scale with the actual rounded image dimensions. Cached native
viewport/scissor state is restored when switching back to a native pass.
Scaled images and transient framebuffers are retained by the submission fence cycle.
The original 1x texture allocation and presentation paths remain in use.

This is NOT full Eden resolution-scaling parity. Eden retains scaled texture-cache images
and applies shader coordinate compensation. This first Strato implementation isolates
rasterization and keeps guest-visible images native. The extra resolve/load blits can
outweigh reduced fragment work, especially for many short passes. No FPS improvement
or crash-free gameplay is claimed without device measurements.

Whole passes remain native for partial/mixed attachment sizes, input/feedback attachments,
layered/mip views, multisampling, non-triangle topology, screen-coordinate fragment shader
reads, storage writes or unsupported blit formats. Precise occlusion queries disable scaling
for the remainder of the emulation session to retain native pixel counter semantics.
Image/framebuffer/viewport device limits are checked. Additional scaled image memory
has a 128 MiB reservation limit; passes exceeding it remain native. Optional allocation failures disable
scaling for the session and are logged; unrelated errors and device loss still propagate.

## Validation

- `tests/test_resolution_scaling.py`: all ratios, tiny/odd dimensions, invalid indices,
  integer overflow and device limits, with ASan/UBSan.
- `tests/test_resolution_vulkan.py`: actual rasterization at seven scales and even/odd
  dimensions with the shared renderer blit and dynamic-state code. Verifies color,
  depth, stencil, native CPU readback, negative-height viewports and restoring cached
  native state in a subsequent native draw, with Khronos validation and synchronization
  validation enabled.
- Android workflow compiles the settings resources, Kotlin and native code.

The Vulkan test is host validation, not validation of PanVK or any specific game.
Device tests have not yet been completed.

## Device test

1. Keep existing app data and drivers. Record the APK commit, driver, game version,
   docked mode and display refresh rate.
2. Use custom settings for one game. Start at 1x and record the same scene, HUD and
   frame times. Then restart at 0.50x, 0.75x and 2x. Test the intermediate scales next.
3. Confirm log entries `Internal resolution scale` and `Scaled render attachment`.
   A selected value alone does not demonstrate scaled rendering; native fallback is logged.
4. Check menus, gameplay, scene changes, cutscenes, shadows/depth/stencil effects,
   repeated game restarts and several minutes of play. Restore 1x and compare again.
5. Start with Celeste, then JoJo, LEGO Jurassic World and Donkey Kong. Treat any game
   with only native fallback as unsupported until its shader/cache paths are implemented.
6. Compare the same scene and driver in Eden separately. Do not infer equivalence from
   the setting label or from FPS alone. Collect logs and screenshots for discrepancies.
