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

## Configuration

`$XDG_CONFIG_HOME/tundra/borealis/windows` (or `~/.config/tundra/borealis/windows`)
says which programs prefer a vertical place, in the format of ADR-033. A program
not listed is horizontal:

```
v = org.example.tall
h = frost
```

A wrong file is reported and ignored.

## Tests

```
make check-unit
make check
```

`check` runs the compositor without a screen (`WLR_BACKENDS=headless`), drawing
in software, and needs no privileges.
