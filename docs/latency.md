# Latency settings

## Latency mode (SETTINGS tab)

Most users only need **SETTINGS → Latency mode**. Use left/right or X to cycle it;
the change is saved immediately and applies to the next stream.

| Mode | What it does | Trade-off |
| --- | --- | --- |
| **Standard** (default) | The tested defaults. | Smoothest picture. |
| **Low** | Decodes each frame as soon as it arrives instead of handing it to a queue thread first. With YCbCr output, uses three scanout buffers so decoding the next frame no longer waits for the previous one to reach the screen. | Less tested. |
| **Lowest (tearing)** | Low, plus frames are shown on the next scanline instead of waiting for the next screen refresh, saving on average half a refresh (~8 ms at 60 Hz). | A horizontal tear line may be visible. |

In every mode the controller is read every 1 ms on its own thread and only sent
to the host when it changes, instead of once per 8 ms main-loop tick.

## Advanced keys (moonlight.ini)

These keys in `/data/moonlight/moonlight.ini` fine-tune or override the mode.

| Key | Set by mode | Effect |
| --- | --- | --- |
| `latency_mode` | — | `standard`, `low` or `lowest`. Written by the SETTINGS tab. |
| `direct_submit` | Low, Lowest: `true` | `CAPABILITY_DIRECT_SUBMIT` for the hardware decoder: decode units go straight from the depacketizer to the decoder, skipping moonlight-common-c's queue thread. |
| `ycbcr_buffers` | Low, Lowest: `3` | YCbCr scanout buffers, 1–3 (only with `prefer_ycbcr = true`). Registration steps down 3 → 2 → 1 if the kernel rejects it, and allocation falls back to 2. |
| `ycbcr_wait_flip` | Low, Lowest: `false` | Block the decode thread after each YCbCr flip until the frame is on screen. Always on when fewer than 3 buffers are registered. |
| `flip_hsync` | Lowest: `true` | Flip on hsync instead of vsync. Falls back to vsync if the port rejects it. |
| `input_poll_us` | — (default `1000`) | Controller poll period on the input thread. `0` restores the old 8 ms main-loop poll. Values below 250 are clamped. |
| `rfi` | — (default `false`) | Experimental. `CAPABILITY_REFERENCE_FRAME_INVALIDATION_AVC`: after packet loss the host invalidates references instead of sending a full IDR frame. Needs validation that Videodec2 recovers cleanly from missing references. |

**Precedence.** The mode is applied first; any of `direct_submit`,
`ycbcr_buffers`, `ycbcr_wait_flip` or `flip_hsync` present in the file then
overrides it, and the menu shows the mode as "(custom)". Picking a mode in the
SETTINGS tab clears those overrides. The app only writes these four keys,
`input_poll_us` and `rfi` back to the file when they differ from what the mode
or default implies, so files stay short and future default changes still apply.

Related existing key: `dec_pipeline_depth = 2` (the default since cfg rev 2)
overlaps CPU parsing with hardware decode and adds a frame of latency, per its
comment in `config.c`. `1` is synchronous decode.

## Validating a change

With `show_stats = true` (BGRA path) or the UDP stats log, compare one minute
before and after a change:

- `decode`, `present` and `drop` from the per-second stats line;
- the stream-start log line, which shows `latency=<mode>` and `(custom)` when
  the ini overrides it;
- `flip wait timeout` warnings, which should not appear in Low or Lowest with
  YCbCr and 3 buffers registered.
