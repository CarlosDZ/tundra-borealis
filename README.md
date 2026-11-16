# tundra-borealis

Compositor of Tundra OS, on wlroots. Its design is in ADR-036 of
[Tundra-OS](https://github.com/CarlosDZ/Tundra-OS).

## Requirements

- wlroots 0.20, wayland, xkbcommon, libinput and pixman.
- tundra-libconf. A checkout of tundra-libconf next to this one, built, is used
  instead of an installed one.

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

Still to come: the header, the pixel art of empty places, the lid, locking,
notices and captures.

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
