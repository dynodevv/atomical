/*
 * Atomical OS - PS/2 Keyboard Driver
 * IRQ1-driven scancode translation with a ring buffer.
 * MIT License - Copyright (c) 2026 Dyno
 */

#include <kernel/kernel.h>
#include <kernel/keyboard.h>
#include <hal/hal.h>

/* Ring buffer for key events (shared across architectures) */
#define KBD_BUFFER_SIZE 128
static volatile char kbd_buffer[KBD_BUFFER_SIZE];
static volatile uint32_t kbd_read_idx  = 0;
static volatile uint32_t kbd_write_idx = 0;

static char kbd_buffer_pop(void)
{
    if (kbd_read_idx == kbd_write_idx)
        return 0;
    char c = kbd_buffer[kbd_read_idx];
    kbd_read_idx = (kbd_read_idx + 1) % KBD_BUFFER_SIZE;
    return c;
}

#ifdef __x86_64__
#include <arch/x86_64/arch.h>

/* PS/2 keyboard I/O ports */
#define KBD_DATA_PORT   0x60
#define KBD_STATUS_PORT 0x64

/* Modifier key state */
static volatile bool shift_held = false;
static volatile bool caps_lock  = false;

static void kbd_buffer_push(char c)
{
    uint32_t next = (kbd_write_idx + 1) % KBD_BUFFER_SIZE;
    if (next != kbd_read_idx) {
        kbd_buffer[kbd_write_idx] = c;
        kbd_write_idx = next;
    }
}

/*
 * US QWERTY scancode set 1 -> ASCII lookup table.
 * Index = scancode, value = ASCII character (0 = no mapping).
 */
static const char scancode_to_ascii[128] = {
    0,   KEY_ESCAPE,
    '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
    '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p',
    '[', ']', '\n', 0 /* LCtrl */,
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l',
    ';', '\'', '`', 0 /* LShift */,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm',
    ',', '.', '/', 0 /* RShift */,
    '*',  0 /* LAlt */, ' ', 0 /* CapsLock */,
    0,0,0,0,0,0,0,0,0,0,       /* F1-F10 */
    0,0,                         /* NumLock, ScrollLock */
    0,0,0,'-',0,0,0,'+',0,0,0,  /* Keypad */
    0,0,                         /* unused */
    0,0,0,                       /* F11, F12, unused */
};

/* Shifted variants */
static const char scancode_to_ascii_shift[128] = {
    0,   KEY_ESCAPE,
    '!', '@', '#', '$', '%', '^', '&', '*', '(', ')',
    '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P',
    '{', '}', '\n', 0,
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L',
    ':', '"', '~', 0,
    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M',
    '<', '>', '?', 0,
    '*', 0, ' ', 0,
    0,0,0,0,0,0,0,0,0,0,
    0,0,
    0,0,0,'-',0,0,0,'+',0,0,0,
    0,0,
    0,0,0,
};

/* PS/2 keyboard IRQ handler (IRQ 1, vector 33) */
static void keyboard_irq_handler(uint32_t irq, void *context)
{
    (void)irq;
    (void)context;

    uint8_t scancode = inb(KBD_DATA_PORT);

    /* Key release (bit 7 set) */
    if (scancode & 0x80) {
        uint8_t released = scancode & 0x7F;
        if (released == 0x2A || released == 0x36) /* LShift or RShift */
            shift_held = false;
        return;
    }

    /* Modifier keys */
    if (scancode == 0x2A || scancode == 0x36) { /* LShift or RShift */
        shift_held = true;
        return;
    }

    if (scancode == 0x3A) { /* CapsLock toggle */
        caps_lock = !caps_lock;
        return;
    }

    /* Translate scancode to ASCII */
    char c;
    if (shift_held)
        c = scancode_to_ascii_shift[scancode & 0x7F];
    else
        c = scancode_to_ascii[scancode & 0x7F];

    /* Apply caps lock for letters */
    if (caps_lock && c >= 'a' && c <= 'z')
        c -= 32;
    else if (caps_lock && c >= 'A' && c <= 'Z')
        c += 32;

    if (c)
        kbd_buffer_push(c);
}

void keyboard_init(void)
{
    /* Flush any pending data */
    while (inb(KBD_STATUS_PORT) & 0x01)
        inb(KBD_DATA_PORT);

    hal_irq_register(1, keyboard_irq_handler, NULL);

    /* Unmask IRQ1 on the PIC */
    hal_irq_unmask(1);

    kprintf("[kbd]  PS/2 keyboard initialized (IRQ1)\n");
}

#else /* Non-x86_64 architectures: stub */

void keyboard_init(void)
{
    kprintf("[kbd]  Keyboard not yet supported on this architecture\n");
}

#endif /* __x86_64__ */

char keyboard_poll(void)
{
    return kbd_buffer_pop();
}

char keyboard_getchar(void)
{
    char c;
    while (!(c = kbd_buffer_pop()))
        hal_cpu_idle();
    return c;
}
