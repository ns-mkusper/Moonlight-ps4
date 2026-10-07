#pragma once
/*
 * Latency test hooks, off unless moonlight.ini sets test_hooks_port.
 *
 * A host-side harness sends one-word UDP commands to that port:
 *   flash  draw a white patch (top-left corner) into the next presented
 *          frame; the client replies "flash <us>" with the time from
 *          command received to SubmitFlip of that frame.
 *   tap    inject a right-stick tap (full right) into the controller stream
 *          for test_hooks_tap_ms; the client replies "tap <us>" with the time
 *          from command received to the tap leaving for the host.
 *   ping   replies "pong", to measure the UDP hop.
 * Replies go to the sender's address. Every reply is a duration on the
 * console's clock, so the harness never needs the two clocks in sync.
 */
#include <stdbool.h>
#include <stdint.h>

int test_hooks_start(int port, int tap_ms);
void test_hooks_stop(void);

/* Renderer: true once per "flash" command; the caller paints the patch into
 * the frame it is about to flip, then calls test_hooks_flash_flipped(). */
bool test_hooks_take_flash(void);
void test_hooks_flash_flipped(void);

/* Input: true while an injected tap is active; the caller overrides the
 * right stick and calls test_hooks_tap_sent() when that state is sent. */
bool test_hooks_tap_active(void);
void test_hooks_tap_sent(void);
