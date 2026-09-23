# Changelog

## 2026-09-23

- Created `~/Documents/daeboard` with `AGENTS.md`, `HANDOFF.md`, and this file.
- Recorded the measured LED path on the FA507NVR (`asus::kbd_backlight`, single zone, root write) and the recommended loop (epoll, timerfd, signalfd, `SOCK_SEQPACKET`).
- No daemon code. Spec written to `docs/superpowers/specs/2026-09-23-daeboard-design.md` after the process shape was accepted. Live aura writes use cmd 0 (set), never cmd 1 (save to BIOS).
