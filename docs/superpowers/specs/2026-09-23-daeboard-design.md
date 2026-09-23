# daeboard

Date: 2026-09-23. Machine: NixOS, ASUS TUF A15 FA507NVR, kernel 6.18.52.

One root static C11 daemon paints the whole keyboard from three key gestures. A UNIX socket accepts a few live commands. Ctron is a later client of that socket and is not part of this build.

## Hardware

The backlight is one zone. `/etc/asusd/aura_tuf.ron` has `multizone_on: false`. There is no per-key color.

Writes go to `/sys/class/leds/asus::kbd_backlight/`:

- `brightness` — integer 0–3. Readable by anyone, writable by root.
- `kbd_rgb_mode` — write-only, root. Six decimal integers: `cmd mode red green blue speed`. This matches `kbd_rgb_mode_store` in the current asus-wmi driver: cmd `0` sets the live color (`0xb3`), cmd `1` saves to the BIOS (`0xb4`). Speed `0`, `1`, `2` become `0xe1`, `0xeb`, `0xf5`. Any other speed becomes medium. Mode `9` and every mode above `11` are rewritten to `10` by the kernel.

daeboard always sends cmd `0`. It never saves to the BIOS. Static is mode `0`. Breathe is mode `1`. Channels are clamped to 0–255 before the write.

The built-in keyboard is the input device whose name is `AT Translated Set 2 keyboard`. It is opened by walking `/sys/class/input`, not by a remembered `eventN`. The fd is not grabbed.

asusd stays running. daeboard does not talk to it and does not open `/dev/hidraw*`.

## Process

musl `-static` by default, same flags as daetron: `-O2 -pipe -Wall -Wextra -std=c11 -s`. `make STATIC=0` is glibc.

`epoll_wait` is the only blocking call. Level-triggered. The fds are:

- `signalfd` for SIGINT, SIGTERM, SIGHUP. The handlers are blocked. Nothing runs in a signal handler.
- the keyboard evdev fd
- one `timerfd` on `CLOCK_MONOTONIC`
- the `AF_UNIX` `SOCK_SEQPACKET` listener
- up to four accepted clients

A lock file `/run/daeboard/daeboard.lock` is held with `flock`. A second start prints one line to stderr and exits 1.

## Normal color

`normal` is the color and brightness gestures return to. At start it is read from `/etc/asusd/aura_tuf.ron`: the first `colour1` block's `r`, `g`, `b`, and the first `brightness:` word. `Off` `Low` `Med` `High` are 0, 1, 2, 3. If the file cannot be parsed, normal is `204 255 254` and the brightness currently in sysfs.

Startup does not write the LED. The keyboard is already showing that color.

SIGHUP re-reads normal from the ron file and does not interrupt a gesture. A fade keeps the normal it snapshotted when the key was released.

SIGTERM and SIGINT write static normal, unlink the socket, and exit 0.

## Gestures

Watched codes, and only these. Every other event is discarded in the read loop. Value `2` (typematic repeat) is ignored. Value `1` is down, value `0` is up.

| Gesture | Keys | While it owns the LED |
|---|---|---|
| meta | `KEY_LEFTMETA`, `KEY_RIGHTMETA` | One write: mode breathe, normal color, speed 2, plus brightness at the normal level. No timer. Release ends it. |
| enter | `KEY_ENTER` | Four brightness edges, 120 ms apart: `0`, normal, `0`, normal. No mode write. A repeat during the blink starts the four edges over. |
| erase | `KEY_BACKSPACE`, `KEY_DELETE` | While held, one static write of `255 0 0` and brightness 3. Release fades from that red back to the normal snapshotted at release, across 2 seconds. |

Left and right Meta are one gesture. Backspace and Delete are one gesture. The second of the pair going down does not restart the clock.

Clocks are monotonic timestamps. A gesture that is not the owner does not arm the timer and does not write sysfs. Its clock still advances. When it becomes owner, the frame is computed from `now` minus its start.

Newest press owns the LED:

- An active gesture has a start time: the moment its key went down. Enter's start time updates when the blink restarts.
- The owner is the active gesture with the greatest start time.
- Erase stays active through its 2 second fade. If the fade finishes while hidden, erase drops out. If nothing else is active, the next visible state is static normal.
- Meta's breathe write happens on the transition into ownership, not on a timer. The next owner's first frame replaces the mode. Enter, while it owns, touches only brightness, so a breathe it preempted is still the firmware mode when Enter finishes and Meta is still down.

Enter arms the timer at 120 ms per edge while it owns the LED. The fade arms it at 100 ms. A held erase does not arm the timer. When a fade reaches its end, that gesture ends; if no other gesture is active, the transition to idle writes static normal once. Every mode write is followed by a brightness write in the same turn.

`u` for the hold is `clamp((now - down_since) / 5s, 0, 1)`. `u` for the fade is `clamp((now - released_at) / 2s, 0, 1)`.

## Socket

`/run/daeboard/daeboard.sock`, mode `0660`, owner root, group `wheel`. The directory `/run/daeboard` is `0755`. `SOCK_SEQPACKET`: one recv is one command. Limit 64 bytes. A 5th client is accepted and closed.

Trailing `CR`/`LF` is stripped. Replies are one datagram.

| Command | Effect | Reply |
|---|---|---|
| `ping` | none | `pong` |
| `color RRGGBB` | sets normal rgb; if no gesture is active, writes static normal | `ok` |
| `brightness N` | `N` is 0–3; sets normal brightness; writes it if no gesture is active | `ok` |
| `quit` | same path as SIGTERM | `ok` |

Anything else replies `err`. A bad command does not drop the client. The socket cannot run a program, cannot read key state, and cannot change the gesture table.

## Failures

- A short or failed sysfs write keeps the clock. The next frame retries. The process does not exit. One line goes to stderr for that streak of failures, not one line per tick.
- evdev `ENODEV` or a hangup removes the fd. Watched keys are treated as released at that instant, so a held erase begins its fade.
- One timerfd serves both jobs. Enter, while it owns the LED, sets it to 120 ms. A fade sets it to 100 ms. Each of those ticks also retries the open when the keyboard fd is down. While the fd is down and no gesture wants a frame, the timer is 1 second and only retries the open.
- The lock is busy: exit 1.

## Modules

- `src/main.c` — epoll, signalfd, timerfd, lock
- `src/input.c` — open by name, read, filter
- `src/led.c` — brightness and `kbd_rgb_mode` writes
- `src/ron.c` — normal color from `aura_tuf.ron`
- `src/gesture.c` — pure step: keys plus timestamps in, owner and frame out
- `src/sock.c` — bind, accept, the four commands

`gesture.c` does not open fds and does not include the LED writer. `tests/gesture_test.c` links it alone.

## Tests

`make test` runs the gesture tests with no root and no hardware. Cases:

- Meta down, then up, returns to idle. Meta's breathe write uses speed 2. Enter during Meta: each edge is one brightness write 120 ms apart, then Meta owns again with no time gap in Meta's start.
- Erase on press writes static `255 0 0` and brightness 3, with the timer disarmed. After Meta preempts and releases, the held erase is still that red.
- Erase released while Meta owns: the fade clock runs hidden. Meta released after the 2 second fade: the result is idle, not a fade frame.
- Enter during its own blink: the four edges start over from off.
- A non-watched code changes nothing. A repeat (`value 2`) changes nothing.
- Backspace down, then Delete down, does not reset `down_since`.

On the laptop, one `sudo` pass, then restore:

1. `0 0 204 255 254 1` shows static ice.
2. `0 1 204 255 254 1` breathes. If it does not, stop. The breathe constant is wrong for this EC and the gesture code does not land on top of a bad mode number.
3. Hold Meta, tap Enter, hold Backspace, release it, and confirm the timings above.
4. SIGTERM leaves static normal.

## Out of scope

Flow files, a ctron panel, exec of any kind, hidraw, per-key color, an NixOS module, stopping asusd, forwarding key codes.
