#pragma once

// Keybinding management functions

#include <xkbcommon/xkbcommon.h>

//Returns 0 on success, returns 1 on fail
int e_keybinding_init();

//void EKeybindingDeinit();
void e_keybinding_deinit();

//Returns keybind id, returns -1 on keybind array expansion fail, returns -2 if duplicate keybind
//int EKeybindingBind(Display* display, KeyCode keyCode, int modifierMask, const char* command);

int e_keybinding_bind(xkb_keysym_t keycode, xkb_mod_mask_t mods, const char* command);

//void EKeybindingHandleInput(Display* display, KeyCode keyCode, int modifierMask);
