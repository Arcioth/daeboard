# Changelog

## 2026-09-23

- Created `~/Documents/daeboard` with `AGENTS.md`, `HANDOFF.md`, and this file.
- Recorded the measured LED path on the FA507NVR (`asus::kbd_backlight`, single zone, root write) and the recommended loop (epoll, timerfd, signalfd, `SOCK_SEQPACKET`).
- Daemon implemented and started as transient `daeboard.service`. musl static, 66 KB, idle RSS about 100 KB. Gesture tests pass. Kernel accepted static and breathe writes. Socket `ping` and `color` answered. Key gestures still need a hand on the keyboard.
- Enter holds each blink edge for 120 ms. Backspace snaps to full red instead of ramping, and writes brightness in the same turn. Meta breathe uses kernel speed 2.
- Added `README.md`. ctron integration steps are in the ctron repo's `PLANS.md`, not here.
- `install.sh` installs the binary and a systemd unit on non-NixOS machines. AUR and Flatpak are documented, not published.
- Macros load from a binds file. `reload` recompiles it. `fire <name>` goes to socket clients. Unknown key names are skipped. A 0 ms step holds only when it is last. Key codes match the kernel.
