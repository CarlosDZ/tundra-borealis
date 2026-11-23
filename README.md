# tundra-borealis

Compositor of Tundra OS, on wlroots. Its design is in ADR-036 of
[Tundra-OS](https://github.com/CarlosDZ/Tundra-OS).

## Requirements

- wlroots 0.20, wayland, xkbcommon, libinput and pixman.
- tundra-libconf, and tundra-libdraw, which paints the header and reads the
  theme and the fonts. Built checkouts of tundra-libconf, tundra-libdraw and
  tundra-libtoa next to this one are used instead of installed ones.

## Usage

```
tundra-borealis
tundra-borealis screens [mirror | extended]
tundra-borealis --version
```

Started from a text console, it takes the screen until SIGTERM or SIGINT.
Ctrl + Alt + F1 to F12 switch to another console.

The keyboard layout comes from `XKB_DEFAULT_LAYOUT` and the like; Caps Lock is
Esc. Super + T starts `frost`, or the command in `TUNDRA_BOREALIS_TERMINAL`.

## Canvases and shortcuts

Sixteen canvases, four across by four deep; `23` is the second across, third
deep. Each one is tiled after how many of its windows prefer a horizontal or a
vertical place. The keys are found by their place on the keyboard, whatever the
layout:

| Keys | What they do |
|------|--------------|
| Super + Tab | Focus to the next window, clockwise |
| Super + Shift + Tab | The focused window one place on among those of its kind |
| Super + `h` / `l` | Canvas to the left or right |
| Super + `j` / `k` | Next or previous depth |
| Super + Shift + `h j k l` | The focused window to the canvas beside, and the view with it |
| Super + two digits | To that canvas |
| Super + Shift + two digits | The focused window to that canvas, and the view with it |
| Super + Backspace | Closes the focused window; never a program of a fixed canvas |
| Super + F | Fullscreen |
| Super + T | Terminal |
| Super + O | Focus to the other screen |
| Super + R | Horizontal or vertical for the focused window, while it lives |
| Super + M | Locks or frees the pointer |

The focus follows the pointer. Three fingers swipe from canvas to canvas: up is
the next depth. Neither axis wraps around.

Each screen has a header on top: the place on the left (`H2 P3`, second
across and third deep), the date and time in the middle (`2026-11-23  14:05`),
repainted as each minute starts. Its line is the largest size the `ui` fonts of
`/etc/tundra/fonts` draw to the pixel within 1/45 of the screen's height (24 at
1080 pixels, 48 at 2160), with a sixth of it above and below. A fullscreen
window covers it. Its colors, and those of the borders and backgrounds, come
from the theme of libdraw, `/etc/tundra/theme` and `~/.config/tundra/theme`,
each one as `borealis.name` and then `name`; a theme saved is painted at once.

On the right, the battery and the brightness, each its icon and its number,
read from `/sys` when the kernel says that something changed there, and each
minute for batteries that do not; a computer without one shows nothing. Each
icon is a drawing of the toa format with an animation for each state:

| Drawing | States |
|---------|--------|
| `battery` | `empty` up to 15 %, `low` to 35, `half` to 60, `high` to 85, `full`; `charging` |
| `brightness` | `low` up to 25 %, `half` to 50, `high` to 75, `full` |

They are installed in `/usr/share/tundra/borealis/art`, built from
`data/art/*.toas` by the `toa` command of tundra-libtoa; one of the same name
in `~/.config/tundra/borealis/art` goes instead. A drawing missing or broken
leaves its place empty and says why.

Still to come: volume and lid in the header, the pixel art of empty places,
the lid, locking, notices and captures.

## Configuration

`$XDG_CONFIG_HOME/tundra/borealis/windows`, or the same under `~/.config`,
says which programs prefer a vertical place, in the format of ADR-033. A program
not listed is horizontal:

```
v = org.example.tall
h = frost
```

A wrong file is reported and ignored.

`$XDG_CONFIG_HOME/tundra/borealis/screens` says how two screens start:

```
mode = extended
second = left
```

`mirror` shows the same canvas on both; `right` puts the second screen on the
right. `tundra-borealis screens` changes the mode of the running borealis, and
without a mode it prints the current one.

### Fixed canvases

`/etc/tundra/borealis/canvas-HD`, owned by root and writable by no one else,
makes canvas HD fixed (ADR-036, DR-126). One block per program:

```
recursos {
	command = "frost -e btop"
	place = 50 0
	size = 50 50
	level = 1
	privileges = none
}
```

`place` and `size` are whole percents of the canvas. `level` goes from 0 to 9,
0 by default; a higher one covers what lies under it, and two programs on one
level cannot overlap. `privileges` is `none` or a comma separated list of
`overlay`, `capture`, `virtual-input`, `window-control` and `lock`.

Only the program started from the file gets its privileges, not the ones it
starts in turn: any other client never sees the screen capture, the virtual
keyboard and pointer, or the other privileged globals. A program that dies is
started again; after three deaths within 10 s its place is covered. A canvas
whose file is wrong stays a working one, and at least one of the 16 always
does.

## Tests

```
make check-unit
make check
```

`check` runs the compositor without a screen (`WLR_BACKENDS=headless`), drawing
in software, and needs no privileges.
