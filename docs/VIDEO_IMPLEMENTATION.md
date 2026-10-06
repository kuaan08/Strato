# NVDEC/VIC integration

Status: command diagnostics only. Video decoding and VIC output are not implemented by this branch yet.

Reference: yuzu commit `767b024755c70aeacdf555907b18c11b27127825`, preserved in
`eden-emulator/mirror`. The relevant files are `src/video_core/host1x/nvdec.cpp`,
`codecs/{h264,vp8,vp9}.cpp`, `nvdec_common.h`, `vic.cpp`, and `externals/ffmpeg/CMakeLists.txt`.
No reference decoder code is linked into this diagnostic build.

The current Strato submission path applies address relocations but does not execute engine
commands. It increments syncpoints on the CPU. The existing NVDEC and VIC classes are stubs.
Enabling the command FIFO alone cannot produce decoded pictures.

## Diagnostic capture

`VideoTrace` logs the first 32 submissions and at most 2048 method writes per channel.
Each inspected gather is capped at 8192 words. Register writes distinguish NVDEC from VIC,
including raw THI method addresses, to identify the codec and output configuration requested
by Donkey Kong Country Returns HD. Unknown or truncated opcodes stop inspection and are logged.
These diagnostics do not change command execution or fence behavior. They do not dump video payloads.

## Integration gates

1. Capture the actual NVDEC codec selection, THI method address units, VIC configuration, and
   syncpoint ordering. Preserve the captured metadata as a device-side diagnostic artifact.
2. Adapt Yuzu's bitstream reconstruction to checked Strato SMMU reads. Preserve each imported
   file's attribution and license and include applicable notices.
3. Integrate a pinned FFmpeg build with software decoding, bounded packet/frame allocations,
   explicit errors, decoder reset, and a frame queue with defined ownership.
4. Implement the requested VIC pixel formats, pitch/block-linear layout, and checked writes.
   Validate generated pixels against independently decoded reference fixtures.
5. Execute owned command buffers, retain pinned mappings until completion, and signal
   syncpoints after actual processing. Remove the current completion shortcut as part of
   this integration, not before the execution and lifetime requirements are satisfied.
6. Test decoder errors, malformed addresses, multiple frames, resets, and teardown. Then
   validate the Android build and the opening sequence on the device.

Passing the command parser and relocation tests does not establish video playback support.
