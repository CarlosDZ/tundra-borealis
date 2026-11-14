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

## Tests

```
make check-unit
make check
```

`check` runs the compositor without a screen (`WLR_BACKENDS=headless`), drawing
in software, and needs no privileges.
