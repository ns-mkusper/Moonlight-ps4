#!/usr/bin/env python3
"""Drive the client's latency test hooks (moonlight.ini: test_hooks_port).

  scripts/latency_hooks.py PS4_IP ping  [-n 100]   UDP round trip
  scripts/latency_hooks.py PS4_IP flash [-n 20]    command -> SubmitFlip of the patched frame
  scripts/latency_hooks.py PS4_IP tap   [-n 20]    command -> tap sent to the host

Every number the client replies with is a duration on the console's clock.
Pair "flash" with a camera on the TV and "tap" with a capture of the game
to get panel and end-to-end latency on the laptop's own clock.
"""
import argparse
import socket
import statistics
import time


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("host")
    ap.add_argument("cmd", choices=["ping", "flash", "tap"])
    ap.add_argument("-p", "--port", type=int, default=48100)
    ap.add_argument("-n", type=int, default=10)
    ap.add_argument("--gap", type=float, default=0.5, help="seconds between commands")
    a = ap.parse_args()

    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.settimeout(1.0)
    vals = []
    for _ in range(a.n):
        t0 = time.monotonic()
        s.sendto(a.cmd.encode(), (a.host, a.port))
        try:
            data, _ = s.recvfrom(64)
        except socket.timeout:
            print("timeout")
            continue
        rtt_ms = (time.monotonic() - t0) * 1000
        word, _, val = data.decode().partition(" ")
        if a.cmd == "ping":
            vals.append(rtt_ms)
            print(f"pong rtt={rtt_ms:.2f} ms")
        else:
            ms = int(val) / 1000
            vals.append(ms)
            print(f"{word} {ms:.2f} ms (console clock), reply after {rtt_ms:.1f} ms")
        time.sleep(a.gap)
    if vals:
        print(f"n={len(vals)} median={statistics.median(vals):.2f} ms "
              f"min={min(vals):.2f} max={max(vals):.2f}")


if __name__ == "__main__":
    main()
