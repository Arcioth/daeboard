# daeboard

Status: **running**. README: `README.md`. Spec: `docs/superpowers/specs/2026-09-23-daeboard-design.md`. Unit: `daeboard.service` (transient). Stop with `sudo systemctl stop daeboard`. Socket: `/run/daeboard/daeboard.sock`. ctron steps are in `~/Documents/ctron/PLANS.md`.

Machine: NixOS, hostname `nixos`, ASUS TUF A15 FA507NVR, kernel 6.18.52. asusd is active.

## What this is

A root static-C daemon. One epoll loop. It listens for commands on a UNIX socket, for time on a timerfd, and (only if a flow asks) for input events. It writes the whole-keyboard color. Ctron is a future client, not a library and not a parent process.

## Structure

1. Paint through `/sys/class/leds/asus::kbd_backlight/kbd_rgb_mode`. The kernel already owns the EC. hidraw5 is that EC, bound to `hid-generic`, and a second writer races asusd. The other hidraw nodes are the Logitech receiver and the touchpad.
2. The keyboard is one color. `aura_tuf.ron` has `multizone_on: false`. Brightness is 0–3.
3. Firmware breathe stays on the EC. The red ramp, the fade, and the Enter blink are timerfd writes. First ramp interval to measure is 50 ms. WMI is the limit, not the loop.
4. Idle contract, same spirit as daetron: with no watched key down, epoll sleeps and the timer is disarmed. RSS target is tens to low hundreds of KB. daetron blocks in `poll` on one netlink socket. daeboard watches evdev, timerfd, and signalfd, so epoll is the same idea with more than one fd.
5. Level-triggered epoll.
6. evdev is opened by device name. Only Meta, Enter, Backspace, and Delete are compared. Every other code is dropped in the read loop. No grab, no log.
7. Socket path `/run/daeboard/daeboard.sock`, mode `0660`, group `wheel`, `SOCK_SEQPACKET`. v1 commands are `ping`, `color`, `brightness`, `quit`. Ctron links nothing from this repo.

## First build

The spec is the build. Loop, three gestures, and the four socket commands land together.

## Gestures (accepted 2026-09-23)

The built-in keyboard only (`AT Translated Set 2 keyboard`). Typematic repeats are ignored. The daemon remembers the normal color itself, because `kbd_rgb_mode` is write-only. Startup normal comes from `/etc/asusd/aura_tuf.ron` (currently static `#ccfffe`, brightness 3).

Backspace and Delete are one gesture.

| Keys | When | Light |
|---|---|---|
| Left or right Meta | held | Firmware breathe in the normal color. Release returns to static normal. |
| Enter | pressed | Two off/on blinks, each edge held 120 ms, so the pair is visible. Then the still-held gesture shows again. |
| Backspace, Delete | held | Immediately static full red at brightness 3. Release fades back to normal over 2 s. |

Firmware breathe stays on the EC at speed 2, the fastest value the kernel maps. Enter only changes brightness, one edge per timer wake. Backspace writes red once, with brightness 3 in the same turn, so the mode change does not leave the keyboard dark.

These rows are compiled into the first binary. A later ctron panel can replace them over the socket. The socket's v1 job is normal color, brightness, ping, and quit.

## Overlap (accepted 2026-09-23)

Newest key wins. Only one gesture is drawn. Meta left/right are one gesture. Backspace and Delete are one gesture.

- The newest press owns the LEDs until that gesture ends. Enter's gesture ends when both blinks have finished, which is after the key has already come up. A hold ends when its key comes up.
- A preempted gesture keeps its clock running while it is hidden. Clocks are `CLOCK_MONOTONIC` timestamps, so a hidden gesture does not arm timerfd and does not write sysfs.
- When the owner ends, the next watched key that is still down becomes the owner and is drawn at the time it has already reached. If its fade already finished while hidden, the keyboard is back to normal.
- Pressing Enter again during its own blink restarts the two blinks.

## Process shape (accepted 2026-09-23)

Locked in the spec. One root musl-static binary. epoll on signalfd, the AT keyboard, one timerfd, and a `SOCK_SEQPACKET` socket (`ping`, `color`, `brightness`, `quit`). Gestures are compiled in. cmd `0` only, so the BIOS copy of the aura color is never overwritten.

## Running

Binary `~/Documents/daeboard/daeboard`, musl static, 66960 bytes. RSS about 100 KB while idle. `make test` prints `ok`.

Restarted after the gesture timing change. Enter holds each edge 120 ms. Backspace snaps to red. Meta breathe uses speed 2. Try those three on the built-in keyboard.

`ping` returned `pong`. `color ccfffe` returned `ok`.

## Resume

- Try Meta, Enter, and Backspace on the built-in keyboard.
- Stop with `sudo systemctl stop daeboard`. It is a transient unit, gone after stop, and it is not in the NixOS config.
- Do not edit `~/Documents/ctron` or `/etc/nixos` for this.
- Do not stop asusd.
- WhatsApp replies in this session go to LID `262607789416556@lid`. The phone-number JID was accepted by the server and did not show up in the chat.
