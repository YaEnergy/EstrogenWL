# EstrogenWL

Tiling wayland compositor using wlroots.

This project is mostly made to have fun & learn! :3
This already has been quite the adventure...

## Support

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
  - Gamma control
- Plans to support
  - Security context protocol
  - Shells
    - wlr\_layer\_shell\_unstable\_v1 (right now very basic)
  - xwayland
  - Configuration
    - Keybinding
    - Compositor
  - Windows
    - Fullscreen
    - Maximalization
    - Minimalization
    - Focus
    - Resizing
    - Movement & reordering
    - Borders
  - Desktop extension file for login managers
  - …
- No plans to support (yet)
  - Input devices
    - Touch
  - HiDPI
  - Multi-seat support

## References
- tinywl
- sway
- [Writing a Wayland compositor by Drew DeVault (out-of-date, but still useful)](https://drewdevault.com/2018/02/17/Writing-a-Wayland-compositor-1.html)

## Dependancies
- wlroots
- xkb

## Project structure:
- src: C source files
- include: header files
- protocols: wayland protocol xml files & generated header files
- install: files neccessary for installation of EstrogenWL
- compile_flags.txt: for clangd
- Makefile
- README.md
- .gitignore
