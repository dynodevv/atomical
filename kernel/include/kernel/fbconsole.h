/*
 * Atomical OS - Framebuffer Text Console
 * MIT License - Copyright (c) 2026 Dyno
 */

#ifndef _KERNEL_FBCONSOLE_H
#define _KERNEL_FBCONSOLE_H

#include <kernel/types.h>
#include <hal/hal.h>

/* Initialize the framebuffer console with the given framebuffer */
void fbcon_init(hal_framebuffer_t *fb);

/* Write a single character at the current cursor position */
void fbcon_putchar(char c);

/* Write a null-terminated string */
void fbcon_puts(const char *s);

/* Clear the screen */
void fbcon_clear(void);

/* Set text foreground and background colors (0xRRGGBB) */
void fbcon_set_color(uint32_t fg, uint32_t bg);

/* Check if the framebuffer console is initialized */
bool fbcon_is_active(void);

#endif /* _KERNEL_FBCONSOLE_H */
