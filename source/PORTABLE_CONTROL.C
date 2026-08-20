#include "TYPES.H"
#include "KEYBOARD.H"
#include "CONTROL.H"
#include "MOUSE.H"
#include <dos.h>
#include <conio.h>
#include <tsm.h>
#include <string.h>

#define CONTROL_MOUSE_AXES 2
#define CONTROL_MOUSE_BUTTONS 3
#define CONTROL_JOY_AXES 4
#define CONTROL_JOY_BUTTONS 8
#define CONTROL_GAMEPORT 0x201u
#define CONTROL_GAMEPORT_TIMEOUT_US 5000u
#define CONTROL_AXIS_NONE (-1)
#define CONTROL_KEY_NONE sc_None

typedef struct
{
    kb_scancode key1;
    kb_scancode key2;
    boolean toggle;
    boolean toggle_state;
    boolean old_pressed;
} control_keymap_t;

typedef struct
{
    int32 analog;
    int32 digital[2];
    int32 scale;
} control_axismap_t;

boolean CONTROL_RudderEnabled = false;
boolean CONTROL_MousePresent = false;
boolean CONTROL_JoysPresent[MaxJoys] = { false, false };
boolean CONTROL_MouseEnabled = false;
boolean CONTROL_JoystickEnabled = false;
byte CONTROL_JoystickPort = 0;
uint32 CONTROL_ButtonState1 = 0;
uint32 CONTROL_ButtonHeldState1 = 0;
uint32 CONTROL_ButtonState2 = 0;
uint32 CONTROL_ButtonHeldState2 = 0;

static control_keymap_t control_keys[MAXGAMEBUTTONS];
static int32 control_button[CONTROL_JOY_BUTTONS][2];
static control_axismap_t control_axis[CONTROL_JOY_AXES];
static uint32 control_joy_center[CONTROL_JOY_AXES];
static uint32 control_joy_min[CONTROL_JOY_AXES];
static uint32 control_joy_max[CONTROL_JOY_AXES];
static int32 control_mouse_sensitivity = 32768;
static controltype control_type = controltype_keyboard;
static int32 (*control_timefunc)(void);
static int32 control_ticspersecond;

static void control_set_button(int32 which, boolean down)
{
    uint32 mask;

    if ((unsigned long)which >= MAXGAMEBUTTONS)
        return;

    if (which < 32)
    {
        mask = (uint32)1u << which;
        if (down) CONTROL_ButtonState1 |= mask;
        else CONTROL_ButtonState1 &= ~mask;
    }
    else
    {
        mask = (uint32)1u << (which - 32);
        if (down) CONTROL_ButtonState2 |= mask;
        else CONTROL_ButtonState2 &= ~mask;
    }
}

static boolean control_key_pressed(const control_keymap_t *m)
{
    if (m->key1 != CONTROL_KEY_NONE && KB_KeyPressed(m->key1)) return true;
    if (m->key2 != CONTROL_KEY_NONE && KB_KeyPressed(m->key2)) return true;
    return false;
}

static void control_apply_analog(ControlInfo *info, int32 analog, int32 value)
{
    switch (analog)
    {
        case analog_turning:          info->dyaw += value; break;
        case analog_strafing:         info->dx += value; break;
        case analog_lookingupanddown: info->dpitch += value; break;
        case analog_elevation:        info->dy += value; break;
        case analog_rolling:          info->droll += value; break;
        case analog_moving:           info->dz += value; break;
        default: break;
    }
}

boolean MOUSE_Init(void)
{
    union REGS in, out;
    memset(&in, 0, sizeof(in));
    in.w.ax = 0;
    int386(0x33, &in, &out);
    return out.w.ax != 0;
}

void MOUSE_Shutdown(void)
{
}

void MOUSE_ShowCursor(void)
{
    union REGS in, out;
    memset(&in, 0, sizeof(in));
    in.w.ax = 1;
    int386(0x33, &in, &out);
}

void MOUSE_HideCursor(void)
{
    union REGS in, out;
    memset(&in, 0, sizeof(in));
    in.w.ax = 2;
    int386(0x33, &in, &out);
}

int32 MOUSE_GetButtons(void)
{
    union REGS in, out;
    memset(&in, 0, sizeof(in));
    in.w.ax = 3;
    int386(0x33, &in, &out);
    return (int32)(out.w.bx & 7u);
}

void MOUSE_GetPosition(int32 *x, int32 *y)
{
    union REGS in, out;
    memset(&in, 0, sizeof(in));
    in.w.ax = 3;
    int386(0x33, &in, &out);
    if (x) *x = (int16)out.w.cx;
    if (y) *y = (int16)out.w.dx;
}

void MOUSE_GetDelta(int32 *x, int32 *y)
{
    union REGS in, out;
    memset(&in, 0, sizeof(in));
    in.w.ax = 11;
    int386(0x33, &in, &out);
    if (x) *x = (int16)out.w.cx;
    if (y) *y = (int16)out.w.dx;
}

void CONTROL_ClearAssignments(void)
{
    int i;

    memset(control_keys, 0, sizeof(control_keys));
    for (i = 0; i < MAXGAMEBUTTONS; ++i)
    {
        control_keys[i].key1 = CONTROL_KEY_NONE;
        control_keys[i].key2 = CONTROL_KEY_NONE;
    }

    for (i = 0; i < CONTROL_JOY_BUTTONS; ++i)
    {
        control_button[i][0] = -1;
        control_button[i][1] = -1;
    }

    for (i = 0; i < CONTROL_JOY_AXES; ++i)
    {
        control_axis[i].analog = CONTROL_AXIS_NONE;
        control_axis[i].digital[0] = -1;
        control_axis[i].digital[1] = -1;
        control_axis[i].scale = 65536;
    }

    CONTROL_ButtonState1 = CONTROL_ButtonState2 = 0;
    CONTROL_ButtonHeldState1 = CONTROL_ButtonHeldState2 = 0;
}

void CONTROL_MapKey(int32 which, kb_scancode key1, kb_scancode key2)
{
    if ((unsigned long)which >= MAXGAMEBUTTONS) return;
    control_keys[which].key1 = key1;
    control_keys[which].key2 = key2;
}

void CONTROL_MapButton(int32 whichfunction, int32 whichbutton, boolean doubleclicked)
{
    if ((unsigned long)whichbutton >= CONTROL_JOY_BUTTONS) return;
    if ((unsigned long)whichfunction >= MAXGAMEBUTTONS) return;
    control_button[whichbutton][doubleclicked ? 1 : 0] = whichfunction;
}

void CONTROL_DefineFlag(int32 which, boolean toggle)
{
    if ((unsigned long)which >= MAXGAMEBUTTONS) return;
    control_keys[which].toggle = toggle;
    control_keys[which].toggle_state = false;
    control_keys[which].old_pressed = false;
}

boolean CONTROL_FlagActive(int32 which)
{
    if ((unsigned long)which >= MAXGAMEBUTTONS) return false;
    return BUTTON(which) != 0;
}

void CONTROL_ClearButton(int32 whichbutton)
{
    control_set_button(whichbutton, false);
    if ((unsigned long)whichbutton < MAXGAMEBUTTONS)
        control_keys[whichbutton].toggle_state = false;
}

void CONTROL_MapAnalogAxis(int32 whichaxis, int32 whichanalog)
{
    if ((unsigned long)whichaxis >= CONTROL_JOY_AXES) return;
    control_axis[whichaxis].analog = whichanalog;
}

void CONTROL_MapDigitalAxis(int32 whichaxis, int32 whichfunction, int32 direction)
{
    if ((unsigned long)whichaxis >= CONTROL_JOY_AXES || (unsigned long)direction >= 2) return;
    control_axis[whichaxis].digital[direction] = whichfunction;
}

void CONTROL_SetAnalogAxisScale(int32 whichaxis, int32 axisscale)
{
    if ((unsigned long)whichaxis >= CONTROL_JOY_AXES) return;
    control_axis[whichaxis].scale = axisscale;
}

int32 CONTROL_GetMouseSensitivity(void)
{
    return control_mouse_sensitivity;
}

void CONTROL_SetMouseSensitivity(int32 newsensitivity)
{
    control_mouse_sensitivity = newsensitivity;
}

static unsigned control_joy_axis_base(void)
{
    return CONTROL_JoystickPort ? 2u : 0u;
}

static unsigned control_joy_button_base(void)
{
    return CONTROL_JoystickPort ? 6u : 4u;
}

/*
 * Trigger the IBM gameport one-shot and measure each axis in microseconds.
 * TSM_YieldTime() is deliberately called on every poll: it is the single
 * cooperative service point for both emulator devices/IRQs and native timer
 * tasks, and returns the same monotonic microsecond counter used here.
 */
static unsigned control_gameport_read(uint32 axis_us[CONTROL_JOY_AXES])
{
    uint32 start, now;
    unsigned pending = 0x0fu;
    unsigned v;
    int i;

    for (i = 0; i < CONTROL_JOY_AXES; ++i)
        axis_us[i] = CONTROL_GAMEPORT_TIMEOUT_US;

    start = TSM_YieldTime();
    outp(CONTROL_GAMEPORT, 0xffu);

    while (pending)
    {
        now = TSM_YieldTime();
        v = inp(CONTROL_GAMEPORT);

        for (i = 0; i < CONTROL_JOY_AXES; ++i)
        {
            unsigned bit = 1u << i;
            if ((pending & bit) && !(v & bit))
            {
                axis_us[i] = now - start;
                pending &= ~bit;
            }
        }

        if ((uint32)(now - start) >= CONTROL_GAMEPORT_TIMEOUT_US)
            break;
    }

    return pending;
}

static unsigned control_gameport_buttons(void)
{
    unsigned v = inp(CONTROL_GAMEPORT);
    return ((~v) >> control_joy_button_base()) & 3u;
}

static boolean control_probe_joystick(unsigned joy)
{
    uint32 raw[CONTROL_JOY_AXES];
    unsigned pending = control_gameport_read(raw);
    unsigned base = joy ? 2u : 0u;
    unsigned mask = 3u << base;
    return (pending & mask) == 0u;
}

static int32 control_normalize_joy(unsigned axis, uint32 raw)
{
    uint32 center = control_joy_center[axis];
    uint32 edge;
    int32 value;

    if (raw < center)
    {
        edge = control_joy_min[axis];
        if (center <= edge) return 0;
        value = -(int32)(((unsigned long long)(center - raw) * 32768u) / (center - edge));
    }
    else
    {
        edge = control_joy_max[axis];
        if (edge <= center) return 0;
        value = (int32)(((unsigned long long)(raw - center) * 32767u) / (edge - center));
    }

    if (value < -32768) value = -32768;
    if (value > 32767) value = 32767;
    return value;
}

static void control_wait_joy_press(void)
{
    while (control_gameport_buttons())
        TSM_Yield();
    while (!control_gameport_buttons())
        TSM_Yield();
    while (control_gameport_buttons())
        TSM_Yield();
}

void CONTROL_Startup(controltype which, int32 (*TimeFunction)(void), int32 ticspersecond)
{
    control_type = which;
    control_timefunc = TimeFunction;
    control_ticspersecond = ticspersecond;

    CONTROL_MousePresent = MOUSE_Init();
    CONTROL_MouseEnabled = CONTROL_MousePresent && (which == controltype_keyboardandmouse);
    CONTROL_JoysPresent[0] = control_probe_joystick(0);
    CONTROL_JoysPresent[1] = control_probe_joystick(1);
    {
        uint32 raw[CONTROL_JOY_AXES];
        int i;
        (void)control_gameport_read(raw);
        for (i = 0; i < CONTROL_JOY_AXES; ++i)
        {
            control_joy_center[i] = raw[i];
            control_joy_min[i] = 0;
            control_joy_max[i] = CONTROL_GAMEPORT_TIMEOUT_US;
        }
    }
    CONTROL_JoystickEnabled =
        (which == controltype_keyboardandjoystick ||
         which == controltype_keyboardandgamepad ||
         which == controltype_keyboardandflightstick ||
         which == controltype_keyboardandthrustmaster) &&
        CONTROL_JoysPresent[CONTROL_JoystickPort ? 1 : 0];
    (void)control_type;
    (void)control_timefunc;
    (void)control_ticspersecond;
}

void CONTROL_Shutdown(void)
{
    MOUSE_Shutdown();
    CONTROL_MouseEnabled = false;
    CONTROL_JoystickEnabled = false;
}

void CONTROL_GetInput(ControlInfo *info)
{
    int i;
    int32 mx = 0, my = 0;
    int32 mouse_buttons = 0;

    TSM_Yield();

    CONTROL_ButtonHeldState1 = CONTROL_ButtonState1;
    CONTROL_ButtonHeldState2 = CONTROL_ButtonState2;
    CONTROL_ButtonState1 = CONTROL_ButtonState2 = 0;
    memset(info, 0, sizeof(*info));

    for (i = 0; i < MAXGAMEBUTTONS; ++i)
    {
        boolean pressed = control_key_pressed(&control_keys[i]);
        if (control_keys[i].toggle)
        {
            if (pressed && !control_keys[i].old_pressed)
                control_keys[i].toggle_state = !control_keys[i].toggle_state;
            control_keys[i].old_pressed = pressed;
            control_set_button(i, control_keys[i].toggle_state);
        }
        else
            control_set_button(i, pressed);
    }

    if (CONTROL_MouseEnabled)
    {
        mouse_buttons = MOUSE_GetButtons();
        MOUSE_GetDelta(&mx, &my);

        for (i = 0; i < CONTROL_MOUSE_BUTTONS; ++i)
        {
            if (mouse_buttons & (1 << i))
            {
                int32 fn = control_button[i][0];
                if ((unsigned long)fn < MAXGAMEBUTTONS) control_set_button(fn, true);
            }
        }

        {
            int32 raw[2];
            raw[0] = mx;
            raw[1] = my;
            for (i = 0; i < CONTROL_MOUSE_AXES; ++i)
            {
                int32 scaled = (int32)(((long long)raw[i] * control_mouse_sensitivity * control_axis[i].scale) >> 16);
                int32 fn;
                control_apply_analog(info, control_axis[i].analog, scaled);
                fn = control_axis[i].digital[raw[i] < 0 ? 0 : 1];
                if (raw[i] && (unsigned long)fn < MAXGAMEBUTTONS) control_set_button(fn, true);
            }
        }
    }

    if (CONTROL_JoystickEnabled)
    {
        uint32 raw[CONTROL_JOY_AXES];
        unsigned base = control_joy_axis_base();
        unsigned buttons;
        (void)control_gameport_read(raw);
        buttons = control_gameport_buttons();

        for (i = 0; i < 2; ++i)
        {
            if (buttons & (1u << i))
            {
                int32 fn = control_button[i][0];
                if ((unsigned long)fn < MAXGAMEBUTTONS) control_set_button(fn, true);
            }
        }

        for (i = 0; i < 2; ++i)
        {
            unsigned axis = base + (unsigned)i;
            int32 value = control_normalize_joy(axis, raw[axis]);
            int32 scaled = (int32)(((long long)value * control_axis[i].scale) >> 16);
            int32 fn;
            control_apply_analog(info, control_axis[i].analog, scaled);
            fn = control_axis[i].digital[value < 0 ? 0 : 1];
            if (value && (unsigned long)fn < MAXGAMEBUTTONS) control_set_button(fn, true);
        }
    }
}

void CONTROL_ClearUserInput(UserInput *info)
{
    memset(info, 0, sizeof(*info));
    info->dir = dir_None;
}

void CONTROL_GetUserInput(UserInput *info)
{
    CONTROL_ClearUserInput(info);
}

void CONTROL_WaitRelease(void)
{
    while (MOUSE_GetButtons() || KB_KeyWaiting() ||
           (CONTROL_JoystickEnabled && control_gameport_buttons()))
    {
        KB_FlushKeyboardQueue();
        TSM_Yield();
    }
}

void CONTROL_Ack(void)
{
    CONTROL_WaitRelease();
    while (!MOUSE_GetButtons() && !KB_KeyWaiting() &&
           !(CONTROL_JoystickEnabled && control_gameport_buttons()))
        TSM_Yield();
    CONTROL_WaitRelease();
}

void CONTROL_CenterJoystick(void (*CenterCenter)(void), void (*UpperLeft)(void),
                            void (*LowerRight)(void), void (*CenterThrottle)(void),
                            void (*CenterRudder)(void))
{
    uint32 raw[CONTROL_JOY_AXES];
    unsigned base = control_joy_axis_base();
    unsigned i;

    if (!CONTROL_JoystickEnabled)
        return;

    if (CenterCenter) CenterCenter();
    control_wait_joy_press();
    (void)control_gameport_read(raw);
    for (i = 0; i < 2; ++i)
        control_joy_center[base + i] = raw[base + i];

    if (UpperLeft) UpperLeft();
    control_wait_joy_press();
    (void)control_gameport_read(raw);
    for (i = 0; i < 2; ++i)
        control_joy_min[base + i] = raw[base + i];

    if (LowerRight) LowerRight();
    control_wait_joy_press();
    (void)control_gameport_read(raw);
    for (i = 0; i < 2; ++i)
        control_joy_max[base + i] = raw[base + i];

    /* The plain IBM gameport exposes two calibrated axes per joystick. */
    (void)CenterThrottle;
    (void)CenterRudder;
}

void CONTROL_PrintAxes(void)
{
}
