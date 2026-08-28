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
#include "SNDCARDS.H"
#include "KEYBOARD.H"
#include "CONTROL.H"
#include "FUNCTION.H"
#define __SETUP__
#include "_FUNCTIO.H"
#include "dos_mem.h"
#include "dos_guest_ptr.h"
#include "dos_yield.h"

#include <conio.h>
#include <dos.h>
#include <stdio.h>
#include <string.h>

#define MOUSE_LEFT 1u
#define SETUP_DSS_RATE 7000

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
static int mouse_last_x = -1;
static int mouse_last_y = -1;

static void wait_for_ack(void);

static const char *const menu_items[] = {
    "Sound Setup",
    "Screen Setup",
    "Game Setup",
    "Controller Setup",
    "Exit"
};

static const char *const menu_help[] = {
    "Setup sound and music for Duke Nukem 3D",
    "Setup screen modes for Duke Nukem 3D",
    "Setup gameplay preferences for Duke Nukem 3D",
    "Setup controls. Change button and key assignments.",
    "Save the current setup and exit"
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

static void mouse_set_sensitivity(void)
{
    union REGS in, out;

    if (!mouse_present)
        return;

    /* MS Mouse function 0Fh: mickeys per 8 pixels.  32/64 gives one
       quarter of the usual 8/16 text-mode pointer motion. */
    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    in.w.ax = 0x000f;
    in.w.cx = 32;
    in.w.dx = 64;
    int386(0x33, &in, &out);
}

static int mouse_left_pressed(void)
{
    union REGS in, out;

    if (!mouse_present)
        return 0;

    /* MS Mouse function 05h returns presses accumulated since the previous
       query for this button, so a short click cannot be lost between polls. */
    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    in.w.ax = 0x0005;
    in.w.bx = 0;
    int386(0x33, &in, &out);
    return out.w.bx != 0;
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

static void bios_write_repeat_page(unsigned char page, int x, int y,
                                   unsigned char ch, unsigned char attr,
                                   int count)
{
    union REGS regs;

    if (count <= 0 || (unsigned)x >= SCREEN_COLS ||
        (unsigned)y >= SCREEN_ROWS)
        return;
    if (x + count > SCREEN_COLS)
        count = SCREEN_COLS - x;

    bios_set_cursor(page, x, y);
    memset(&regs, 0, sizeof(regs));
    regs.h.ah = 0x09;
    regs.h.al = ch;
    regs.h.bh = page;
    regs.h.bl = attr;
    regs.w.cx = (uint16_t)count;
    int386(0x10, &regs, &regs);
}

static void bios_write_repeat(int x, int y, unsigned char ch,
                              unsigned char attr, int count)
{
    bios_write_repeat_page(draw_page, x, y, ch, attr, count);
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
    text(x + 1, y + h - 2, "Esc Saves & Exits   \x18\x19 Move   Enter Selects", ATTR_WINDOW);

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

static void redraw_menu_hover(int old_selected, int new_selected)
{
    const int x = 17;
    const int y = 4;
    const int w = 46;
    int row;
    const char *s;
    int tx;

    if (old_selected == new_selected)
        return;

    mouse_hide();

    row = y + 5 + old_selected;
    bios_write_repeat_page(visible_page, x + 2, row, ' ', ATTR_WINDOW, w - 4);
    s = menu_items[old_selected];
    tx = x + 3;
    while (*s && tx < SCREEN_COLS)
        bios_write_repeat_page(visible_page, tx++, row,
                               (unsigned char)*s++, ATTR_WINDOW, 1);

    row = y + 5 + new_selected;
    bios_write_repeat_page(visible_page, x + 2, row, ' ', ATTR_SELECT, w - 4);
    s = menu_items[new_selected];
    tx = x + 3;
    while (*s && tx < SCREEN_COLS)
        bios_write_repeat_page(visible_page, tx++, row,
                               (unsigned char)*s++, ATTR_SELECT, 1);

    bios_write_repeat_page(visible_page, 0, SCREEN_ROWS - 1,
                           ' ', ATTR_HEADER, SCREEN_COLS);
    s = menu_help[new_selected];
    tx = (SCREEN_COLS - (int)strlen(s)) / 2;
    if (tx < 0) tx = 0;
    while (*s && tx < SCREEN_COLS)
        bios_write_repeat_page(visible_page, tx++, SCREEN_ROWS - 1,
                               (unsigned char)*s++, ATTR_HEADER, 1);

    current_selection = new_selected;
    mouse_show();
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
            int moved = (mx != mouse_last_x || my != mouse_last_y);

            if (moved) {
                mouse_last_x = mx;
                mouse_last_y = my;
                if (item >= 0 && item != *selected) {
                    int old_selected = *selected;
                    *selected = item;
                    redraw_menu_hover(old_selected, *selected);
                }
            }

            mouse_buttons = buttons;
            if (mouse_left_pressed() && item >= 0) {
                if (*selected != item) {
                    int old_selected = *selected;
                    *selected = item;
                    redraw_menu_hover(old_selected, *selected);
                }
                return KEY_ENTER;
            }
        }
    }
}

static void wait_for_ack(void)
{
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
            if (mouse_left_pressed())
                return;
        }
    }
}



typedef struct setup_sound_s {
    int32 fx_device;
    int32 music_device;
    int32 num_voices;
    int32 num_channels;
    int32 num_bits;
    int32 mix_rate;
    int32 midi_port;
    int32 blaster_address;
    int32 blaster_type;
    int32 blaster_interrupt;
    int32 blaster_dma8;
    int32 blaster_dma16;
    int32 blaster_emu;
    int32 covox_port;
    int32 soundsource_port;
    int32 tandy_port;
    int32 sound_toggle;
    int32 music_toggle;
    int32 voice_toggle;
    int32 ambience_toggle;
    int32 reverse_stereo;
} setup_sound_t;

typedef struct setup_screen_s {
    int32 mode;
    int32 width;
    int32 height;
} setup_screen_t;

typedef struct screen_choice_s {
    int32 mode;
    int32 width;
    int32 height;
} screen_choice_t;

static const int fx_devices[] = {
    SoundBlaster, SoundSource, Covox, NumSoundCards
};
static const char *const fx_device_names[] = {
    "Sound Blaster", "Disney Sound Source",
    "Covox Speech Thing", "None"
};
static const int music_devices[] = { SoundBlaster, Adlib, GenMidi, NumSoundCards };
static const char *const music_device_names[] = {
    "Sound Blaster FM", "AdLib", "MPU-401 / General MIDI", "None"
};
static const int voice_values[] = { 4, 8, 16, 24, 32 };
static const int channel_values[] = { 1, 2 };
static const int bit_values[] = { 8, 16 };
static const int rate_values[] = { 8000, 11025, 16000, 22050, 32000, 44100 };

#define ARRAY_COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))
#define MAX_SCREEN_CHOICES 32

static uint16_t rd16(const unsigned char *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t rd32(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int find_int(const int *values, int count, int value)
{
    int i;
    for (i = 0; i < count; ++i)
        if (values[i] == value)
            return i;
    return -1;
}

static const char *device_name(const int *ids, const char *const *names,
                               int count, int value)
{
    int i = find_int(ids, count, value);
    return i >= 0 ? names[i] : "Unknown / legacy";
}

static void cfg_get_number_default(int handle, char *section, char *entry,
                                   int32 *value, int32 fallback)
{
    *value = fallback;
    (void)SCRIPT_GetNumber(handle, section, entry, value);
}

static void load_sound_cfg(int handle, setup_sound_t *c)
{
    /* These are setup defaults only for a newly-created/partial CFG.  Existing
       values are always retained verbatim by SCRIPT_GetNumber(). */
    cfg_get_number_default(handle, "Sound Setup", "FXDevice", &c->fx_device, SoundBlaster);
    cfg_get_number_default(handle, "Sound Setup", "MusicDevice", &c->music_device, Adlib);
    cfg_get_number_default(handle, "Sound Setup", "NumVoices", &c->num_voices, 16);
    cfg_get_number_default(handle, "Sound Setup", "NumChannels", &c->num_channels, 2);
    cfg_get_number_default(handle, "Sound Setup", "NumBits", &c->num_bits, 8);
    cfg_get_number_default(handle, "Sound Setup", "MixRate", &c->mix_rate, 22050);
    if (c->fx_device == SoundSource)
        c->mix_rate = SETUP_DSS_RATE;
    cfg_get_number_default(handle, "Sound Setup", "MidiPort", &c->midi_port, 0x330);
    cfg_get_number_default(handle, "Sound Setup", "BlasterAddress", &c->blaster_address, 0x220);
    cfg_get_number_default(handle, "Sound Setup", "BlasterType", &c->blaster_type, 6);
    cfg_get_number_default(handle, "Sound Setup", "BlasterInterrupt", &c->blaster_interrupt, 5);
    cfg_get_number_default(handle, "Sound Setup", "BlasterDma8", &c->blaster_dma8, 1);
    cfg_get_number_default(handle, "Sound Setup", "BlasterDma16", &c->blaster_dma16, 5);
    cfg_get_number_default(handle, "Sound Setup", "BlasterEmu", &c->blaster_emu, 0x620);
    cfg_get_number_default(handle, "Sound Setup", "CovoxPort", &c->covox_port, 0x278);
    cfg_get_number_default(handle, "Sound Setup", "SoundSourcePort", &c->soundsource_port, 0x378);
    cfg_get_number_default(handle, "Sound Setup", "TandyPort", &c->tandy_port, 0xC0);
    cfg_get_number_default(handle, "Sound Setup", "SoundToggle", &c->sound_toggle, 1);
    cfg_get_number_default(handle, "Sound Setup", "MusicToggle", &c->music_toggle, 1);
    cfg_get_number_default(handle, "Sound Setup", "VoiceToggle", &c->voice_toggle, 1);
    cfg_get_number_default(handle, "Sound Setup", "AmbienceToggle", &c->ambience_toggle, 1);
    cfg_get_number_default(handle, "Sound Setup", "ReverseStereo", &c->reverse_stereo, 0);
}

static void save_sound_cfg(int handle, const setup_sound_t *c)
{
    SCRIPT_PutNumber(handle, "Sound Setup", "FXDevice", c->fx_device, false, false);
    SCRIPT_PutNumber(handle, "Sound Setup", "MusicDevice", c->music_device, false, false);
    SCRIPT_PutNumber(handle, "Sound Setup", "NumVoices", c->num_voices, false, false);
    SCRIPT_PutNumber(handle, "Sound Setup", "NumChannels", c->num_channels, false, false);
    SCRIPT_PutNumber(handle, "Sound Setup", "NumBits", c->num_bits, false, false);
    SCRIPT_PutNumber(handle, "Sound Setup", "MixRate", c->mix_rate, false, false);
    SCRIPT_PutNumber(handle, "Sound Setup", "MidiPort", c->midi_port, true, false);
    SCRIPT_PutNumber(handle, "Sound Setup", "BlasterAddress", c->blaster_address, true, false);
    SCRIPT_PutNumber(handle, "Sound Setup", "BlasterType", c->blaster_type, false, false);
    SCRIPT_PutNumber(handle, "Sound Setup", "BlasterInterrupt", c->blaster_interrupt, false, false);
    SCRIPT_PutNumber(handle, "Sound Setup", "BlasterDma8", c->blaster_dma8, false, false);
    SCRIPT_PutNumber(handle, "Sound Setup", "BlasterDma16", c->blaster_dma16, false, false);
    SCRIPT_PutNumber(handle, "Sound Setup", "BlasterEmu", c->blaster_emu, true, false);
    SCRIPT_PutNumber(handle, "Sound Setup", "CovoxPort", c->covox_port, true, false);
    SCRIPT_PutNumber(handle, "Sound Setup", "SoundSourcePort", c->soundsource_port, true, false);
    SCRIPT_PutNumber(handle, "Sound Setup", "TandyPort", c->tandy_port, true, false);
    SCRIPT_PutNumber(handle, "Sound Setup", "SoundToggle", c->sound_toggle, false, false);
    SCRIPT_PutNumber(handle, "Sound Setup", "MusicToggle", c->music_toggle, false, false);
    SCRIPT_PutNumber(handle, "Sound Setup", "VoiceToggle", c->voice_toggle, false, false);
    SCRIPT_PutNumber(handle, "Sound Setup", "AmbienceToggle", c->ambience_toggle, false, false);
    SCRIPT_PutNumber(handle, "Sound Setup", "ReverseStereo", c->reverse_stereo, false, false);
}

static void draw_choice_menu(const char *title, const char *const *names,
                             int count, int selected)
{
    const int w = 50;
    const int h = 18;
    const int x = (SCREEN_COLS - w) / 2;
    const int y = 3;
    const int visible = 12;
    int first = selected - visible / 2;
    int i;

    if (first < 0) first = 0;
    if (first + visible > count)
        first = count > visible ? count - visible : 0;

    desktop();
    box(x, y, w, h);
    text_center(x + 1, y + 1, w - 2, title, ATTR_WINDOW);
    hline(x, y + 2, w, ATTR_BORDER);

    for (i = 0; i < visible; ++i) {
        int idx = first + i;
        int row = y + 3 + i;
        unsigned char a;

        fill(x + 2, row, w - 4, 1, ' ', ATTR_WINDOW);
        if (idx >= count)
            continue;

        a = idx == selected ? ATTR_SELECT : ATTR_WINDOW;
        fill(x + 2, row, w - 4, 1, ' ', a);
        text(x + 4, row, names[idx], a);
    }

    hline(x, y + h - 3, w, ATTR_BORDER);
    text_center(x + 1, y + h - 2, w - 2,
                "Esc Back   Up/Down Move   Enter Select", ATTR_WINDOW);
    fill(0, SCREEN_ROWS - 1, SCREEN_COLS, 1, ' ', ATTR_HEADER);
    text_center(0, SCREEN_ROWS - 1, SCREEN_COLS,
                "The selected value is applied immediately.", ATTR_HEADER);
    present_page();
}

static int choose_from_list(const char *title, const char *const *names,
                            int count, int selected)
{
    if (count <= 0)
        return -1;
    if (selected < 0 || selected >= count)
        selected = 0;

    for (;;) {
        int key;
        draw_choice_menu(title, names, count, selected);
        key = read_key();
        if (key == KEY_ESC)
            return -1;
        if (key == (0x100 | SCAN_UP)) {
            if (--selected < 0) selected = count - 1;
        } else if (key == (0x100 | SCAN_DOWN)) {
            if (++selected >= count) selected = 0;
        } else if (key == KEY_ENTER) {
            return selected;
        }
    }
}

static int choose_numeric_value(const char *title, const int *values, int count,
                                int current, const char *suffix)
{
    char storage[32][20];
    const char *names[32];
    int selected;
    int i;

    if (count > 32)
        count = 32;
    selected = find_int(values, count, current);
    if (selected < 0)
        selected = 0;

    for (i = 0; i < count; ++i) {
        if (suffix && *suffix)
            sprintf(storage[i], "%d %s", values[i], suffix);
        else
            sprintf(storage[i], "%d", values[i]);
        names[i] = storage[i];
    }

    i = choose_from_list(title, names, count, selected);
    return i < 0 ? current : values[i];
}

static int choose_hex_value(const char *title, const int *values, int count,
                            int current)
{
    char storage[32][12];
    const char *names[32];
    int selected;
    int i;

    if (count > 32) count = 32;
    selected = find_int(values, count, current);
    if (selected < 0) selected = 0;
    for (i = 0; i < count; ++i) {
        sprintf(storage[i], "0x%03X", values[i]);
        names[i] = storage[i];
    }
    i = choose_from_list(title, names, count, selected);
    return i < 0 ? current : values[i];
}

static int choose_midi_port(int current)
{
    static const int values[] = { 0x300, 0x310, 0x320, 0x330 };
    return choose_hex_value("MPU-401 Port", values, ARRAY_COUNT(values), current);
}

static int configure_sound_blaster(setup_sound_t *c)
{
    static const char *const labels[] = {
        "Base Address", "Card Type", "IRQ", "8-bit DMA", "16-bit DMA", "Back"
    };
    static const int addresses[] = { 0x210, 0x220, 0x230, 0x240, 0x250, 0x260, 0x280 };
    static const int types[] = { 1, 2, 3, 4, 6 };
    static const char *const type_names[] = {
        "Sound Blaster", "Sound Blaster Pro", "Sound Blaster 2.0",
        "Sound Blaster Pro 2", "Sound Blaster 16"
    };
    static const int irqs[] = { 2, 5, 7, 10 };
    static const int dma8[] = { 0, 1, 3 };
    static const int dma16[] = { 5, 6, 7 };
    int selected = 0, changed = 0;

    for (;;) {
        char values[6][24];
        const char *items[6];
        int key, old, choice, i;
        sprintf(values[0], "Base Address   0x%03lX", (unsigned long)c->blaster_address);
        strcpy(values[1], "Card Type      ");
        i = find_int(types, ARRAY_COUNT(types), c->blaster_type);
        strcat(values[1], i >= 0 ? type_names[i] : "Unknown");
        sprintf(values[2], "IRQ            %ld", (long)c->blaster_interrupt);
        sprintf(values[3], "8-bit DMA      %ld", (long)c->blaster_dma8);
        sprintf(values[4], "16-bit DMA     %ld", (long)c->blaster_dma16);
        strcpy(values[5], "Back");
        for (i = 0; i < 6; ++i) items[i] = values[i];
        choice = choose_from_list("Sound Blaster Configuration", items, 6, selected);
        if (choice < 0 || choice == 5) return changed;
        selected = choice;
        old = 0;
        switch (choice) {
        case 0: old=c->blaster_address; c->blaster_address=choose_hex_value(labels[0],addresses,ARRAY_COUNT(addresses),old); changed|=old!=c->blaster_address; break;
        case 1: old=c->blaster_type; i=choose_from_list(labels[1],type_names,ARRAY_COUNT(types),find_int(types,ARRAY_COUNT(types),old)); if(i>=0)c->blaster_type=types[i]; changed|=old!=c->blaster_type; break;
        case 2: old=c->blaster_interrupt; c->blaster_interrupt=choose_numeric_value(labels[2],irqs,ARRAY_COUNT(irqs),old,""); changed|=old!=c->blaster_interrupt; break;
        case 3: old=c->blaster_dma8; c->blaster_dma8=choose_numeric_value(labels[3],dma8,ARRAY_COUNT(dma8),old,""); changed|=old!=c->blaster_dma8; break;
        case 4: old=c->blaster_dma16; c->blaster_dma16=choose_numeric_value(labels[4],dma16,ARRAY_COUNT(dma16),old,""); changed|=old!=c->blaster_dma16; break;
        }
        (void)key;
    }
}

static int configure_parallel_port(const char *title, int32 *port)
{
    static const int ports[] = { 0x3BC, 0x378, 0x278 };
    int old = *port;
    *port = choose_hex_value(title, ports, ARRAY_COUNT(ports), *port);
    return old != *port;
}

static int configure_tandy_port(setup_sound_t *c)
{
    static const int ports[] = { 0x0C0, 0x1E0, 0x2C0 };
    int old = c->tandy_port;
    c->tandy_port = choose_hex_value("Tandy SN76489 Port", ports, ARRAY_COUNT(ports), old);
    return old != c->tandy_port;
}

static int configure_fx_device(setup_sound_t *c)
{
    if (c->fx_device == SoundBlaster)
        return configure_sound_blaster(c);
    if (c->fx_device == Covox)
        return configure_parallel_port("Covox LPT Port", &c->covox_port);
    if (c->fx_device == SoundSource)
        return configure_parallel_port("Disney Sound Source Port", &c->soundsource_port);
    if (c->fx_device == TandySoundSource)
        return configure_tandy_port(c);
    return 0;
}

static int configure_music_device(setup_sound_t *c)
{
    int old;
    if (c->music_device == GenMidi) {
        old = c->midi_port;
        c->midi_port = choose_midi_port(c->midi_port);
        return old != c->midi_port;
    }
    if (c->music_device == SoundBlaster)
        return configure_sound_blaster(c);
    return 0;
}

static int choose_boolean(const char *title, int current)
{
    static const char *const names[] = { "Off", "On" };
    int i = choose_from_list(title, names, 2, current ? 1 : 0);
    return i < 0 ? current : i;
}

static void draw_sound_menu(const setup_sound_t *c, int selected)
{
    const int x = 12, y = 2, w = 56, h = 22;
    static const char *const labels[] = {
        "Sound FX Device", "Music Device", "Voices", "Channels", "Sample Bits",
        "Mix Rate", "Sound", "Music", "Duke Talk", "Ambience", "Reverse Stereo"
    };
    char value[48];
    int i;

    desktop();
    box(x, y, w, h);
    text_center(x + 1, y + 1, w - 2, "Sound Setup", ATTR_WINDOW);
    hline(x, y + 2, w, ATTR_BORDER);
    for (i = 0; i < 11; ++i) {
        int row = y + 3 + i;
        unsigned char a = i == selected ? ATTR_SELECT : ATTR_WINDOW;
        fill(x + 2, row, w - 4, 1, ' ', a);
        text(x + 3, row, labels[i], a);
        value[0] = 0;
        switch (i) {
        case 0: strcpy(value, device_name(fx_devices, fx_device_names, ARRAY_COUNT(fx_devices), c->fx_device)); break;
        case 1: strcpy(value, device_name(music_devices, music_device_names, ARRAY_COUNT(music_devices), c->music_device)); break;
        case 2: sprintf(value, "%ld", (long)c->num_voices); break;
        case 3: sprintf(value, "%ld", (long)c->num_channels); break;
        case 4: sprintf(value, "%ld bit", (long)c->num_bits); break;
        case 5:
            if (c->fx_device == SoundSource)
                sprintf(value, "%d Hz (fixed)", SETUP_DSS_RATE);
            else
                sprintf(value, "%ld Hz", (long)c->mix_rate);
            break;
        case 6: strcpy(value, c->sound_toggle ? "On" : "Off"); break;
        case 7: strcpy(value, c->music_toggle ? "On" : "Off"); break;
        case 8: strcpy(value, c->voice_toggle ? "On" : "Off"); break;
        case 9: strcpy(value, c->ambience_toggle ? "On" : "Off"); break;
        case 10: strcpy(value, c->reverse_stereo ? "On" : "Off"); break;
        }
        text(x + 31, row, value, a);
    }
    text_center(x + 1, y + h - 2, w - 2,
                "Esc Back   Up/Down Move   Enter Choose", ATTR_WINDOW);
    fill(0, SCREEN_ROWS - 1, SCREEN_COLS, 1, ' ', ATTR_HEADER);
    text_center(0, SCREEN_ROWS - 1, SCREEN_COLS,
                "Changes are kept automatically; no Apply step is required.",
                ATTR_HEADER);
    present_page();
}

static int sound_setup_page(int handle)
{
    setup_sound_t c;
    int selected = 0;
    int changed = 0;

    load_sound_cfg(handle, &c);
    for (;;) {
        int key;
        int old_value;
        int choice;

        draw_sound_menu(&c, selected);
        key = read_key();

        if (key == KEY_ESC) {
            if (changed)
                save_sound_cfg(handle, &c);
            return changed;
        }
        if (key == (0x100 | SCAN_UP)) {
            if (--selected < 0) selected = 10;
            continue;
        }
        if (key == (0x100 | SCAN_DOWN)) {
            if (++selected > 10) selected = 0;
            continue;
        }
        if (key != KEY_ENTER)
            continue;

        switch (selected) {
        case 0:
            choice = choose_from_list("Sound FX Device", fx_device_names,
                                      ARRAY_COUNT(fx_devices),
                                      find_int(fx_devices, ARRAY_COUNT(fx_devices), c.fx_device));
            if (choice >= 0) {
                old_value = c.fx_device;
                c.fx_device = fx_devices[choice];
                changed |= old_value != c.fx_device;
                if (c.fx_device == SoundSource && c.mix_rate != SETUP_DSS_RATE) {
                    c.mix_rate = SETUP_DSS_RATE;
                    changed = 1;
                }
                changed |= configure_fx_device(&c);
            }
            break;
        case 1:
            choice = choose_from_list("Music Device", music_device_names,
                                      ARRAY_COUNT(music_devices),
                                      find_int(music_devices, ARRAY_COUNT(music_devices), c.music_device));
            if (choice >= 0) {
                old_value = c.music_device;
                c.music_device = music_devices[choice];
                changed |= old_value != c.music_device;
                changed |= configure_music_device(&c);
            }
            break;
        case 2:
            old_value = c.num_voices;
            c.num_voices = choose_numeric_value("Voices", voice_values,
                                                ARRAY_COUNT(voice_values),
                                                c.num_voices, "");
            changed |= old_value != c.num_voices;
            break;
        case 3:
            old_value = c.num_channels;
            c.num_channels = choose_numeric_value("Channels", channel_values,
                                                  ARRAY_COUNT(channel_values),
                                                  c.num_channels, "");
            changed |= old_value != c.num_channels;
            break;
        case 4:
            old_value = c.num_bits;
            c.num_bits = choose_numeric_value("Sample Bits", bit_values,
                                              ARRAY_COUNT(bit_values),
                                              c.num_bits, "bit");
            changed |= old_value != c.num_bits;
            break;
        case 5:
            old_value = c.mix_rate;
            if (c.fx_device == SoundSource)
                c.mix_rate = SETUP_DSS_RATE;
            else
                c.mix_rate = choose_numeric_value("Mix Rate", rate_values,
                                                  ARRAY_COUNT(rate_values),
                                                  c.mix_rate, "Hz");
            changed |= old_value != c.mix_rate;
            break;
        case 6:
            old_value = c.sound_toggle;
            c.sound_toggle = choose_boolean("Sound", c.sound_toggle);
            changed |= old_value != c.sound_toggle;
            break;
        case 7:
            old_value = c.music_toggle;
            c.music_toggle = choose_boolean("Music", c.music_toggle);
            changed |= old_value != c.music_toggle;
            break;
        case 8:
            old_value = c.voice_toggle;
            c.voice_toggle = choose_boolean("Duke Talk", c.voice_toggle);
            changed |= old_value != c.voice_toggle;
            break;
        case 9:
            old_value = c.ambience_toggle;
            c.ambience_toggle = choose_boolean("Ambience", c.ambience_toggle);
            changed |= old_value != c.ambience_toggle;
            break;
        case 10:
            old_value = c.reverse_stereo;
            c.reverse_stereo = choose_boolean("Reverse Stereo", c.reverse_stereo);
            changed |= old_value != c.reverse_stereo;
            break;
        }

        if (changed)
            save_sound_cfg(handle, &c);
    }
}

static int enumerate_screen_choices(screen_choice_t *out, int max_choices)
{
    unsigned char *buf;
    union REGS in, regs;
    struct SREGS sregs;
    uint32_t farptr, linear;
    const uint16_t *modes;
    int count = 0, n;

    if (max_choices <= 0) return 0;
    out[count].mode = 2; out[count].width = 320; out[count].height = 200; ++count;
    buf = (unsigned char *)dos_alloc_low(512);
    if (!buf) return count;
    memset(buf, 0, 512);
    memcpy(buf, "VBE2", 4);
    memset(&in, 0, sizeof(in)); memset(&regs, 0, sizeof(regs));
    segread(&sregs);
    in.w.ax = 0x4f00; in.w.di = 0; sregs.es = dos_ptr_segment(buf);
    int386x(0x10, &in, &regs, &sregs);
    if (regs.w.ax != 0x004f || memcmp(buf, "VESA", 4) != 0) { dos_free_low(buf); return count; }
    farptr = rd32(buf + 14);
    linear = ((farptr >> 16) << 4) + (farptr & 0xffffu);
    modes = (const uint16_t *)dos_guest_linear_ptr(linear);
    for (n = 0; n < 256 && count < max_choices; ++n) {
        uint16_t mode = modes[n];
        int32 w, h;
        int i, duplicate = 0;
        if (mode == 0xffffu) break;
        memset(buf, 0, 256); memset(&in, 0, sizeof(in)); memset(&regs, 0, sizeof(regs));
        segread(&sregs);
        in.w.ax = 0x4f01; in.w.cx = mode; in.w.di = 0; sregs.es = dos_ptr_segment(buf);
        int386x(0x10, &in, &regs, &sregs);
        if (regs.w.ax != 0x004f) continue;
        if (!(rd16(buf + 0) & 1)) continue;
        if (buf[24] != 1 || buf[25] != 8 || buf[27] != 4) continue;
        w = rd16(buf + 18); h = rd16(buf + 20);
        for (i = 0; i < count; ++i) if (out[i].width == w && out[i].height == h) { duplicate = 1; break; }
        if (!duplicate) { out[count].mode = 1; out[count].width = w; out[count].height = h; ++count; }
    }
    dos_free_low(buf);
    for (n = 1; n < count; ++n) {
        screen_choice_t v = out[n]; int j = n - 1;
        while (j >= 1 && (out[j].width > v.width || (out[j].width == v.width && out[j].height > v.height))) { out[j + 1] = out[j]; --j; }
        out[j + 1] = v;
    }
    return count;
}

static void load_screen_cfg(int handle, setup_screen_t *c)
{
    cfg_get_number_default(handle, "Screen Setup", "ScreenMode", &c->mode, 2);
    cfg_get_number_default(handle, "Screen Setup", "ScreenWidth", &c->width, 320);
    cfg_get_number_default(handle, "Screen Setup", "ScreenHeight", &c->height, 200);
}

static void save_screen_cfg(int handle, const setup_screen_t *c)
{
    SCRIPT_PutNumber(handle, "Screen Setup", "ScreenMode", c->mode, false, false);
    SCRIPT_PutNumber(handle, "Screen Setup", "ScreenWidth", c->width, false, false);
    SCRIPT_PutNumber(handle, "Screen Setup", "ScreenHeight", c->height, false, false);
}

static void draw_screen_menu(const screen_choice_t *choices, int count, int selected)
{
    const int x = 18, y = 5, w = 44, h = 15;
    int i, first = selected - 4;
    char b[40];
    if (first < 0) first = 0;
    if (first + 8 > count) first = count > 8 ? count - 8 : 0;
    desktop(); box(x, y, w, h);
    text_center(x + 1, y + 1, w - 2, "Screen Setup", ATTR_WINDOW); hline(x, y + 2, w, ATTR_BORDER);
    for (i = 0; i < 8 && first + i < count; ++i) {
        int idx = first + i, row = y + 3 + i; unsigned char a = idx == selected ? ATTR_SELECT : ATTR_WINDOW;
        fill(x + 2, row, w - 4, 1, ' ', a);
        sprintf(b, "%ld x %ld   %s", (long)choices[idx].width, (long)choices[idx].height,
                choices[idx].mode == 2 ? "VGA" : "VBE");
        text(x + 4, row, b, a);
    }
    hline(x, y + h - 3, w, ATTR_BORDER);
    text_center(x + 1, y + h - 2, w - 2, "Esc Back   Enter Select", ATTR_WINDOW);
    fill(0, SCREEN_ROWS - 1, SCREEN_COLS, 1, ' ', ATTR_HEADER);
    text_center(0, SCREEN_ROWS - 1, SCREEN_COLS, "Modes reported by the VGA/VBE BIOS and accepted by BUILD.", ATTR_HEADER);
    present_page();
}

static int screen_setup_page(int handle)
{
    screen_choice_t choices[MAX_SCREEN_CHOICES];
    setup_screen_t c;
    int count = enumerate_screen_choices(choices, MAX_SCREEN_CHOICES), selected = 0, i;
    load_screen_cfg(handle, &c);
    for (i = 0; i < count; ++i) if (choices[i].mode == c.mode && choices[i].width == c.width && choices[i].height == c.height) { selected = i; break; }
    for (;;) {
        int key;
        draw_screen_menu(choices, count, selected);
        key = read_key();
        if (key == KEY_ESC) return 0;
        if (key == (0x100 | SCAN_UP)) { if (--selected < 0) selected = count - 1; }
        else if (key == (0x100 | SCAN_DOWN)) { if (++selected >= count) selected = 0; }
        else if (key == KEY_ENTER) {
            c.mode = choices[selected].mode; c.width = choices[selected].width; c.height = choices[selected].height;
            save_screen_cfg(handle, &c); return 1;
        }
    }
}



typedef struct setup_game_s {
    int32 run_mode;
    int32 crosshairs;
    int32 gamma;
    int32 screen_size;
    int32 weapon_choice[10];
} setup_game_t;

static const char *const weapon_names[] = {
    "Knee", "Pistol", "Shotgun", "Chaingun", "RPG",
    "Pipe Bomb", "Shrinker", "Devastator", "Tripbomb", "Freezer"
};

static void load_game_cfg(int handle, setup_game_t *g)
{
    static const int defaults[10] = { 3, 4, 5, 7, 8, 6, 0, 2, 9, 1 };
    int i;
    char entry[32];
    cfg_get_number_default(handle, "Misc", "RunMode", &g->run_mode, 0);
    cfg_get_number_default(handle, "Misc", "Crosshairs", &g->crosshairs, 0);
    cfg_get_number_default(handle, "Screen Setup", "ScreenGamma", &g->gamma, 0);
    cfg_get_number_default(handle, "Screen Setup", "ScreenSize", &g->screen_size, 8);
    for (i = 0; i < 10; ++i) {
        sprintf(entry, "WeaponChoice%d", i);
        cfg_get_number_default(handle, "Misc", entry, &g->weapon_choice[i], defaults[i]);
        if (g->weapon_choice[i] < 0 || g->weapon_choice[i] > 9)
            g->weapon_choice[i] = defaults[i];
    }
}

static void save_game_cfg(int handle, const setup_game_t *g)
{
    int i;
    char entry[32];
    SCRIPT_PutNumber(handle, "Misc", "RunMode", g->run_mode, false, false);
    SCRIPT_PutNumber(handle, "Misc", "Crosshairs", g->crosshairs, false, false);
    SCRIPT_PutNumber(handle, "Screen Setup", "ScreenGamma", g->gamma, false, false);
    SCRIPT_PutNumber(handle, "Screen Setup", "ScreenSize", g->screen_size, false, false);
    for (i = 0; i < 10; ++i) {
        sprintf(entry, "WeaponChoice%d", i);
        SCRIPT_PutNumber(handle, "Misc", entry, g->weapon_choice[i], false, false);
    }
}

static void draw_game_menu(const setup_game_t *g, int selected)
{
    const int x = 13, y = 2, w = 54, h = 22;
    static const char *const labels[] = {
        "Auto Run", "Crosshairs", "Screen Gamma", "Screen Size",
        "Weapon Preference 1", "Weapon Preference 2", "Weapon Preference 3",
        "Weapon Preference 4", "Weapon Preference 5", "Weapon Preference 6",
        "Weapon Preference 7", "Weapon Preference 8", "Weapon Preference 9",
        "Weapon Preference 10"
    };
    int i;
    char value[32];
    desktop(); box(x, y, w, h);
    text_center(x + 1, y + 1, w - 2, "Game Setup", ATTR_WINDOW);
    hline(x, y + 2, w, ATTR_BORDER);
    for (i = 0; i < 14; ++i) {
        int row = y + 3 + i;
        unsigned char a = i == selected ? ATTR_SELECT : ATTR_WINDOW;
        fill(x + 2, row, w - 4, 1, ' ', a);
        text(x + 3, row, labels[i], a);
        if (i == 0) strcpy(value, g->run_mode ? "On" : "Off");
        else if (i == 1) strcpy(value, g->crosshairs ? "On" : "Off");
        else if (i == 2) sprintf(value, "%ld", (long)g->gamma);
        else if (i == 3) sprintf(value, "%ld", (long)g->screen_size);
        else strcpy(value, weapon_names[g->weapon_choice[i - 4]]);
        text(x + 33, row, value, a);
    }
    hline(x, y + h - 3, w, ATTR_BORDER);
    text_center(x + 1, y + h - 2, w - 2,
                "Esc Back   Up/Down Move   Enter Choose", ATTR_WINDOW);
    present_page();
}

static int game_setup_page(int handle)
{
    setup_game_t g;
    static const int gamma_values[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
    static const int size_values[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8 };
    int selected = 0, changed = 0;
    load_game_cfg(handle, &g);
    for (;;) {
        int key, choice, old;
        draw_game_menu(&g, selected);
        key = read_key();
        if (key == KEY_ESC)
            return changed;
        if (key == (0x100 | SCAN_UP)) {
            if (--selected < 0) selected = 13;
            continue;
        }
        if (key == (0x100 | SCAN_DOWN)) {
            if (++selected > 13) selected = 0;
            continue;
        }
        if (key != KEY_ENTER)
            continue;
        if (selected == 0) {
            old = g.run_mode; g.run_mode = choose_boolean("Auto Run", g.run_mode);
            changed |= old != g.run_mode;
        } else if (selected == 1) {
            old = g.crosshairs; g.crosshairs = choose_boolean("Crosshairs", g.crosshairs);
            changed |= old != g.crosshairs;
        } else if (selected == 2) {
            old = g.gamma;
            g.gamma = choose_numeric_value("Screen Gamma", gamma_values,
                                           ARRAY_COUNT(gamma_values), g.gamma, "");
            changed |= old != g.gamma;
        } else if (selected == 3) {
            old = g.screen_size;
            g.screen_size = choose_numeric_value("Screen Size", size_values,
                                                 ARRAY_COUNT(size_values), g.screen_size, "");
            changed |= old != g.screen_size;
        } else {
            int idx = selected - 4;
            choice = choose_from_list("Weapon Preference", weapon_names, 10,
                                      (int)g.weapon_choice[idx]);
            if (choice >= 0) {
                changed |= g.weapon_choice[idx] != choice;
                g.weapon_choice[idx] = choice;
            }
        }
        if (changed) {
            save_game_cfg(handle, &g);
            SCRIPT_Save(handle, SETUP_CFG);
        }
    }
}


typedef struct setup_mouse_s {
    char button[3][32];
    char clicked[3][32];
    char analog[2][32];
    char digital[2][2][32];
    int32 sensitivity;
    int32 aiming;
    int32 aiming_flipped;
} setup_mouse_t;

static const char *const analog_names[] = {
    "None", "analog_turning", "analog_strafing", "analog_moving",
    "analog_lookingupanddown"
};

void Error(char *error, ...)
{
    printf("SETUP: %s\n", error ? error : "error");
}

static int game_function_index(const char *name)
{
    int i;
    if (!name || !*name)
        return 0;
    for (i = 0; i < NUMGAMEFUNCTIONS; ++i)
        if (strcasecmp(name, gamefunctions[i]) == 0)
            return i + 1;
    return 0;
}

static int choose_game_function(const char *title, const char *current)
{
    const char *names[NUMGAMEFUNCTIONS + 1];
    int i;
    names[0] = "None";
    for (i = 0; i < NUMGAMEFUNCTIONS; ++i)
        names[i + 1] = gamefunctions[i];
    i = choose_from_list(title, names, NUMGAMEFUNCTIONS + 1,
                         game_function_index(current));
    return i;
}

static int analog_index(const char *name)
{
    int i;
    if (!name || !*name)
        return 0;
    for (i = 1; i < ARRAY_COUNT(analog_names); ++i)
        if (strcasecmp(name, analog_names[i]) == 0)
            return i;
    return 0;
}

static void load_key_pair(int handle, int function, char *key1, char *key2)
{
    key1[0] = 0;
    key2[0] = 0;
    SCRIPT_GetDoubleString(handle, "KeyDefinitions", gamefunctions[function],
                           key1, key2);
    if (!key1[0] && !key2[0]) {
        strcpy(key1, keydefaults[function * 3 + 1]);
        strcpy(key2, keydefaults[function * 3 + 2]);
    }
}

static void save_key_pair(int handle, int function, const char *key1,
                          const char *key2)
{
    SCRIPT_PutDoubleString(handle, "KeyDefinitions", gamefunctions[function],
                           (char *)key1, (char *)key2);
}

static void draw_keyboard_menu(int handle, int selected, int column)
{
    const int x = 5, y = 2, w = 70, h = 22, visible = 15;
    int first = selected - visible / 2;
    int i;
    char key1[32], key2[32], line[68];

    if (first < 0) first = 0;
    if (first + visible > NUMGAMEFUNCTIONS)
        first = NUMGAMEFUNCTIONS - visible;
    if (first < 0) first = 0;

    desktop();
    box(x, y, w, h);
    text_center(x + 1, y + 1, w - 2, "Setup Keyboard", ATTR_WINDOW);
    hline(x, y + 2, w, ATTR_BORDER);
    text(x + 3, y + 3, "Game Function", ATTR_BORDER);
    text(x + 38, y + 3, "Primary", ATTR_BORDER);
    text(x + 53, y + 3, "Secondary", ATTR_BORDER);

    for (i = 0; i < visible; ++i) {
        int idx = first + i;
        int row = y + 4 + i;
        unsigned char a = idx == selected ? ATTR_SELECT : ATTR_WINDOW;
        if (idx >= NUMGAMEFUNCTIONS) break;
        load_key_pair(handle, idx, key1, key2);
        fill(x + 2, row, w - 4, 1, ' ', a);
        sprintf(line, "%-32s", gamefunctions[idx]);
        text(x + 3, row, line, a);
        if (idx == selected && column == 0) text(x + 37, row, ">", a);
        text(x + 39, row, key1[0] ? key1 : "None", a);
        if (idx == selected && column == 1) text(x + 52, row, ">", a);
        text(x + 54, row, key2[0] ? key2 : "None", a);
    }

    hline(x, y + h - 3, w, ATTR_BORDER);
    text_center(x + 1, y + h - 2, w - 2,
                "Esc Back  Up/Down Function  Left/Right Slot  Enter Rebind",
                ATTR_WINDOW);
    fill(0, SCREEN_ROWS - 1, SCREEN_COLS, 1, ' ', ATTR_HEADER);
    text_center(0, SCREEN_ROWS - 1, SCREEN_COLS,
                "Delete clears the selected binding.", ATTR_HEADER);
    present_page();
}

static int capture_binding(char *out, int out_size)
{
    kb_scancode scan;
    const char *name;

    desktop();
    box(18, 8, 44, 9);
    text_center(19, 10, 42, "Press a key for this binding", ATTR_WINDOW);
    text_center(19, 12, 42, "Esc leaves the binding unchanged", ATTR_BORDER);
    present_page();

    mouse_hide();
    KB_Startup();
    KB_ClearKeysDown();
    KB_ClearLastScanCode();
    for (;;) {
        KB_ServiceEvents();
        (void)dos_yield();
        scan = KB_GetLastScanCode();
        if (scan != sc_None)
            break;
    }
    KB_Shutdown();
    mouse_show();

    if (scan == sc_Escape)
        return 0;
    name = KB_ScanCodeToString(scan);
    if (!name || !*name)
        return 0;
    strncpy(out, name, out_size - 1);
    out[out_size - 1] = 0;
    return 1;
}

static int keyboard_setup_page(int handle)
{
    int selected = 0, column = 0, changed = 0;
    for (;;) {
        int key;
        char key1[32], key2[32], captured[32];
        draw_keyboard_menu(handle, selected, column);
        key = read_key();
        if (key == KEY_ESC)
            return changed;
        if (key == (0x100 | SCAN_UP)) {
            if (--selected < 0) selected = NUMGAMEFUNCTIONS - 1;
        } else if (key == (0x100 | SCAN_DOWN)) {
            if (++selected >= NUMGAMEFUNCTIONS) selected = 0;
        } else if (key == (0x100 | 0x4b)) {
            column = 0;
        } else if (key == (0x100 | 0x4d)) {
            column = 1;
        } else if (key == (0x100 | 0x53)) {
            load_key_pair(handle, selected, key1, key2);
            if (column == 0) key1[0] = 0; else key2[0] = 0;
            save_key_pair(handle, selected, key1, key2);
            changed = 1;
        } else if (key == KEY_ENTER) {
            load_key_pair(handle, selected, key1, key2);
            if (capture_binding(captured, sizeof(captured))) {
                if (column == 0) strcpy(key1, captured); else strcpy(key2, captured);
                save_key_pair(handle, selected, key1, key2);
                changed = 1;
            }
        }
    }
}

static void load_mouse_cfg(int handle, setup_mouse_t *m)
{
    int i, d;
    char entry[40];
    static const char *const defaults_button[] = { "Fire", "Strafe", "Move_Forward" };
    static const char *const defaults_clicked[] = { "", "Open", "" };
    static const char *const defaults_analog[] = { "analog_turning", "analog_moving" };

    memset(m, 0, sizeof(*m));
    for (i = 0; i < 3; ++i) {
        sprintf(entry, "MouseButton%d", i);
        strcpy(m->button[i], defaults_button[i]);
        SCRIPT_GetString(handle, "Controls", entry, m->button[i]);
        sprintf(entry, "MouseButtonClicked%d", i);
        strcpy(m->clicked[i], defaults_clicked[i]);
        SCRIPT_GetString(handle, "Controls", entry, m->clicked[i]);
    }
    for (i = 0; i < 2; ++i) {
        sprintf(entry, "MouseAnalogAxes%d", i);
        strcpy(m->analog[i], defaults_analog[i]);
        SCRIPT_GetString(handle, "Controls", entry, m->analog[i]);
        for (d = 0; d < 2; ++d) {
            sprintf(entry, "MouseDigitalAxes%d_%d", i, d);
            m->digital[i][d][0] = 0;
            SCRIPT_GetString(handle, "Controls", entry, m->digital[i][d]);
        }
    }
    cfg_get_number_default(handle, "Controls", "MouseSensitivity", &m->sensitivity, 32768);
    cfg_get_number_default(handle, "Controls", "MouseAiming", &m->aiming, 0);
    cfg_get_number_default(handle, "Controls", "MouseAimingFlipped", &m->aiming_flipped, 0);
}

static void save_mouse_cfg(int handle, const setup_mouse_t *m)
{
    int i, d;
    char entry[40];
    for (i = 0; i < 3; ++i) {
        sprintf(entry, "MouseButton%d", i);
        SCRIPT_PutString(handle, "Controls", entry, (char *)m->button[i]);
        sprintf(entry, "MouseButtonClicked%d", i);
        SCRIPT_PutString(handle, "Controls", entry, (char *)m->clicked[i]);
    }
    for (i = 0; i < 2; ++i) {
        sprintf(entry, "MouseAnalogAxes%d", i);
        SCRIPT_PutString(handle, "Controls", entry, (char *)m->analog[i]);
        for (d = 0; d < 2; ++d) {
            sprintf(entry, "MouseDigitalAxes%d_%d", i, d);
            SCRIPT_PutString(handle, "Controls", entry, (char *)m->digital[i][d]);
        }
    }
    SCRIPT_PutNumber(handle, "Controls", "MouseSensitivity", m->sensitivity, false, false);
    SCRIPT_PutNumber(handle, "Controls", "MouseAiming", m->aiming, false, false);
    SCRIPT_PutNumber(handle, "Controls", "MouseAimingFlipped", m->aiming_flipped, false, false);
}

static void draw_mouse_menu(const setup_mouse_t *m, int selected)
{
    const int x = 9, y = 2, w = 62, h = 22;
    static const char *const labels[] = {
        "Left Button", "Middle Button", "Right Button",
        "Left Double Click", "Middle Double Click", "Right Double Click",
        "X Axis Analog", "Y Axis Analog", "X Axis Left", "X Axis Right",
        "Y Axis Up", "Y Axis Down", "Sensitivity", "Mouse Aiming", "Aiming Flipped"
    };
    int i;
    char value[40];
    desktop(); box(x, y, w, h);
    text_center(x + 1, y + 1, w - 2, "Setup Mouse", ATTR_WINDOW);
    hline(x, y + 2, w, ATTR_BORDER);
    for (i = 0; i < 15; ++i) {
        int row = y + 3 + i;
        unsigned char a = i == selected ? ATTR_SELECT : ATTR_WINDOW;
        fill(x + 2, row, w - 4, 1, ' ', a);
        text(x + 3, row, labels[i], a);
        value[0] = 0;
        if (i < 3) strcpy(value, m->button[i][0] ? m->button[i] : "None");
        else if (i < 6) strcpy(value, m->clicked[i - 3][0] ? m->clicked[i - 3] : "None");
        else if (i < 8) strcpy(value, m->analog[i - 6][0] ? m->analog[i - 6] : "None");
        else if (i < 12) {
            int axis = (i - 8) / 2, dir = (i - 8) & 1;
            strcpy(value, m->digital[axis][dir][0] ? m->digital[axis][dir] : "None");
        } else if (i == 12) sprintf(value, "%ld", (long)(m->sensitivity >> 10));
        else if (i == 13) strcpy(value, m->aiming ? "On" : "Off");
        else strcpy(value, m->aiming_flipped ? "On" : "Off");
        text(x + 31, row, value, a);
    }
    hline(x, y + h - 3, w, ATTR_BORDER);
    text_center(x + 1, y + h - 2, w - 2,
                "Esc Back   Up/Down Move   Enter Choose", ATTR_WINDOW);
    fill(0, SCREEN_ROWS - 1, SCREEN_COLS, 1, ' ', ATTR_HEADER);
    text_center(0, SCREEN_ROWS - 1, SCREEN_COLS,
                "Mouse assignments use the same [Controls] entries as Duke.", ATTR_HEADER);
    present_page();
}

static int mouse_setup_page(int handle)
{
    setup_mouse_t m;
    int selected = 0, changed = 0;
    static const int sensitivity_values[] = {
        4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 60, 64
    };
    load_mouse_cfg(handle, &m);
    for (;;) {
        int key, choice, old;
        draw_mouse_menu(&m, selected);
        key = read_key();
        if (key == KEY_ESC)
            return changed;
        if (key == (0x100 | SCAN_UP)) {
            if (--selected < 0) selected = 14;
            continue;
        }
        if (key == (0x100 | SCAN_DOWN)) {
            if (++selected > 14) selected = 0;
            continue;
        }
        if (key != KEY_ENTER)
            continue;

        if (selected < 6) {
            char *dst = selected < 3 ? m.button[selected] : m.clicked[selected - 3];
            choice = choose_game_function("Mouse Button Function", dst);
            if (choice >= 0) {
                strcpy(dst, choice == 0 ? "" : gamefunctions[choice - 1]);
                changed = 1;
            }
        } else if (selected < 8) {
            int axis = selected - 6;
            choice = choose_from_list("Mouse Analog Axis", analog_names,
                                      ARRAY_COUNT(analog_names), analog_index(m.analog[axis]));
            if (choice >= 0) {
                strcpy(m.analog[axis], choice == 0 ? "" : analog_names[choice]);
                changed = 1;
            }
        } else if (selected < 12) {
            int axis = (selected - 8) / 2, dir = (selected - 8) & 1;
            choice = choose_game_function("Mouse Digital Axis", m.digital[axis][dir]);
            if (choice >= 0) {
                strcpy(m.digital[axis][dir], choice == 0 ? "" : gamefunctions[choice - 1]);
                changed = 1;
            }
        } else if (selected == 12) {
            old = m.sensitivity;
            choice = choose_numeric_value("Mouse Sensitivity", sensitivity_values,
                                          ARRAY_COUNT(sensitivity_values),
                                          (int)(m.sensitivity >> 10), "");
            m.sensitivity = (int32)choice << 10;
            changed |= old != m.sensitivity;
        } else if (selected == 13) {
            old = m.aiming; m.aiming = choose_boolean("Mouse Aiming", m.aiming);
            changed |= old != m.aiming;
        } else {
            old = m.aiming_flipped; m.aiming_flipped = choose_boolean("Aiming Flipped", m.aiming_flipped);
            changed |= old != m.aiming_flipped;
        }
        if (changed)
            save_mouse_cfg(handle, &m);
    }
}


typedef struct setup_joystick_s {
    char button[2][32];
    char analog[2][32];
    char digital[2][2][32];
    int32 scale[2];
    int32 port;
} setup_joystick_t;

static void load_joystick_cfg(int handle, setup_joystick_t *j)
{
    int i, d;
    char entry[40];
    static const char *const button_defaults[] = { "Fire", "Strafe" };
    static const char *const analog_defaults[] = { "analog_turning", "analog_moving" };
    memset(j, 0, sizeof(*j));
    for (i = 0; i < 2; ++i) {
        sprintf(entry, "JoystickButton%d", i);
        strcpy(j->button[i], button_defaults[i]);
        SCRIPT_GetString(handle, "Controls", entry, j->button[i]);
        sprintf(entry, "JoystickAnalogAxes%d", i);
        strcpy(j->analog[i], analog_defaults[i]);
        SCRIPT_GetString(handle, "Controls", entry, j->analog[i]);
        for (d = 0; d < 2; ++d) {
            sprintf(entry, "JoystickDigitalAxes%d_%d", i, d);
            j->digital[i][d][0] = 0;
            SCRIPT_GetString(handle, "Controls", entry, j->digital[i][d]);
        }
        sprintf(entry, "JoystickAnalogScale%d", i);
        cfg_get_number_default(handle, "Controls", entry, &j->scale[i], 65536);
    }
    cfg_get_number_default(handle, "Controls", "JoystickPort", &j->port, 0);
    if (j->port != 0) j->port = 0;
}

static void save_joystick_cfg(int handle, const setup_joystick_t *j)
{
    int i, d;
    char entry[40];
    for (i = 0; i < 2; ++i) {
        sprintf(entry, "JoystickButton%d", i);
        SCRIPT_PutString(handle, "Controls", entry, (char *)j->button[i]);
        sprintf(entry, "JoystickAnalogAxes%d", i);
        SCRIPT_PutString(handle, "Controls", entry, (char *)j->analog[i]);
        for (d = 0; d < 2; ++d) {
            sprintf(entry, "JoystickDigitalAxes%d_%d", i, d);
            SCRIPT_PutString(handle, "Controls", entry, (char *)j->digital[i][d]);
        }
        sprintf(entry, "JoystickAnalogScale%d", i);
        SCRIPT_PutNumber(handle, "Controls", entry, j->scale[i], false, false);
    }
    SCRIPT_PutNumber(handle, "Controls", "JoystickPort", 0, false, false);
}

static void draw_joystick_menu(const setup_joystick_t *j, int selected)
{
    const int x = 10, y = 2, w = 60, h = 22;
    static const char *const labels[] = {
        "Button 1", "Button 2", "X Axis Analog", "Y Axis Analog",
        "X Axis Left", "X Axis Right", "Y Axis Up", "Y Axis Down",
        "X Axis Scale", "Y Axis Scale"
    };
    int i;
    char value[40];
    desktop(); box(x, y, w, h);
    text_center(x + 1, y + 1, w - 2, "Setup Joystick", ATTR_WINDOW);
    hline(x, y + 2, w, ATTR_BORDER);
    for (i = 0; i < 10; ++i) {
        int row = y + 4 + i;
        unsigned char a = i == selected ? ATTR_SELECT : ATTR_WINDOW;
        fill(x + 2, row, w - 4, 1, ' ', a);
        text(x + 3, row, labels[i], a);
        if (i < 2) strcpy(value, j->button[i][0] ? j->button[i] : "None");
        else if (i < 4) strcpy(value, j->analog[i - 2][0] ? j->analog[i - 2] : "None");
        else if (i < 8) {
            int axis = (i - 4) / 2, dir = (i - 4) & 1;
            strcpy(value, j->digital[axis][dir][0] ? j->digital[axis][dir] : "None");
        } else sprintf(value, "%ld", (long)(j->scale[i - 8] >> 12));
        text(x + 29, row, value, a);
    }
    hline(x, y + h - 3, w, ATTR_BORDER);
    text_center(x + 1, y + h - 2, w - 2,
                "Esc Back   Up/Down Move   Enter Choose", ATTR_WINDOW);
    fill(0, SCREEN_ROWS - 1, SCREEN_COLS, 1, ' ', ATTR_HEADER);
    text_center(0, SCREEN_ROWS - 1, SCREEN_COLS,
                "Native gameport backend: joystick A, 2 axes and 2 buttons.", ATTR_HEADER);
    present_page();
}

static int joystick_setup_page(int handle)
{
    setup_joystick_t j;
    static const int scale_values[] = { 4, 8, 12, 16, 20, 24, 28, 32, 48, 64, 96, 128 };
    int selected = 0, changed = 0;
    load_joystick_cfg(handle, &j);
    for (;;) {
        int key, choice, old;
        draw_joystick_menu(&j, selected);
        key = read_key();
        if (key == KEY_ESC)
            return changed;
        if (key == (0x100 | SCAN_UP)) { if (--selected < 0) selected = 9; continue; }
        if (key == (0x100 | SCAN_DOWN)) { if (++selected > 9) selected = 0; continue; }
        if (key != KEY_ENTER) continue;
        if (selected < 2) {
            choice = choose_game_function("Joystick Button Function", j.button[selected]);
            if (choice >= 0) {
                strcpy(j.button[selected], choice == 0 ? "" : gamefunctions[choice - 1]);
                changed = 1;
            }
        } else if (selected < 4) {
            int axis = selected - 2;
            choice = choose_from_list("Joystick Analog Axis", analog_names,
                                      ARRAY_COUNT(analog_names), analog_index(j.analog[axis]));
            if (choice >= 0) {
                strcpy(j.analog[axis], choice == 0 ? "" : analog_names[choice]);
                changed = 1;
            }
        } else if (selected < 8) {
            int axis = (selected - 4) / 2, dir = (selected - 4) & 1;
            choice = choose_game_function("Joystick Digital Axis", j.digital[axis][dir]);
            if (choice >= 0) {
                strcpy(j.digital[axis][dir], choice == 0 ? "" : gamefunctions[choice - 1]);
                changed = 1;
            }
        } else {
            int axis = selected - 8;
            old = j.scale[axis];
            choice = choose_numeric_value("Joystick Axis Scale", scale_values,
                                          ARRAY_COUNT(scale_values), (int)(j.scale[axis] >> 12), "");
            j.scale[axis] = (int32)choice << 12;
            changed |= old != j.scale[axis];
        }
        if (changed) {
            save_joystick_cfg(handle, &j);
            SCRIPT_Save(handle, SETUP_CFG);
        }
    }
}

static int controller_type_index(int32 type)
{
    if (type == controltype_keyboardandmouse) return 1;
    if (type == controltype_keyboardandjoystick) return 2;
    return 0;
}

static void draw_controller_menu(int handle, int selected)
{
    const int x = 18, y = 5, w = 44, h = 14;
    static const char *const labels[] = {
        "Controller Type", "Setup Keyboard", "Setup Mouse", "Setup Joystick"
    };
    static const char *const type_names[] = {
        "Keyboard", "Keyboard + Mouse", "Keyboard + Joystick"
    };
    int i;
    int32 type;
    cfg_get_number_default(handle, "Controls", "ControllerType", &type, controltype_keyboardandmouse);
    desktop(); box(x, y, w, h);
    text_center(x + 1, y + 1, w - 2, "Controller Setup", ATTR_WINDOW);
    hline(x, y + 2, w, ATTR_BORDER);
    for (i = 0; i < 4; ++i) {
        int row = y + 4 + i * 2;
        unsigned char a = i == selected ? ATTR_SELECT : ATTR_WINDOW;
        fill(x + 3, row, w - 6, 1, ' ', a);
        text(x + 5, row, labels[i], a);
        if (i == 0)
            text(x + 22, row, type_names[controller_type_index(type)], a);
    }
    hline(x, y + h - 3, w, ATTR_BORDER);
    text_center(x + 1, y + h - 2, w - 2,
                "Esc Back   Up/Down Move   Enter Select", ATTR_WINDOW);
    present_page();
}

static int controller_setup_page(int handle)
{
    static const char *const type_names[] = {
        "Keyboard", "Keyboard + Mouse", "Keyboard + Joystick"
    };
    static const int type_values[] = {
        controltype_keyboard, controltype_keyboardandmouse, controltype_keyboardandjoystick
    };
    int selected = 0, changed = 0;
    for (;;) {
        int key;
        draw_controller_menu(handle, selected);
        key = read_key();
        if (key == KEY_ESC)
            return changed;
        if (key == (0x100 | SCAN_UP)) {
            if (--selected < 0) selected = 3;
        } else if (key == (0x100 | SCAN_DOWN)) {
            if (++selected > 3) selected = 0;
        } else if (key == KEY_ENTER) {
            if (selected == 0) {
                int32 type;
                int idx, choice;
                cfg_get_number_default(handle, "Controls", "ControllerType", &type,
                                       controltype_keyboardandmouse);
                idx = controller_type_index(type);
                choice = choose_from_list("Controller Type", type_names, 3, idx);
                if (choice >= 0 && type != type_values[choice]) {
                    SCRIPT_PutNumber(handle, "Controls", "ControllerType",
                                     type_values[choice], false, false);
                    changed = 1;
                }
            } else if (selected == 1) {
                changed |= keyboard_setup_page(handle);
            } else if (selected == 2) {
                changed |= mouse_setup_page(handle);
            } else {
                changed |= joystick_setup_page(handle);
            }
            if (changed)
                SCRIPT_Save(handle, SETUP_CFG);
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
        mouse_set_sensitivity();
        mouse_buttons = mouse_state(&mx, &my);
        mouse_last_x = mx;
        mouse_last_y = my;
        /* Drain any press count left by reset/startup. */
        (void)mouse_left_pressed();
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
            if (selected == 0) {
                if (sound_setup_page(handle)) dirty = 1;
            } else if (selected == 1) {
                if (screen_setup_page(handle)) dirty = 1;
            } else if (selected == 2) {
                if (game_setup_page(handle)) dirty = 1;
            } else if (selected == 3) {
                if (controller_setup_page(handle)) dirty = 1;
            } else if (selected == MENU_COUNT - 1) {
                done = 1;
            }
        }
    }

    if (dirty)
        SCRIPT_Save(handle, SETUP_CFG);
    SCRIPT_Free(handle);
    video_mode3();
    return 0;
}
