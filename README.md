# tundra-borealis

Compositor of Tundra OS, on wlroots. Its design is in ADR-036 of
[Tundra-OS](https://github.com/CarlosDZ/Tundra-OS).

## Requirements

- wlroots 0.20, wayland, xkbcommon, libinput and pixman.

## Usage

```
tundra-borealis
tundra-borealis --version
```

Started from a text console, it takes the screen until SIGTERM or SIGINT.
Ctrl + Alt + F1 to F12 switch to another console.

The keyboard layout comes from `XKB_DEFAULT_LAYOUT` and the like; Caps Lock is
Esc. Super + T starts `frost`, or the command in `TUNDRA_BOREALIS_TERMINAL`.

## Configuration

`$XDG_CONFIG_HOME/tundra/borealis/windows` (or `~/.config/tundra/borealis/windows`)
says which programs prefer a vertical place, in the format of ADR-033. A program
not listed is horizontal:

```
v = org.example.tall
h = frost
```

A wrong file is reported and ignored.

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
`overlay`, `capture`, `virtual-input`, `window-control` and `lock`. A program
that dies is started again; after three deaths within 10 s its place is
covered. A canvas whose file is wrong stays a working one, and at least one of
the 16 always does.

## Tests

```
make check-unit
make check
```

`check` runs the compositor without a screen (`WLR_BACKENDS=headless`), drawing
in software, and needs no privileges.
