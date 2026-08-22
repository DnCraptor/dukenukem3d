/*
 * Native Duke Nukem 3D setup utility for the RP2040/RP2350 DOS runtime.
 *
 * Stage 1 deliberately keeps the configuration pages as placeholders.  The
 * executable itself, original-style main menu, CFG load/create/save path and
 * DOS/BIOS integration are functional; later stages fill in the individual
 * setup pages without changing this shell.
 */

#include "TYPES.H"
#include "SCRIPLIB.H"

#include <conio.h>
#include <dos.h>
#include <stdio.h>
#include <string.h>

#define MOUSE_LEFT 1u

#define SETUP_CFG "DUKE3D.CFG"
#define SCREEN_COLS 80
#define SCREEN_ROWS 25

#define ATTR_DESKTOP 0x70
#define ATTR_HEADER  0x70
#define ATTR_WINDOW  0x1f
#define ATTR_BORDER  0x1b
#define ATTR_SELECT  0x70
#define ATTR_SHADOW  0x00

#define KEY_ESC       27
#define KEY_ENTER     13
#define KEY_EXTENDED   0
#define SCAN_UP     0x48
#define SCAN_DOWN   0x50

static unsigned char visible_page = 0;
static unsigned char draw_page = 1;
static int current_selection = 0;
static int cursor_shape_saved = 0;
static unsigned char saved_cursor_start = 0;
static unsigned char saved_cursor_end = 0;
static int mouse_present = 0;
static int mouse_visible = 0;
static unsigned mouse_buttons = 0;

static void wait_for_ack(void);

static const char *const menu_items[] = {
    "Sound Setup",
    "Screen Setup",
    "Controller Setup",
    "Network Game",
    "Modem Game",
    "Serial Game",
    "See The Duke Nukem 3D Help File",
    "User Level Selection",
    "Change Setup Filename",
    "Save and launch Duke Nukem 3D"
};

static const char *const menu_help[] = {
    "Setup sound and music for Duke Nukem 3D",
    "Setup screen modes for Duke Nukem 3D",
    "Setup controls. Change button and key assignments.",
    "Play a network game of Duke Nukem 3D",
    "Play a modem game of Duke Nukem 3D",
    "Play a serial game of Duke Nukem 3D",
    "Browse the help file for Duke Nukem 3D",
    "Select a user level to launch",
    "Changes the filename of the SETUP configuration file",
    "Save the current setup and play Duke Nukem 3D"
};

#define MENU_COUNT ((int)(sizeof(menu_items) / sizeof(menu_items[0])))

static void video_mode3(void)
{
    union REGS regs;
    memset(&regs, 0, sizeof(regs));
    regs.w.ax = 0x0003;
    int386(0x10, &regs, &regs);
    visible_page = 0;
    draw_page = 1;
}


static void bios_save_and_hide_cursor(void)
{
    union REGS regs;

    memset(&regs, 0, sizeof(regs));
    regs.h.ah = 0x03;
    regs.h.bh = visible_page;
    int386(0x10, &regs, &regs);
    saved_cursor_start = regs.h.ch;
    saved_cursor_end = regs.h.cl;
    cursor_shape_saved = 1;

    memset(&regs, 0, sizeof(regs));
    regs.h.ah = 0x01;
    regs.h.ch = 0x20;
    regs.h.cl = saved_cursor_end;
    int386(0x10, &regs, &regs);
}

static void bios_restore_cursor(void)
{
    union REGS regs;

    if (!cursor_shape_saved)
        return;

    memset(&regs, 0, sizeof(regs));
    regs.h.ah = 0x01;
    regs.h.ch = saved_cursor_start;
    regs.h.cl = saved_cursor_end;
    int386(0x10, &regs, &regs);
}

static int mouse_reset(void)
{
    union REGS in, out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    in.w.ax = 0;
    int386(0x33, &in, &out);
    mouse_buttons = 0;
    return out.w.ax != 0;
}

static void mouse_show(void)
{
    union REGS in, out;

    if (!mouse_present || mouse_visible)
        return;
    memset(&in, 0, sizeof(in));
    in.w.ax = 1;
    int386(0x33, &in, &out);
    mouse_visible = 1;
}

static void mouse_hide(void)
{
    union REGS in, out;

    if (!mouse_present || !mouse_visible)
        return;
    memset(&in, 0, sizeof(in));
    in.w.ax = 2;
    int386(0x33, &in, &out);
    mouse_visible = 0;
}

static unsigned mouse_state(int *x, int *y)
{
    union REGS in, out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    in.w.ax = 3;
    int386(0x33, &in, &out);
    if (x) *x = (int)out.w.cx;
    if (y) *y = (int)out.w.dx;
    return (unsigned)(out.w.bx & 7u);
}

void _fini(void *ctx)
{
    union REGS regs;
    (void)ctx;

    mouse_hide();

    memset(&regs, 0, sizeof(regs));
    regs.h.ah = 0x05;
    regs.h.al = 0;
    int386(0x10, &regs, &regs);

    bios_restore_cursor();
}

static void bios_set_cursor(unsigned char page, int x, int y)
{
    union REGS regs;
    memset(&regs, 0, sizeof(regs));
    regs.h.ah = 0x02;
    regs.h.bh = page;
    regs.h.dh = (unsigned char)y;
    regs.h.dl = (unsigned char)x;
    int386(0x10, &regs, &regs);
}

static void bios_write_repeat(int x, int y, unsigned char ch,
                              unsigned char attr, int count)
{
    union REGS regs;

    if (count <= 0 || (unsigned)x >= SCREEN_COLS ||
        (unsigned)y >= SCREEN_ROWS)
        return;
    if (x + count > SCREEN_COLS)
        count = SCREEN_COLS - x;

    bios_set_cursor(draw_page, x, y);
    memset(&regs, 0, sizeof(regs));
    regs.h.ah = 0x09;
    regs.h.al = ch;
    regs.h.bh = draw_page;
    regs.h.bl = attr;
    regs.w.cx = (uint16_t)count;
    int386(0x10, &regs, &regs);
}

static void present_page(void)
{
    union REGS regs;
    unsigned char old_visible = visible_page;

    mouse_hide();

    memset(&regs, 0, sizeof(regs));
    regs.h.ah = 0x05;
    regs.h.al = draw_page;
    int386(0x10, &regs, &regs);

    visible_page = draw_page;
    draw_page = old_visible;
    mouse_show();
}

static void cell(int x, int y, unsigned char ch, unsigned char attr)
{
    bios_write_repeat(x, y, ch, attr, 1);
}

static void fill(int x, int y, int w, int h, unsigned char ch, unsigned char attr)
{
    int yy;
    for (yy = 0; yy < h; ++yy)
        bios_write_repeat(x, y + yy, ch, attr, w);
}

static void text(int x, int y, const char *s, unsigned char attr)
{
    while (*s && x < SCREEN_COLS)
        cell(x++, y, (unsigned char)*s++, attr);
}

static void text_center(int x, int y, int w, const char *s, unsigned char attr)
{
    int n = (int)strlen(s);
    int at = x + (w - n) / 2;
    if (at < x) at = x;
    text(at, y, s, attr);
}

static void hline(int x, int y, int w, unsigned char attr)
{
    int i;
    cell(x, y, 0xc3, attr);
    for (i = 1; i < w - 1; ++i) cell(x + i, y, 0xc4, attr);
    cell(x + w - 1, y, 0xb4, attr);
}

static void box(int x, int y, int w, int h)
{
    int i;

    fill(x + 2, y + 1, w, h, ' ', ATTR_SHADOW);
    fill(x, y, w, h, ' ', ATTR_WINDOW);

    cell(x, y, 0xda, ATTR_BORDER);
    cell(x + w - 1, y, 0xbf, ATTR_BORDER);
    cell(x, y + h - 1, 0xc0, ATTR_BORDER);
    cell(x + w - 1, y + h - 1, 0xd9, ATTR_BORDER);
    for (i = 1; i < w - 1; ++i) {
        cell(x + i, y, 0xc4, ATTR_BORDER);
        cell(x + i, y + h - 1, 0xc4, ATTR_BORDER);
    }
    for (i = 1; i < h - 1; ++i) {
        cell(x, y + i, 0xb3, ATTR_BORDER);
        cell(x + w - 1, y + i, 0xb3, ATTR_BORDER);
    }
}

static void desktop(void)
{
    int y;
    fill(0, 0, SCREEN_COLS, SCREEN_ROWS, ' ', ATTR_DESKTOP);

    text(1, 0, "Duke Nukem 3D Atomic Edition Setup Version 1.4", ATTR_HEADER);
    text(48, 0, "(c) 1996 3D Realms Entertainment", ATTR_HEADER);

    /* The original setup uses a stippled grey desktop behind its windows. */
    for (y = 1; y < SCREEN_ROWS - 1; ++y)
        fill(0, y, SCREEN_COLS, 1, (y & 1) ? 0xb1 : 0xb0, 0x08);

    fill(0, SCREEN_ROWS - 1, SCREEN_COLS, 1, ' ', ATTR_HEADER);
}

static void draw_main_menu_content(int selected)
{
    const int x = 17;
    const int y = 4;
    const int w = 46;
    const int h = 18;
    int i;

    desktop();
    box(x, y, w, h);
    text_center(x + 1, y + 1, w - 2, "Main Menu", ATTR_WINDOW);
    hline(x, y + 2, w, ATTR_BORDER);
    text_center(x + 1, y + 3, w - 2,
                "Current Filename ( DUKE3D.CFG )", ATTR_WINDOW);
    hline(x, y + 4, w, ATTR_BORDER);

    for (i = 0; i < MENU_COUNT; ++i) {
        int row = y + 5 + i;
        fill(x + 2, row, w - 4, 1, ' ',
             i == selected ? ATTR_SELECT : ATTR_WINDOW);
        text(x + 3, row, menu_items[i],
             i == selected ? ATTR_SELECT : ATTR_WINDOW);
    }

    hline(x, y + h - 3, w, ATTR_BORDER);
    text(x + 4, y + h - 2, "Esc Exits   \x18\x19 Move   \x1b\x1a Selects", ATTR_WINDOW);

    fill(0, SCREEN_ROWS - 1, SCREEN_COLS, 1, ' ', ATTR_HEADER);
    text_center(0, SCREEN_ROWS - 1, SCREEN_COLS, menu_help[selected], ATTR_HEADER);
}

static void draw_main_menu(int selected)
{
    current_selection = selected;
    draw_main_menu_content(selected);
    present_page();
}

static void message_box(const char *line1, const char *line2)
{
    const int x = 20;
    const int y = 9;
    const int w = 40;
    const int h = 7;

    draw_main_menu_content(current_selection);
    box(x, y, w, h);
    text_center(x + 1, y + 2, w - 2, line1, ATTR_WINDOW);
    text_center(x + 1, y + 3, w - 2, line2, ATTR_WINDOW);
    text_center(x + 1, y + 5, w - 2, "Press any key to continue", ATTR_BORDER);
    present_page();
    wait_for_ack();
}

static int read_key(void)
{
    int ch = getch();
    if (ch == KEY_EXTENDED)
        return 0x100 | getch();
    return ch;
}

static int menu_item_at_mouse(int mx, int my)
{
    const int x = 17;
    const int y = 4;
    const int w = 46;
    int col = mx >> 3;
    int row = my >> 3;
    int item;

    if (col < x + 2 || col >= x + w - 2)
        return -1;

    item = row - (y + 5);
    if (item < 0 || item >= MENU_COUNT)
        return -1;
    return item;
}

static int read_menu_input(int *selected)
{
    for (;;) {
        if (kbhit())
            return read_key();

        if (mouse_present) {
            int mx, my;
            unsigned buttons = mouse_state(&mx, &my);
            int item = menu_item_at_mouse(mx, my);

            if (item >= 0 && item != *selected) {
                *selected = item;
                draw_main_menu(*selected);
            }

            if ((buttons & MOUSE_LEFT) && !(mouse_buttons & MOUSE_LEFT)) {
                mouse_buttons = buttons;
                if (item >= 0)
                    return KEY_ENTER;
            } else {
                mouse_buttons = buttons;
            }
        }
    }
}

static void wait_for_ack(void)
{
    unsigned old_buttons = mouse_buttons;

    for (;;) {
        if (kbhit()) {
            (void)read_key();
            return;
        }

        if (mouse_present) {
            int mx, my;
            unsigned buttons = mouse_state(&mx, &my);
            (void)mx;
            (void)my;
            mouse_buttons = buttons;
            if ((buttons & MOUSE_LEFT) && !(old_buttons & MOUSE_LEFT))
                return;
            old_buttons = buttons;
        }
    }
}

static int load_or_create_cfg(void)
{
    int32 handle = SCRIPT_Load(SETUP_CFG);
    if (handle >= 0)
        return (int)handle;

    handle = SCRIPT_Init(SETUP_CFG);
    if (handle < 0)
        return -1;

    /* CONFIG_ReadSetup() applies game defaults for absent entries; an empty
       script is therefore a valid starting configuration. */
    SCRIPT_Save(handle, SETUP_CFG);
    return (int)handle;
}

int main(void)
{
    int handle;
    int selected = 0;
    int done = 0;
    int dirty = 0;

    video_mode3();
    bios_save_and_hide_cursor();
    mouse_present = mouse_reset();
    if (mouse_present) {
        int mx, my;
        mouse_buttons = mouse_state(&mx, &my);
    }
    handle = load_or_create_cfg();
    if (handle < 0) {
        desktop();
        message_box("Unable to create DUKE3D.CFG", "SETUP cannot continue.");
        video_mode3();
        return 1;
    }

    while (!done) {
        int key;
        draw_main_menu(selected);
        key = read_menu_input(&selected);

        if (key == KEY_ESC) {
            done = 1;
        } else if (key == (0x100 | SCAN_UP)) {
            if (--selected < 0) selected = MENU_COUNT - 1;
        } else if (key == (0x100 | SCAN_DOWN)) {
            if (++selected >= MENU_COUNT) selected = 0;
        } else if (key == KEY_ENTER) {
            if (selected == MENU_COUNT - 1) {
                SCRIPT_Save(handle, SETUP_CFG);
                dirty = 0;
                message_box("Configuration saved.",
                            "Game launch is added in stage 4.");
            } else {
                message_box("This setup page is not implemented yet.",
                            "It will be added in the next stage.");
            }
        }
    }

    if (dirty)
        SCRIPT_Save(handle, SETUP_CFG);
    SCRIPT_Free(handle);
    video_mode3();
    return 0;
}
