#include "psxcommon.h"
#include "system.h"

#include "pad.h"

// MOUSE SCPH-1030
// NEGCON - 16 button analog controller NPC101/NPC104(SLPH-00001/SLPH-00069)
// GUN CONTROLLER - gun controller SLPH-00014 from Konami
// STANDARD PAD SCPH-1080, SCPH-1150
// ANALOG JOYSTICK SCPH-1110
// GUNCON - gun controller SLPH-00034 from Namco
// ANALOG CONTROLLER SCPH-1150

#define PAD_TYPE_NONE           0
#define PAD_TYPE_MOUSE          1
#define PAD_TYPE_NEGCON         2
#define PAD_TYPE_GUN            3
#define PAD_TYPE_STANDARD       4
#define PAD_TYPE_ANALOGJOY      5
#define PAD_TYPE_GUNCON         6
#define PAD_TYPE_ANALOGPAD      7

#define PAD_DIGITAL             0
#define PAD_ANALOG              1

static const char   *gamePadText[8] =
{
    "Nothing",
    "Mouse",
    "neGcon (Namco)",
    "Gun (Konami)",
    "Standard Digital",
    "Dual Analog Joystick",
    "Gun (Namco)",
    "Analog Controller"
};

static void PadTypeStandard(void);

typedef struct
{
    void (*type)(void);
    u8      id;
    u8      style;
    u16     buttons;    // 1 = up, 0 = down
    u8      stick[4];
    s8      mouse[2];
    u8      cmd, mode;
    int     rumble[2];
}
PAD;

static PAD          gamePad[2] =
{
    {.type = PadTypeStandard, .id = 0x00, .style = PAD_DIGITAL, .buttons = 0xffff, .stick = {128, 128, 128, 128}, .mouse = {0x00, 0x00}, .cmd = 0x00, .mode = 0x00, .rumble = {0x00, 0x00}},
    {.type = PadTypeStandard, .id = 0x00, .style = PAD_DIGITAL, .buttons = 0xffff, .stick = {128, 128, 128, 128}, .mouse = {0x00, 0x00}, .cmd = 0x00, .mode = 0x00, .rumble = {0x00, 0x00}}
};

static const int    padId[8] = {0x00, 0x12, 0x23, 0x01, 0x41, 0x53, 0x02, 0x73};

static u8           padInput[8] = {0x00, 0x5a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static u8           padConfig[8] = {0xf3, 0x5a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static u8           *padBuffer = padConfig;
static int          bufferPos = 0;

static int          curPad = 0;

static const int    axisDir[4] = {7, 4, 5, 6};

int                 padType[2] = {PAD_TYPE_NONE, PAD_TYPE_NONE};

static void PadButton(int pad, const int button, int state)
{
    gamePad[pad].buttons |= (1 << button);
    gamePad[pad].buttons &= ~(state << button);
}

void PAD_Button(int pad, int button, int state)
{
    PadButton(pad, button, state);
}

void PAD_Axis(int pad, int axis, short value)
{
    short   state[2];

    value /= 256;
    state[0] = value < 0 ? 1 : 0;
    state[1] = value > 0 ? 1 : 0;
    PadButton(pad, axisDir[axis], state[0]);
    PadButton(pad, axisDir[axis ^ 2], state[1]);

    if (value > -20 && value < 20) // 64?
    {
        value = 0;
    }
    gamePad[pad].stick[axis] = value + 128;
}

void PAD_Motion(int pad, s8 x, s8 y)
{
    gamePad[pad].mouse[0] = x;
    gamePad[pad].mouse[1] = y;
}

static void PadTypeMouse()
{
    padInput[4] = gamePad[curPad].mouse[0]; // x
    padInput[5] = gamePad[curPad].mouse[1]; // y
    gamePad[curPad].mouse[0] = 0;
    gamePad[curPad].mouse[1] = 0;
}

static void PadTypeStandard()
{
}

static void PadTypeAnalog()
{
    padInput[4] = gamePad[curPad].stick[3]; // right x
    padInput[5] = gamePad[curPad].stick[2]; // right y
    padInput[6] = gamePad[curPad].stick[0]; // left x
    padInput[7] = gamePad[curPad].stick[1]; // left y
}

u8 PAD_StartPoll(int pad)
{
    curPad = pad;

    bufferPos = 0;

    padConfig[2] = 0x00;
    padConfig[3] = 0x00;
    padConfig[4] = 0x00;
    padConfig[5] = 0x00;
    padConfig[6] = 0x00;
    padConfig[7] = 0x00;

    padInput[0] = gamePad[pad].id;
    padInput[2] = gamePad[pad].buttons & 0xff;
    padInput[3] = gamePad[pad].buttons >> 8;

    gamePad[pad].type();

    return 0xff;
}

u8 PAD_Poll(u8 data)
{
    if (bufferPos == 0)
    {
        gamePad[curPad].cmd = data;

        if (gamePad[curPad].cmd == 'B')
        {
            padBuffer = padInput;
            if (gamePad[curPad].mode)
            {
                padInput[0] = 0xf3;
            }
        }
        else if (gamePad[curPad].cmd == 'C')
        {
            if (gamePad[curPad].mode)
            {
                padBuffer = padConfig;
            }
            else
            {
                padBuffer = padInput;
            }
        }
        else if (gamePad[curPad].cmd == 'E')
        {
            padBuffer = padConfig;
            padConfig[2] = 0x01;
            padConfig[3] = 0x02;
            padConfig[4] = gamePad[curPad].style; // digital/analog
            padConfig[5] = 0x02;
            padConfig[6] = 0x01;
        }
        else if (gamePad[curPad].cmd == 'F')
        {
            padBuffer = padConfig;
        }
        else if (gamePad[curPad].cmd == 'G')
        {
            padBuffer = padConfig;
            padConfig[4] = 0x02;
            padConfig[6] = 0x01;
        }
        else if (gamePad[curPad].cmd == 'L')
        {
            padBuffer = padConfig;
        }
        else if (gamePad[curPad].cmd == 'M') // rumble
        {
            padBuffer = padConfig;
            padConfig[2] = 0xff;
            padConfig[3] = 0xff;
            padConfig[4] = 0xff;
            padConfig[5] = 0xff;
            padConfig[6] = 0xff;
            padConfig[7] = 0xff;
        }
    }
    else if (bufferPos == 2)
    {
        if (gamePad[curPad].cmd == 'C')
        {
            gamePad[curPad].mode = data; // normal/config
        }
        else if (gamePad[curPad].cmd == 'F')
        {
            if (data == 0x00)
            {
                padConfig[4] = 0x01;
                padConfig[5] = 0x02;
                padConfig[6] = 0x00;
                padConfig[7] = 0x0a;
            }
            else if (data == 0x01)
            {
                padConfig[4] = 0x01;
                padConfig[5] = 0x01;
                padConfig[6] = 0x01;
                padConfig[7] = 0x14;
            }
        }
        else if (gamePad[curPad].cmd == 'L')
        {
            if (data == 0x00)
            {
                padConfig[5] = 0x04;
            }
            else if (data == 0x01)
            {
                padConfig[5] = 0x07;
            }
        }
    }
    // this is necessary as only 8 bytes are ever read
    // emulator issue or game demand?
    else if (bufferPos == 8)
    {
        return 0x00;
    }
#if 0 // needs testing with dual shock controller
    if (bufferPos >= 2 && bufferPos < 8)
    {
        if (gamePad[curPad].cmd == 'M')
        {
            if (bufferPos == gamePad[curPad].rumble[data & 1])
            {
                padConfig[bufferPos] = data & 1;
            }

            gamePad[curPad].rumble[data & 1] = bufferPos;
        }
    }
#endif
    return padBuffer[bufferPos++];
}

void PAD_Init(int pad)
{
    switch (padType[pad])
    {
      case PAD_TYPE_MOUSE:
        gamePad[pad].type = PadTypeMouse;
        break;

      case PAD_TYPE_NEGCON:
      case PAD_TYPE_ANALOGPAD:
      case PAD_TYPE_ANALOGJOY:
        gamePad[pad].type = PadTypeAnalog;
        gamePad[pad].style = PAD_ANALOG;
        break;

      case PAD_TYPE_STANDARD:
        gamePad[pad].type = PadTypeStandard;
        break;

      default:
        return;
    }

    gamePad[pad].id = padId[padType[pad]];

    SysPrintf("Game pad %i connected: %s\n", pad + 1, gamePadText[padType[pad]]);
}
