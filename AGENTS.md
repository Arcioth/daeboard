# daeboard

C11 daemon that paints the FA507NVR keyboard from live events. Sibling of `~/Documents/daetron` in toolchain and footprint. Not part of the ctron tree. Ctron, when it grows a client, speaks the socket. The daemon runs with no client connected.

Apache-2.0, matching daetron, once there is code. No code yet. Design is open. See `HANDOFF.md`.

## Do

- C11, `musl-gcc -static` by default, glibc via `make STATIC=0`. Flags in the daetron shape: `-O2 -pipe -Wall -Wextra -std=c11 -s`.
- One blocking call: `epoll_wait`. Arm `timerfd` only while a transient effect needs frames. Static color and firmware effects leave the timer disarmed, so idle is a sleeping process.
- `signalfd` for SIGTERM, SIGINT, and SIGHUP, on the same epoll. No async work in a signal handler.
- `AF_UNIX` `SOCK_SEQPACKET` for clients. One recv is one command.
- Open input devices by name through `/sys/class/input`, never by a remembered `eventN`.
- Read evdev only when a loaded flow subscribes to it. Never `EVIOCGRAB`.
- Write keyboard color through the asus-nb-wmi sysfs node. One writer.
- On a clean shutdown, put the keyboard back to the saved static color (`#ccfffe` as of 2026-09-23, read from `/etc/asusd/aura_tuf.ron` at exit rather than hardcoding it).
- Keep the binary free of libudev, libsystemd, and dbus.

## Do not

- Open `/dev/hidraw*` for the laptop keyboard. On this machine those nodes are the Logitech receiver, the G305, the touchpad, and the ITE5570 EC. The EC node races `asus-nb-wmi` and asusd.
- Stop or reconfigure asusd. Fans, charge limit, and the saved aura file stay with it. daeboard only takes the live color while it runs.
- Poll sysfs. Write it when a timer or a command says the color changed.
- Log key codes, forward the key stream, or let a socket client subscribe to raw keys. Match only the keys named in the flow (v1: Meta, Enter, Backspace, Delete). Discard every other code in the read loop.
- Exec anything. The light is the reaction.
- Touch `~/Documents/ctron` from this tree.
- Bump the kernel, or depend on `asus-armoury`. It does not load on 6.18. RGB is `asus-nb-wmi`.

## Hardware (measured 2026-09-23, kernel 6.18.52)

- LED class: `/sys/class/leds/asus::kbd_backlight/`
- `brightness` is world-readable, range 0–3, currently 3.
- `kbd_rgb_mode` and `kbd_rgb_state` are write-only, root (`--w-------`). The daemon is root for that reason.
- Mode index text: `cmd mode red green blue speed`
- State index text: `cmd boot awake sleep keyboard`
- asusd aura file: single zone (`multizone_on: false`), mode Static, colour `204 255 254`.
- Built-in keys: `AT Translated Set 2 keyboard` (i8042). Fn row: `Asus WMI hotkeys`. Lid: `Lid Switch`.
