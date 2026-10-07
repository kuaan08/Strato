# NVDEC/VIC video playback

Status: experimental implementation; Android and real-game playback validation pending.

The previous branch applied Host1x relocations and advanced syncpoints without
executing NVDEC/VIC commands. Audio could play but no video frames were generated.

This branch executes owned command gathers on Host1x workers, retains NvMap pins
until completion and stops workers before NvMap destruction. Syncpoint conditions
are completed on the worker, after preceding synchronous engine work.

NVDEC reconstructs H.264/VP8/VP9 packets using Eden/Yuzu code, decodes with pinned
FFmpeg n6.1.2 software decoders, and passes frames through a synchronized queue.
VIC reads guest configuration, applies its color matrix and writes RGBA/BGRA/XRGB
or NV12 surfaces to SMMU memory in pitch or Tegra block-linear layout.

Reference: Eden 746a6d9160c817a173d97f645c7b8abf75020f32. Original attribution
is preserved; license texts and notices are bundled under assets/licenses/video.

## Scope and remaining limitations

Progressive 8-bit YUV420/NV12 input and a single opaque VIC slot are implemented.
Advanced VIC filtering, interlacing, transpose and alpha composition are not
implemented. Explicit errors identify unsupported output paths. VP9 packet
reconstruction is ported but has not yet passed an end-to-end sample test.
CPU decoding may be slower than hardware decoding; no PanVK, renderer, UI or
frame-pacing behavior is changed by this work.

## Validation

`tests/test_video_pipeline.py` compiles the actual portable video module with
ASan/UBSan, reconstructs synthetic H.264 and VP8 bitstreams, decodes them and
checks guest pixels against known red-color values. Pitch and GOB heights
1 through 32 and out-of-range address rejection are covered.

`tests/test_host1x_relocations.py` exercises the production submission method:
relocations, malformed request rejection, failure atomicity, immutable command
copies, deferred syncpoint completion and pin retention/release.

Passing these tests does not establish playback in Donkey Kong. The Android build
and opening cinematic still require validation on the device.
