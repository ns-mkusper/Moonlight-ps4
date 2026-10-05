// Persistent client configuration (simple INI).
#pragma once

#include <stdbool.h>
#include <Limelight.h>

#define CONFIG_DIR_PS4 "/data/moonlight"
#define CONFIG_MAX_HOST 128
#define CONFIG_MAX_APP  128

/* User-facing latency presets (SETTINGS → Latency mode). Each mode implies
 * values for direct_submit, ycbcr_buffers, ycbcr_wait_flip and flip_hsync;
 * those keys written explicitly in moonlight.ini override the mode. */
typedef enum {
    LATENCY_STANDARD = 0, /* validated defaults */
    LATENCY_LOW = 1,      /* direct submit, YCbCr triple buffering without flip wait */
    LATENCY_LOWEST = 2,   /* LOW + hsync flips (tearing) */
    LATENCY_MODE_COUNT
} latency_mode_t;

/* Bits in app_config_t.latency_overrides: mode-controlled keys that the ini set explicitly. */
#define LAT_OVR_DIRECT_SUBMIT   0x1u
#define LAT_OVR_YCBCR_BUFFERS   0x2u
#define LAT_OVR_YCBCR_WAIT_FLIP 0x4u
#define LAT_OVR_FLIP_HSYNC      0x8u

typedef struct {
    STREAM_CONFIGURATION stream; // embedded: width/height/fps/bitrate/...

    char host[CONFIG_MAX_HOST];
    char app_name[CONFIG_MAX_APP]; // name or numeric id
    char debug_host[64];

    bool sops;
    bool local_audio;
    bool prefer_hw;
    bool videodec2_spike;
    bool prefer_ycbcr;
    bool enable_file_log; // writes /data/moonlight/debug.log
    bool show_stats; // on-screen perf overlay (FPS/decode/convert/present/KB per frame)
    bool paired_ok; // runtime only

    /* Videodec2 tuning, A/B-able on console without a rebuild. Rev-2 defaults
     * (depth=2 + AU ONION) are the fast path; files with cfg_rev < 2 get them
     * re-applied on load. See docs/CONSOLE_VALIDATE.md. */
    int dec_pipeline_depth; // frames in flight inside Videodec2 (1..VIDEODEC2_MAX_FB)
    int dec_thread_prio;    // Orbis priority of the Vdec CPU worker (lower = higher)
    int slices_per_frame;   // CAPABILITY_SLICES_PER_FRAME asked of the host
    bool dec_au_onion;      // AU bitstream in cacheable ONION (false = WC_GARLIC)
    bool dec_fb_garlic;     // force decoder framebuffer to WC_GARLIC (skip ONION alias)
    int bgra_workers;       // NV12->BGRA convert threads (1..6)
    int bgra_nt;            // -1 auto, 0 cached stores, 1 streaming stores
    /* Latency tuning. latency_mode is the SETTINGS → Latency mode value; the
     * four mode-controlled knobs (direct_submit, ycbcr_buffers,
     * ycbcr_wait_flip, flip_hsync) are derived from it. Any of them present
     * in moonlight.ini overrides the mode (the menu then shows "(custom)"),
     * and picking a mode in the menu clears those overrides. config_save
     * writes a knob only when it differs from what the mode or default
     * implies, so future default changes still reach existing files. */
    int latency_mode;           // latency_mode_t
    unsigned latency_overrides; // LAT_OVR_* bits, runtime only
    int input_poll_us;      // pad poll period (us) on a dedicated thread; 0 = legacy 8 ms main-loop poll; clamped to >= 250
    int ycbcr_buffers;      // YCbCr scanout buffers (1..3); 3 lets decode overlap the vblank wait
    bool ycbcr_wait_flip;   // block the decode thread until each YCbCr flip is on screen
    bool flip_hsync;        // flip on hsync (tearing) instead of vsync: ~half a refresh less wait
    bool direct_submit;     // CAPABILITY_DIRECT_SUBMIT: decode on the receive thread, no DU queue
    bool rfi;               // experimental, ini only: CAPABILITY_REFERENCE_FRAME_INVALIDATION_AVC instead of IDR on loss
} app_config_t;

void config_set_defaults(app_config_t *cfg);
void config_ensure_dir(const char *dir);
int config_load(app_config_t *cfg, const char *dir);
int config_save(const app_config_t *cfg, const char *dir);

/* Set the mode-controlled knobs from cfg->latency_mode, keeping any knob
 * whose LAT_OVR_* bit is set in cfg->latency_overrides. */
void config_apply_latency_mode(app_config_t *cfg);
/* "standard" / "low" / "lowest" (ini value) and the menu label. */
const char *config_latency_mode_key(int mode);
const char *config_latency_mode_label(int mode);
