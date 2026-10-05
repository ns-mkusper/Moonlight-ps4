# Latency tuning

Settings in `/data/moonlight/moonlight.ini` that trade smoothness, tearing or
robustness for lower button-to-photon latency. Defaults keep the validated
1.1.0 behaviour, except the input thread.

| Key | Default | Effect |
| --- | --- | --- |
| `input_poll_us` | `1000` | Polls the pad on a dedicated thread every N µs and sends state only when it changes. `0` restores the old poll from the 8 ms stream main loop, which adds ~4 ms average and 8 ms worst-case input latency and can miss very short presses. Values below 250 are clamped. |
| `ycbcr_buffers` | `2` | Scanout buffers for the YCbCr path (`prefer_ycbcr = true`), 1–3. Registration steps down 3 → 2 → 1 if the kernel rejects it, and allocation falls back to 2. |
| `ycbcr_wait_flip` | `true` | Blocks the decode thread after each YCbCr flip until the frame is on screen. With 2 buffers this is required and stays on regardless. With `ycbcr_buffers = 3` and `false`, decoding of the next frame overlaps the vblank wait instead of queueing behind it. |
| `flip_hsync` | `false` | Flips on hsync instead of vsync. Removes the average half-refresh (~8 ms at 60 Hz) wait for the next vblank, at the cost of a visible tear line. Falls back to vsync if the port rejects hsync. |
| `direct_submit` | `false` | Declares `CAPABILITY_DIRECT_SUBMIT` for the Videodec2 decoder: decode units go straight from the depacketizer to the decoder instead of through moonlight-common-c's queue thread. Moonlight for Android enables this for NVIDIA's decoder. |
| `rfi` | `false` | Declares `CAPABILITY_REFERENCE_FRAME_INVALIDATION_AVC`: after packet loss the host invalidates references instead of sending a full IDR frame, which avoids the large keyframe burst. Needs on-console validation that Videodec2 recovers cleanly from missing references. |

Related existing knob: `dec_pipeline_depth = 2` (the default since cfg rev 2)
overlaps CPU parsing with hardware decode but, per its own comment in
`config.c`, adds a frame of latency. `1` is synchronous decode.

## Validating a change

With `show_stats = true` (BGRA path) or the UDP stats log, compare one minute
before and after each change:

- `decode`, `present` and `drop` from the per-second stats line;
- input-to-photon from a host-side probe (e.g. a virtual pad on the host plus
  screen capture), 16+ trials per setting;
- `flip wait timeout` warnings in the log, which should not appear with
  `ycbcr_wait_flip = false` and 3 buffers.
