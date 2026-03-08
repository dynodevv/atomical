/*
 * Atomical OS - PS/2 Keyboard Driver
 * MIT License - Copyright (c) 2026 Dyno
 */

#ifndef _KERNEL_KEYBOARD_H
#define _KERNEL_KEYBOARD_H

#include <kernel/types.h>

/* Special key codes (values > 0x80) */
#define KEY_ESCAPE      0x1B
#define KEY_BACKSPACE   0x08
#define KEY_TAB         0x09
#define KEY_ENTER       0x0A
#define KEY_LSHIFT      0x80
#define KEY_RSHIFT      0x81
#define KEY_LCTRL       0x82
#define KEY_LALT        0x83
#define KEY_CAPSLOCK    0x84
#define KEY_F1          0x85
#define KEY_F2          0x86
#define KEY_F3          0x87
#define KEY_F4          0x88
#define KEY_F5          0x89
#define KEY_F6          0x8A
#define KEY_F7          0x8B
#define KEY_F8          0x8C
#define KEY_F9          0x8D
#define KEY_F10         0x8E
#define KEY_F11         0x8F
#define KEY_F12         0x90
#define KEY_UP          0x91
#define KEY_DOWN        0x92
#define KEY_LEFT        0x93
#define KEY_RIGHT       0x94

/* Initialize the PS/2 keyboard driver (registers IRQ1) */
void keyboard_init(void);

/* Poll for a key press (non-blocking, returns 0 if no key available) */
char keyboard_poll(void);

/* Wait for a key press (blocking) */
char keyboard_getchar(void);

#endif /* _KERNEL_KEYBOARD_H */
