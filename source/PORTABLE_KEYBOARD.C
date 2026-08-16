#include "types.h"
#include "keyboard.h"
#include "conio.h"
#include "dos_vect.h"
#include "dos_yield.h"
#include "string.h"
#include "util_lib.h"

#define KB_QUEUE_SIZE 64
#define KB_QUEUE_MASK (KB_QUEUE_SIZE - 1)

volatile byte KB_KeyDown[MAXKEYBOARDSCAN];
volatile kb_scancode KB_LastScan = sc_None;

static unsigned char kb_queue[KB_QUEUE_SIZE];
static volatile unsigned int kb_head;
static volatile unsigned int kb_tail;
static int kb_e0;
static int kb_e1_left;
static boolean kb_keypad_active = true;
static dos_native_vector_t kb_vector;

struct kb_name {
    const char *name;
    kb_scancode scan;
};

static const struct kb_name kb_names[] = {
    {"Esc", sc_Escape}, {"Escape", sc_Escape},
    {"1", sc_1}, {"2", sc_2}, {"3", sc_3}, {"4", sc_4}, {"5", sc_5},
    {"6", sc_6}, {"7", sc_7}, {"8", sc_8}, {"9", sc_9}, {"0", sc_0},
    {"-", sc_Minus}, {"=", sc_Equals}, {"Backspace", sc_BackSpace},
    {"Tab", sc_Tab}, {"Q", sc_Q}, {"W", sc_W}, {"E", sc_E}, {"R", sc_R},
    {"T", sc_T}, {"Y", sc_Y}, {"U", sc_U}, {"I", sc_I}, {"O", sc_O},
    {"P", sc_P}, {"[", sc_OpenBracket}, {"]", sc_CloseBracket},
    {"Enter", sc_Enter}, {"LCtrl", sc_LeftControl},
    {"A", sc_A}, {"S", sc_S}, {"D", sc_D}, {"F", sc_F}, {"G", sc_G},
    {"H", sc_H}, {"J", sc_J}, {"K", sc_K}, {"L", sc_L},
    {";", sc_SemiColon}, {"'", sc_Quote}, {"`", sc_Tilde},
    {"LShift", sc_LeftShift}, {"\\", sc_BackSlash},
    {"Z", sc_Z}, {"X", sc_X}, {"C", sc_C}, {"V", sc_V}, {"B", sc_B},
    {"N", sc_N}, {"M", sc_M}, {",", sc_Comma}, {".", sc_Period},
    {"/", sc_Slash}, {"RShift", sc_RightShift}, {"Kpad*", sc_Kpad_Star},
    {"LAlt", sc_LeftAlt}, {"Space", sc_Space}, {"CapLck", sc_CapsLock},
    {"CapsLock", sc_CapsLock}, {"F1", sc_F1}, {"F2", sc_F2}, {"F3", sc_F3},
    {"F4", sc_F4}, {"F5", sc_F5}, {"F6", sc_F6}, {"F7", sc_F7},
    {"F8", sc_F8}, {"F9", sc_F9}, {"F10", sc_F10}, {"NumLck", sc_NumLock},
    {"NumLock", sc_NumLock}, {"ScrLck", sc_ScrollLock},
    {"ScrollLock", sc_ScrollLock}, {"Kpad7", sc_kpad_7}, {"Kpad8", sc_kpad_8},
    {"Kpad9", sc_kpad_9}, {"Kpad-", sc_kpad_Minus}, {"Kpad4", sc_kpad_4},
    {"Kpad5", sc_kpad_5}, {"Kpad6", sc_kpad_6}, {"Kpad+", sc_kpad_Plus},
    {"Kpad1", sc_kpad_1}, {"Kpad2", sc_kpad_2}, {"Kpad3", sc_kpad_3},
    {"Kpad0", sc_kpad_0}, {"Kpad.", sc_kpad_Period}, {"F11", sc_F11},
    {"F12", sc_F12}, {"Pause", sc_Pause}, {"Up", sc_UpArrow},
    {"Down", sc_DownArrow}, {"Left", sc_LeftArrow}, {"Right", sc_RightArrow},
    {"Insert", sc_Insert}, {"Delete", sc_Delete}, {"Home", sc_Home},
    {"End", sc_End}, {"PgUp", sc_PgUp}, {"PgDn", sc_PgDn}, {"RAlt", sc_RightAlt},
    {"RCtrl", sc_RightControl}, {"Kpad/", sc_kpad_Slash}, {"KpdEnt", sc_kpad_Enter},
    {"KpadEnter", sc_kpad_Enter}, {"PrtScn", sc_PrintScreen},
    {"PrintScreen", sc_PrintScreen}, {0, sc_None}
};

static void kb_putch(unsigned char ch)
{
    unsigned int next = (kb_head + 1u) & KB_QUEUE_MASK;
    if (next == kb_tail)
        return;
    kb_queue[kb_head] = ch;
    kb_head = next;
}

static int kb_letter_scan(int sc)
{
    return (sc >= sc_Q && sc <= sc_P) ||
           (sc >= sc_A && sc <= sc_L) ||
           (sc >= sc_Z && sc <= sc_M);
}

static unsigned char kb_ascii_for_scan(int sc)
{
    static const unsigned char plain[0x3a] = {
        0, 27, '1','2','3','4','5','6','7','8','9','0','-','=',8,9,
        'q','w','e','r','t','y','u','i','o','p','[',']',13,0,
        'a','s','d','f','g','h','j','k','l',';','\'', '`',0,'\\',
        'z','x','c','v','b','n','m',',','.','/',0,'*',0,' '
    };
    static const unsigned char shifted[0x3a] = {
        0, 27, '!','@','#','$','%','^','&','*','(',')','_','+',8,9,
        'Q','W','E','R','T','Y','U','I','O','P','{','}',13,0,
        'A','S','D','F','G','H','J','K','L',':','"','~',0,'|',
        'Z','X','C','V','B','N','M','<','>','?',0,'*',0,' '
    };
    int shift = KB_KeyDown[sc_LeftShift] || KB_KeyDown[sc_RightShift];
    int caps = KB_KeyDown[sc_CapsLock] != 0;

    if (sc < 0 || sc >= (int)sizeof(plain))
        return 0;
    if (kb_letter_scan(sc) && caps)
        shift = !shift;
    return shift ? shifted[sc] : plain[sc];
}

static kb_scancode kb_extended_scan(unsigned int code)
{
    switch (code) {
    case 0x48: return sc_UpArrow;
    case 0x50: return sc_DownArrow;
    case 0x4b: return sc_LeftArrow;
    case 0x4d: return sc_RightArrow;
    case 0x52: return sc_Insert;
    case 0x53: return sc_Delete;
    case 0x47: return sc_Home;
    case 0x4f: return sc_End;
    case 0x49: return sc_PgUp;
    case 0x51: return sc_PgDn;
    case 0x38: return sc_RightAlt;
    case 0x1d: return sc_RightControl;
    case 0x35: return sc_kpad_Slash;
    case 0x1c: return sc_kpad_Enter;
    case 0x37: return sc_PrintScreen;
    default: return sc_Bad;
    }
}

void KB_KeyEvent(int scancode, boolean keypressed)
{
    unsigned char ch;

    if (scancode <= sc_None || scancode >= MAXKEYBOARDSCAN)
        return;

    if (scancode == sc_CapsLock && keypressed) {
        KB_KeyDown[scancode] = !KB_KeyDown[scancode];
        KB_LastScan = (kb_scancode)scancode;
        return;
    }

    KB_KeyDown[scancode] = keypressed != 0;
    if (!keypressed)
        return;

    KB_LastScan = (kb_scancode)scancode;
    ch = kb_ascii_for_scan(scancode);
    if (ch)
        kb_putch(ch);
}

static void kb_process_raw(unsigned int raw)
{
    unsigned int released;
    unsigned int code;
    kb_scancode scan;

    if (kb_e1_left) {
        --kb_e1_left;
        return;
    }
    if (raw == 0xe1u) {
        KB_KeyEvent(sc_Pause, true);
        kb_e1_left = 5;
        return;
    }
    if (raw == 0xe0u) {
        kb_e0 = 1;
        return;
    }

    released = raw & 0x80u;
    code = raw & 0x7fu;
    if (kb_e0) {
        kb_e0 = 0;
        /* PrintScreen has fake E0 2A/E0 AA shift events. */
        if (code == 0x2au)
            return;
        scan = kb_extended_scan(code);
    } else {
        scan = (code < MAXKEYBOARDSCAN) ? (kb_scancode)code : sc_Bad;
    }

    if (scan != sc_Bad)
        KB_KeyEvent(scan, released == 0);
}

static bool kb_native_irq(void *cpu)
{
    (void)cpu;
    kb_process_raw(inp(0x60));
    outp(0x20, 0x20);
    return true;
}

boolean KB_KeyWaiting(void)
{
    return kb_head != kb_tail;
}

char KB_Getch(void)
{
    unsigned char ch;
    while (!KB_KeyWaiting())
        (void)dos_yield();
    ch = kb_queue[kb_tail];
    kb_tail = (kb_tail + 1u) & KB_QUEUE_MASK;
    return (char)ch;
}

char KB_GetCh(void)
{
    return KB_Getch();
}

void KB_Addch(char ch)
{
    kb_putch((unsigned char)ch);
}

void KB_FlushKeyboardQueue(void)
{
    kb_tail = kb_head;
}

void KB_FlushKeyBoardQueue(void)
{
    KB_FlushKeyboardQueue();
}

void KB_ClearKeysDown(void)
{
    unsigned int i;
    for (i = 0; i < MAXKEYBOARDSCAN; ++i)
        KB_KeyDown[i] = 0;
    KB_LastScan = sc_None;
}

char *KB_ScanCodeToString(kb_scancode scancode)
{
    unsigned int i;
    for (i = 0; kb_names[i].name; ++i)
        if (kb_names[i].scan == scancode)
            return (char *)kb_names[i].name;
    return "";
}

kb_scancode KB_StringToScanCode(char *string)
{
    unsigned int i;
    if (!string || !*string)
        return sc_None;
    for (i = 0; kb_names[i].name; ++i)
        if (strcasecmp(string, kb_names[i].name) == 0)
            return kb_names[i].scan;
    return sc_Bad;
}

void KB_TurnKeypadOn(void)
{
    kb_keypad_active = true;
}

void KB_TurnKeypadOff(void)
{
    kb_keypad_active = false;
}

boolean KB_KeypadActive(void)
{
    return kb_keypad_active;
}

void KB_Startup(void)
{
    if (kb_vector.installed)
        return;
    kb_head = kb_tail = 0;
    kb_e0 = 0;
    kb_e1_left = 0;
    KB_ClearKeysDown();
    if (!dos_native_setvect(&kb_vector, 9, kb_native_irq))
        Error("KB_Startup: unable to install IRQ1 handler");
}

void KB_Shutdown(void)
{
    dos_native_restorevect(&kb_vector);
    KB_ClearKeysDown();
    KB_FlushKeyboardQueue();
}
