#include "kernel.h"
#include "limine.h"

struct vec2 {
    int x;
    int y;
};

struct window {
    int x;
    int y;
    int w;
    int h;
    uint32_t color;
    const char *title;
};

static volatile struct limine_framebuffer_request fb_req = {
    .id = {
        LIMINE_COMMON_MAGIC1,
        LIMINE_COMMON_MAGIC2,
        LIMINE_FRAMEBUFFER_MAGIC1,
        LIMINE_FRAMEBUFFER_MAGIC2,
    },
    .revision = 0,
    .response = 0,
};

static uint32_t *front;
static uint32_t *back;
static uint64_t fb_width;
static uint64_t fb_height;
static uint64_t fb_pitch;

#define MAX_BACKBUFFER_BYTES (1920u * 1080u * 4u)
static uint8_t backbuf_storage[MAX_BACKBUFFER_BYTES];

static inline void put_px(int x, int y, uint32_t c) {
    if (x < 0 || y < 0 || (uint64_t)x >= fb_width || (uint64_t)y >= fb_height) {
        return;
    }
    back[(fb_pitch / 4) * (uint64_t)y + (uint64_t)x] = c;
}

static void fill_rect(int x, int y, int w, int h, uint32_t c) {
    for (int yy = y; yy < y + h; yy++) {
        for (int xx = x; xx < x + w; xx++) {
            put_px(xx, yy, c);
        }
    }
}

static uint32_t blend(uint32_t fg, uint32_t bg, uint8_t a) {
    uint8_t fr = (fg >> 16) & 0xff;
    uint8_t fg_g = (fg >> 8) & 0xff;
    uint8_t fb = fg & 0xff;
    uint8_t br = (bg >> 16) & 0xff;
    uint8_t bg_g = (bg >> 8) & 0xff;
    uint8_t bb = bg & 0xff;

    uint8_t r = (uint8_t)((fr * a + br * (255 - a)) / 255);
    uint8_t g = (uint8_t)((fg_g * a + bg_g * (255 - a)) / 255);
    uint8_t b = (uint8_t)((fb * a + bb * (255 - a)) / 255);
    return (r << 16) | (g << 8) | b;
}

static void shadow_rect(int x, int y, int w, int h) {
    for (int yy = y; yy < y + h; yy++) {
        for (int xx = x; xx < x + w; xx++) {
            if (xx < 0 || yy < 0 || (uint64_t)xx >= fb_width || (uint64_t)yy >= fb_height) {
                continue;
            }
            uint64_t off = (fb_pitch / 4) * (uint64_t)yy + (uint64_t)xx;
            back[off] = blend(0x000000, back[off], 70);
        }
    }
}

static void draw_metal_titlebar(int x, int y, int w) {
    for (int i = 0; i < 24; i++) {
        uint32_t shade = 180 + (uint32_t)(i * 2);
        uint32_t c = (shade << 16) | (shade << 8) | shade;
        fill_rect(x, y + i, w, 1, c);
    }
}

static void draw_window(struct window wnd) {
    shadow_rect(wnd.x + 6, wnd.y + 8, wnd.w, wnd.h);
    fill_rect(wnd.x, wnd.y, wnd.w, wnd.h, wnd.color);
    draw_metal_titlebar(wnd.x, wnd.y, wnd.w);
}

static void draw_wallpaper(void) {
    for (uint64_t y = 0; y < fb_height; y++) {
        uint32_t b = 60 + (uint32_t)((140 * y) / (fb_height ? fb_height : 1));
        uint32_t c = (10 << 16) | (40 << 8) | b;
        for (uint64_t x = 0; x < fb_width; x++) {
            back[(fb_pitch / 4) * y + x] = c;
        }
    }
}

static void draw_topbar(void) {
    fill_rect(0, 0, (int)fb_width, 28, 0xe6e6e6);
    fill_rect(0, 27, (int)fb_width, 1, 0xbcbcbc);
}

static void draw_dock(void) {
    int dw = 420;
    int dh = 72;
    int dx = (int)((fb_width - (uint64_t)dw) / 2);
    int dy = (int)fb_height - dh - 14;
    fill_rect(dx, dy, dw, dh, 0xffffff);
    fill_rect(dx, dy, dw, 2, 0xd0d0d0);
    for (int i = 0; i < 5; i++) {
        fill_rect(dx + 26 + i * 78, dy + 12, 54, 54, 0x5e8dd6);
    }
}

static void draw_cursor(struct vec2 p) {
    for (int i = 0; i < 14; i++) {
        put_px(p.x, p.y + i, 0xffffff);
        put_px(p.x + 1, p.y + i, 0x000000);
    }
    for (int i = 0; i < 10; i++) {
        put_px(p.x + i, p.y + i, 0xffffff);
        put_px(p.x + i + 1, p.y + i, 0x000000);
    }
}

static void memcpy32(uint32_t *dst, const uint32_t *src, uint64_t count) {
    for (uint64_t i = 0; i < count; i++) {
        dst[i] = src[i];
    }
}

static void draw_apps(void) {
    struct window terminal = {.x = 60, .y = 72, .w = 520, .h = 300, .color = 0x111111, .title = "Terminal"};
    struct window calc = {.x = 620, .y = 90, .w = 280, .h = 320, .color = 0x22262a, .title = "Calculator"};
    struct window clock = {.x = 960, .y = 140, .w = 220, .h = 150, .color = 0x1e2632, .title = "Clock"};

    draw_window(terminal);
    draw_window(calc);
    draw_window(clock);

    fill_rect(terminal.x + 20, terminal.y + 40, terminal.w - 40, 24, 0x0d7a2f);
    fill_rect(calc.x + 20, calc.y + 44, calc.w - 40, 36, 0xf0f0f0);
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            fill_rect(calc.x + 20 + c * 58, calc.y + 100 + r * 50, 48, 40, 0xbfc6cf);
        }
    }

    fill_rect(clock.x + 35, clock.y + 56, 150, 60, 0xeef3ff);
}

void kernel_main(void) {
    gdt_init();
    idt_init();
    pic_init();
    pmm_init();
    paging_init();
    acpi_init();
    ps2_keyboard_init();
    ps2_mouse_init();

    if (!fb_req.response || fb_req.response->framebuffer_count == 0) {
        hlt_forever();
    }

    struct limine_framebuffer *fb = fb_req.response->framebuffers[0];
    front = (uint32_t *)fb->address;
    fb_width = fb->width;
    fb_height = fb->height;
    fb_pitch = fb->pitch;

    uint64_t needed = fb_pitch * fb_height;
    if (needed > MAX_BACKBUFFER_BYTES) {
        hlt_forever();
    }

    back = (uint32_t *)backbuf_storage;

    draw_wallpaper();
    draw_topbar();
    draw_dock();
    draw_apps();
    draw_cursor((struct vec2){.x = (int)(fb_width / 2), .y = (int)(fb_height / 2)});

    memcpy32(front, back, (fb_pitch / 4) * fb_height);

    hlt_forever();
}
