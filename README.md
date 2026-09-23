# daeboard

Root daemon that paints the whole ASUS TUF keyboard from three keys. One static C11 binary. It waits on `epoll` for the built-in keyboard, a timer, signals, and a UNIX socket.

Hold Meta and the backlight breathes. Tap Enter and it blinks off and on twice. Hold Backspace or Delete and it turns red. The socket can set the color it returns to.

## Status

As of 2026-09-23, on the FA507NVR (kernel 6.18.52):

- The daemon runs. Gesture tests pass. The socket answers `ping` and `color`.
- Enter holds each blink edge for 120 ms. Backspace snaps to full red. Meta breathe uses the kernel's fast speed.
- It is a transient unit, `daeboard.service`. It is not part of the NixOS config. asusd keeps running.
- A ctron client is not in this repo. The steps live in ctron's `PLANS.md`.

## Requirements

Linux with the `asus-nb-wmi` keyboard LED at `/sys/class/leds/asus::kbd_backlight/`. `kbd_rgb_mode` is root write-only. The keyboard on the FA507NVR is one color, not per key.

Build wants `gcc`, `make`, and `musl-gcc` for the static binary. `make STATIC=0` uses glibc.

The socket is mode `0660`, group `wheel`. A client has to be in that group.

## Run

From this directory:

```bash
nix-shell -p gcc gnumake --run 'make test'
nix-shell -p gnumake musl --run make
sudo systemd-run --unit=daeboard --collect \
  --description='daeboard keyboard lights' \
  "$PWD/daeboard"
```

Stop it with `sudo systemctl stop daeboard`. On the way out it writes the normal static color back. It never sends the BIOS-save command.

```bash
python3 -c 'import socket; s=socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET); s.connect("/run/daeboard/daeboard.sock"); s.send(b"ping"); print(s.recv(16))'
```

## Keys

Built-in keyboard only (`AT Translated Set 2 keyboard`). Other keys are ignored. Key-repeat is ignored.

| Keys | While held or tapped |
|---|---|
| Left or right Meta | Firmware breathe, fast. Release returns to the static color. |
| Enter | Off, on, off, on. Each step stays 120 ms. |
| Backspace, Delete | Full red at brightness 3, immediately. Release fades back over 2 seconds. |

The newest of those keys owns the light. A hidden one keeps its clock.

## Socket

`/run/daeboard/daeboard.sock`, `SOCK_SEQPACKET`. One datagram, 64 bytes, is one command.

| Command | Reply |
|---|---|
| `ping` | `pong` |
| `color RRGGBB` | `ok` |
| `brightness N` | `ok` (`N` is 0–3) |
| `quit` | `ok`, then the daemon exits |

Anything else replies `err`. The socket cannot run a program or read key codes.

## Docs

README this file. `AGENTS.md` is for agents. `HANDOFF.md` is session state. The design is `docs/superpowers/specs/2026-09-23-daeboard-design.md`.
