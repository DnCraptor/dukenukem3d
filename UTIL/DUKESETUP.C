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
#include <dos_phys.h>
#include <stdio.h>
#include <string.h>

#define SETUP_CFG "DUKE3D.CFG"
#define SCREEN_COLS 80
#define SCREEN_ROWS 25
#define TEXT_BASE 0xb8000u

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
}

static void cell(int x, int y, unsigned char ch, unsigned char attr)
{
    uint32_t addr;
    if ((unsigned)x >= SCREEN_COLS || (unsigned)y >= SCREEN_ROWS)
        return;
    addr = TEXT_BASE + (uint32_t)((y * SCREEN_COLS + x) * 2);
    dos_phys_write16(addr, (uint16_t)ch | ((uint16_t)attr << 8));
}

static void fill(int x, int y, int w, int h, unsigned char ch, unsigned char attr)
{
    int xx, yy;
    for (yy = 0; yy < h; ++yy)
        for (xx = 0; xx < w; ++xx)
            cell(x + xx, y + yy, ch, attr);
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

static void draw_main_menu(int selected)
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

static void message_box(const char *line1, const char *line2)
{
    const int x = 20;
    const int y = 9;
    const int w = 40;
    const int h = 7;

    box(x, y, w, h);
    text_center(x + 1, y + 2, w - 2, line1, ATTR_WINDOW);
    text_center(x + 1, y + 3, w - 2, line2, ATTR_WINDOW);
    text_center(x + 1, y + 5, w - 2, "Press any key to continue", ATTR_BORDER);
    (void)getch();
}

static int read_key(void)
{
    int ch = getch();
    if (ch == KEY_EXTENDED)
        return 0x100 | getch();
    return ch;
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
        key = read_key();

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
