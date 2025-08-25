# EstrogenWL

Tiling wayland compositor using wlroots.

This project is mostly made to have fun & learn! :3
This already has been quite the adventure...

## Building (using meson & ninja)

    meson setup build
    ninja -C build

## Dependencies

- wayland
- wayland-protocols
- [wlroots](https://gitlab.freedesktop.org/wlroots/wlroots)
- xkbcommon
- pixman
- libdrm
- xcb
- json-c

## Support (out-of-date, updating later...)

- Supported (atleast working)
  - Cursor
    - Default theme
    - Button
    - Frame
    - (Absolute) Motion
    - Axis (scrolling, …)
  - Input devices
    - Keyboard
    - Pointer
  - Shells
    - xdg_shell
  - Single seat
  - Windows
    - Tiling (very basic)
    - Floating (movement, resizing)
  - Gamma control
- Plans to support
  - Security context protocol
  - Shells
    - wlr\_layer\_shell\_unstable\_v1 (WIP)
  - xwayland (WIP)
  - Configuration
    - Keybinding
    - Compositor
  - Windows
    - Fullscreen
    - Maximalization
    - Minimalization
    - Resizing (tiled)
    - Movement (tiled) & reordering
    - Borders
  - Desktop extension file for login managers
  - …
- No plans to support (yet)
  - Input devices
    - Touch
  - HiDPI
  - Multi-seat support

## References
- [tinywl](https://gitlab.freedesktop.org/wlroots/wlroots/-/tree/master/tinywl?ref_type=heads)
- [sway](https://github.com/swaywm/sway)
- [labwc](https://github.com/labwc/labwc)
- [kiwmi](https://github.com/buffet/kiwmi)
- [Writing a Wayland compositor by Drew DeVault (out-of-date, but still useful)](https://drewdevault.com/2018/02/17/Writing-a-Wayland-compositor-1.html)
