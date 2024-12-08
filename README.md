# EstrogenCompositor
Tiling wayland compositor using wlroots

Converting from EstrogenWM (X11 tiling wm)!

## Plans to support:
- Grid-based window tiling system
    - Resizing
    - Movement
- Floating windows
- Sloppy focus (focus follows pointer)
- Borders (woaw)
- desktop file (for login managers)
- Keybinding
- ...

## Default keybinds:
- ???: Start $TERMINAL
- ...

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
- install: files neccessary for installation of EstrogenCompositor
- compile_flags.txt: for clangd
- Makefile
- README.md
- .gitignore
